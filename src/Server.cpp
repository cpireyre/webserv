#include "Server.hpp"
#include "Queue.hpp"

extern sig_atomic_t g_ShouldStop;

static int	start_servers(const std::vector<Configuration>,Endpoint*,int,int*);
static Endpoint	*connectNewClient(Endpoint *, const Endpoint *, int, int *);
static void	initEndpoint(int, std::string, std::string, Endpoint *);
static bool endpointAlreadyBound(Endpoint *, int, std::string, std::string);
static bool	isTimedOut(Endpoint *, int);

int	run(const std::vector<Configuration> config)
{
	assert(!config.empty());
  if (config.size() > MAX_SERVERS)
  {
    std::cerr << "Error: webserv: Too many virtual hosts\n";
    return 1;
  }

	int	error = 1;

	int qfd = queue_create();
	if (qfd < 0) return (1);

	int	servers_num = 0;
	Endpoint	endpoints[MAXCONNS];
	for (int n = 0; n < MAXCONNS; n++)
  {
    endpoints[n].state = C_DISCONNECTED;
    endpoints[n].kind = None;
  }

	error = start_servers(config, endpoints, config.size(), &servers_num);
	int max_client_id = servers_num;
	if (error) goto cleanup;

	/* Register all server sockets for read events */
	for (Endpoint *conn = endpoints; conn < endpoints + servers_num; conn++) {
		assert(conn->kind == Server);
		error = queue_add_fd(qfd, conn->sockfd, READABLE, conn);
		if (error) {
			logError("Error registering server socket events");
			goto cleanup;
		}
	}

	queue_event events[QUEUE_MAX_EVENTS];
	bzero(events, sizeof(events));

  try {
	while (!g_ShouldStop) {
		assert(g_ShouldStop == false);
		for (Endpoint *conn = endpoints; conn <= endpoints + max_client_id; conn++) {
      if (conn->state == C_MARKED_FOR_DISCONNECTION) {
        disconnectClient(conn, qfd);
        continue;
      }
			if (isLiveClient(conn) && isTimedOut(conn, qfd)) {
				conn->state = C_TIMED_OUT;
				watch(qfd, conn, WRITABLE);
			}
		}

		int nready = queue_wait(qfd, events, QUEUE_MAX_EVENTS);
		if ((error = nready < 0) != 0) break;

		for (int id = 0; id < nready; id++) {
			Endpoint *conn = (Endpoint*)queue_event_get_data(&events[id]);
			assert(conn->sockfd > 0);

			if (queue_event_is_error(&events[id])) conn->state = C_MARKED_FOR_DISCONNECTION;

			queue_event_type event_type = queue_event_get_type(&events[id]);

			switch (conn->kind)
			{
				case Client: serveConnection(conn, qfd, event_type);
					break;

				case Server: assert(event_type == READABLE);
				 {
					 Endpoint *client = connectNewClient(endpoints, conn, qfd, &max_client_id);
					 if (client == nullptr) break;
					 queue_add_fd(qfd, client->sockfd, READABLE, client);
					 assert(client->sockfd == client->handler.getClientSocket());
					 assert(client->sockfd != conn->handler.getClientSocket());
				 }
					break;

        case None: assert(false); /* Unreachable */
          break;
			}
		}
	}
  }
  catch (...) { error = EXIT_FAILURE; }

cleanup:
  logDebug("⏼ Cleaning up...");
	for (Endpoint *conn = endpoints; conn <= endpoints + max_client_id; conn++) {
    if (conn->kind == None) continue;
    if (conn->kind == Client) { conn->cgiHandler.CgiResetObject(); }
		if (conn->kind == Server || conn->state != C_DISCONNECTED ) {
      string kind = conn->kind == Server ? "server" : "client";
			logDebug("Closing %s socket %s:%s (%d)", kind.c_str(), conn->IP, conn->port, conn->sockfd);
			assert(conn->sockfd > 0);
			close(conn->sockfd);
		}
	}
	close(qfd);
	if (error)
		return (1);
	return (0);
}

static int	start_servers(const std::vector<Configuration> servers,
		Endpoint *endpoints, int count_max, int *count)
{
	assert(servers.size() > 0);
	assert(endpoints != nullptr);
	*count = 0;
	for (int i = 0; i < count_max; i++)
	{
		const std::string host = servers[i].getHost();
		const std::string port = servers[i].getPort();
		if (endpointAlreadyBound(endpoints, i, host, port))
			continue;
		int	sockfd = make_server_socket(host.data(), port.data());
		if (sockfd <= 0)
			return (-1);
		initEndpoint(sockfd, host, port, &endpoints[*count]);
		servers[i].printCompact();
		assert(endpoints[*count].kind == Server);
		*count += 1;
	}
	assert(*count > 0);
	return (0);
}

static Endpoint	*connectNewClient(Endpoint *endpoints,
		const Endpoint *server, int qfd, int *max_client_id)
{
	assert(endpoints != nullptr);
	assert(server->sockfd > 0);

	int i = 0;
	while (endpoints[i].kind == Server && i < MAXCONNS)
		i++;
	while (i < MAXCONNS && endpoints[i].state != C_DISCONNECTED)
		i++;
	if (i == MAXCONNS) /* Uh oh, we need to kick someone out */
	{
		i = 0;
		while (i < MAXCONNS)
		{
			if (endpoints[i].kind == Client)
			{
				Endpoint *conn = &endpoints[i];
				uint64_t recv_header_duration_ms =
					now_ms() - conn->began_sending_header_ms;
				bool too_slow = recv_header_duration_ms > RECV_HEADER_TIMEOUT_MS;
				if (conn->state == C_RECV_HEADER && too_slow)
				{
					disconnectClient(conn, qfd);
					break;
				}
			}
			i++;
		}
	}
	if (i == MAXCONNS) /* We definitely can't make a new connection right now */
		return nullptr;
	assert(i < MAXCONNS);
	struct sockaddr client_addr;
	socklen_t		client_addr_len = sizeof(client_addr);
	memset(&client_addr, 0, client_addr_len);
	int clientSocket = accept(server->sockfd, &client_addr, &client_addr_len);
	if (clientSocket < 0 || socket_set_nonblocking(clientSocket) < 0)
	{
		if (clientSocket > 0)
			close(clientSocket);
		perror("client accept");
		return nullptr;
	}
	endpoints[i].state = C_RECV_HEADER;
	endpoints[i].sockfd = clientSocket;
	memcpy(endpoints[i].IP, server->IP, INET6_ADDRSTRLEN);
	memcpy(endpoints[i].port, server->port, PORT_STRLEN);
	endpoints[i].handler.setClientSocket(clientSocket);
	endpoints[i].handler.setIP(server->IP);
	endpoints[i].handler.setPORT(server->port);
	endpoints[i].began_sending_header_ms = now_ms();
	endpoints[i].last_heard_from_ms = now_ms();
	endpoints[i].kind = Client;
	logDebug("Connected client, socket: %d", clientSocket);

	if (i > *max_client_id)
		*max_client_id = i;
	return &endpoints[i];
}

static bool	isTimedOut(Endpoint *conn, int qfd)
{
	assert(conn->last_heard_from_ms != 0);

	uint64_t idle_duration_ms = now_ms() - conn->last_heard_from_ms;
  /* if (idle_duration_ms > 10 * 1000) */
  /*   logDebug("%d idle for %zums, timing out soon", conn->sockfd, idle_duration_ms); */

	if (conn->handler.getErrorCode() != 408
      && conn->handler.getErrorCode() != 500
			&&  idle_duration_ms > CLIENT_TIMEOUT_THRESHOLD_MS) {
		conn->handler.setErrorCode(
        conn->cgiHandler.cgiPid == 0 ? 408 : 500);
		logDebug("Soft timeout: %d", conn->sockfd);
		return (true);
	}

	if (conn->state == C_TIMED_OUT
			&&  idle_duration_ms > 3 * CLIENT_TIMEOUT_THRESHOLD_MS) {
		logDebug("Hard timeout: %d", conn->sockfd);
		disconnectClient(conn, qfd);
	}

	return (false);
}

static bool endpointAlreadyBound(Endpoint *endpoints, int count_to_check,
		std::string IP, std::string port)
{
	for (int i = 0; i < count_to_check; i++)
	{
		if (strcmp(endpoints[i].IP, IP.data()))
			continue;
		if (strcmp(endpoints[i].port, port.data()))
			continue;
		return (true);
	}
	return (false);
}

// We run this after creating the server socket,
// so we already know the endpoint is valid.
static void	initEndpoint(int sockfd, std::string host, std::string port,
		Endpoint *endpoint)
{
	assert(endpoint != nullptr);
	endpoint->sockfd = sockfd;
	endpoint->handler = HttpConnectionHandler();
	endpoint->kind = Server;
	int i = 0;
	for (char c : host)
		endpoint->IP[i++] = c;
	endpoint->IP[i] = '\0';
	i = 0;
	for (char c : port)
		endpoint->port[i++] = c;
	endpoint->port[i] = '\0';
	assert(endpoint->sockfd > 0);
	assert(strlen(endpoint->port) >= 1);
	assert(strlen(endpoint->IP) >= 7);
	assert(strlen(endpoint->IP) <= 16);
}

int		watch(int qfd, Endpoint *conn, enum queue_event_type t)
{
	return (queue_mod_fd(qfd, conn->sockfd, t, conn));
}
