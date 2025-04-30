#include "Server.hpp"
#include "Queue.hpp"

volatile sig_atomic_t g_ServerShoudClose = false;

static int	start_servers(const std::vector<Configuration>,Endpoint*,int,int*);
static Endpoint	*connectNewClient(Endpoint *, const Endpoint *, int, int *);
static void	initEndpoint(int, std::string, std::string, Endpoint *);
static bool endpointAlreadyBound(Endpoint *, int, std::string, std::string);
static bool	isTimedOut(Endpoint *, int);
static void sigcleanup(int);
static void handlesignals(void(*)(int));

int	run(const std::vector<Configuration> config)
{
	assert(!config.empty());
	handlesignals(sigcleanup);

	int	error = 1;

	int qfd = queue_create();
	if (qfd < 0) return (1);

	int	servers_num = 0;
	Endpoint	endpoints[MAXCONNS];
	for (int n = 0; n < MAXCONNS; n++)
		endpoints[n].state = C_DISCONNECTED;

	error = start_servers(config, endpoints, config.size(), &servers_num);
	int max_client_id = servers_num;
	if (error) goto cleanup;

	/* Register all server sockets for read events */
	for (Endpoint *conn = endpoints; conn < endpoints + servers_num; conn++) {
		assert(conn->state == C_ACTUALLY_A_SERVER);
		error = queue_add_fd(qfd, conn->sockfd, READABLE, conn);
		if (error) {
			logError("Error registering server socket events");
			goto cleanup;
		}
	}

	queue_event events[QUEUE_MAX_EVENTS];
	bzero(events, sizeof(events));

	while (!g_ServerShoudClose) {
		assert(g_ServerShoudClose == false);

		for (Endpoint *conn = endpoints; conn <= endpoints + max_client_id; conn++) {
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

			if (queue_event_is_error(&events[id])) disconnectClient(conn, qfd);
			queue_event_type event_type = queue_event_get_type(&events[id]);

				if (conn->state == C_ACTUALLY_A_SERVER)
				{
					assert(event_type == READABLE);
					Endpoint *client =
						connectNewClient(endpoints, conn, qfd, &max_client_id);
					if (client == nullptr) break;
					queue_add_fd(qfd, client->sockfd, READABLE, client);
					assert(client->sockfd == client->handler.getClientSocket());
					assert(client->sockfd != conn->handler.getClientSocket());
				}

			switch (conn->state) {
				case C_TIMED_OUT:
					conn->state = C_SEND_RESPONSE;
					break;

				case C_RECV_HEADER: assert(event_type == READABLE);
					receiveHeader(conn, qfd);
					conn->last_heard_from_ms = now_ms();
					break;

				case C_RECV_BODY: assert(event_type == READABLE);
					receiveBody(conn, qfd);
					break;
				case C_SEND_RESPONSE:
					assert(event_type == WRITABLE);
					if (conn->handler.getErrorCode() != 0)
					{
						/* depending on handler.getfileServ() we already have the whole response
						 * or we are just sending everyhting but body */
						logDebug("Error with %d", conn->sockfd);
						std::string response = conn->handler
							.createErrorResponse(conn->handler.getErrorCode());
						send(conn->sockfd, response.c_str(), response.size(), 0);
						if (conn->handler.getFileServ()) //update time stamp?
							conn->state = C_FILE_SERVE;
						else
							disconnectClient(conn, qfd);
					}
					else
					{
						conn->handler.handleRequest();
						send(conn->sockfd, conn->handler.getResponse().c_str(), conn->handler.getResponse().size(), 0);
						if (conn->handler.getFileServ()) {
							conn->state = C_FILE_SERVE;
							break;
						}
						watch(qfd, conn, READABLE);
						conn->state = C_RECV_HEADER;
						conn->handler.resetObject();
						conn->began_sending_header_ms = now_ms();
					}
					break;
				case C_FILE_SERVE: assert(event_type == WRITABLE);
				  switch(conn->handler.serveFile()) {
				   case S_Done:
					   watch(qfd, conn, READABLE);
					   conn->state = C_RECV_HEADER;
					   if (conn->handler.getErrorCode() != 0) disconnectClient(conn, qfd);
					   conn->handler.resetObject();
					   break;

				   case S_Error: disconnectClient(conn, qfd);
						 break;

				   default:
						 break;
				  }
				  break;

				default: break;
			}
		}
	}

cleanup:
	for (Endpoint *conn = endpoints; conn <= endpoints + max_client_id; conn++) {
		if (conn->state != C_DISCONNECTED) {
			logDebug("Closing socket %s:%s (%d)", conn->IP, conn->port, conn->sockfd);
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
		assert(endpoints[*count].state == C_ACTUALLY_A_SERVER);
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
	while (i < MAXCONNS && endpoints[i].state != C_DISCONNECTED)
		i++;
	if (i == MAXCONNS) /* Uh oh, we need to kick someone out */
	{
		i = 0;
		while (i < MAXCONNS)
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
	logDebug("Connected client, socket: %d", clientSocket);

	if (i > *max_client_id)
		*max_client_id = i;
	return &endpoints[i];
}

static bool	isTimedOut(Endpoint *conn, int qfd)
{
	assert(conn->last_heard_from_ms != 0);

	uint64_t idle_duration_ms = now_ms() - conn->last_heard_from_ms;

	if (conn->handler.getErrorCode() != 408
			&&  idle_duration_ms > CLIENT_TIMEOUT_THRESHOLD_MS) {
		conn->handler.setErrorCode(408);
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
	endpoint->state = C_ACTUALLY_A_SERVER;
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

static void sigcleanup(int sig)
{
	(void)sig;
	g_ServerShoudClose = true;
}

static void handlesignals(void(*hdl)(int))
{
	struct sigaction sa;

	signal(SIGPIPE, SIG_IGN);
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = hdl;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, nullptr);
	sigaction(SIGHUP, &sa, nullptr);
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGQUIT, &sa, nullptr);
}

int		watch(int qfd, Endpoint *conn, enum queue_event_type t)
{
	return (queue_mod_fd(qfd, conn->sockfd, t, conn));
}
