#include "Server.hpp"
#include "Queue.hpp"

volatile sig_atomic_t g_ServerShoudClose = false;
int max_client_id = 0;

static int	start_servers(std::vector<Configuration>, Endpoint *, int, int *);
static Endpoint	*connectNewClient(Endpoint *, const Endpoint *, int);
static void	initEndpoint(int, std::string, std::string, Endpoint *);
static bool endpointAlreadyBound(Endpoint *, int, std::string, std::string);
static void	timeoutInactiveClients(Endpoint *, int);
static void	cleanup(Endpoint *, int);
static void sigcleanup(int);
static void handlesignals(void(*)(int));

int	run(std::vector<Configuration> serverMap)
{
	assert(!serverMap.empty());

	int	error = 1;

	int qfd = queue_create();
	if (qfd < 0)
		return (1);
	handlesignals(sigcleanup);

	int	endpoints_count = 0;
	const int	server_socket_count = serverMap.size();
	Endpoint	endpoints[MAXCONNS];
	for (int n = 0; n < MAXCONNS; n++)
		endpoints[n].state = CONNECTION_DISCONNECTED;
	error = start_servers(serverMap, endpoints,
			server_socket_count, &endpoints_count);
	int i = 0;
	if (error)
		goto cleanup;

	while (i < endpoints_count)
	{
		assert(endpoints[i].state == CONNECTION_ACTUALLY_A_SERVER);
		assert(endpoints[i].sockfd > 0);
		error = queue_add_fd(qfd, endpoints[i].sockfd,
				QUEUE_EVENT_READ, &endpoints[i]);
		if (error)
		{
			logError("Error registering server socket events, "
					"execution cannot proceed");
			goto cleanup;
		}
		i++;
	}

	queue_event events[QUEUE_MAX_EVENTS];
	memset(events, 0, sizeof(events));
	while (!g_ServerShoudClose)
	{
		assert(g_ServerShoudClose == false);
		timeoutInactiveClients(endpoints, qfd);
		int nready = queue_wait(qfd, events, QUEUE_MAX_EVENTS);
		if (nready < 0 && !g_ServerShoudClose)
		{
			logError("Error getting events from kernel");
			error = 1;
			break;
		}
		for (int id = 0; id < nready; id++)
		{
			Endpoint *conn = (Endpoint*)queue_event_get_data(&events[id]);
			assert(conn->sockfd > 0);
			assert(conn->state != CONNECTION_DISCONNECTED);
			if (queue_event_is_error(&events[id]))
				disconnectClient(conn, qfd);
			queue_event_type event_type = queue_event_get_type(&events[id]);
			switch (conn->state)
			{
				case CONNECTION_TIMED_OUT:
					conn->handler.setErrorCode(408);
					conn->state = CONNECTION_SEND_RESPONSE;
					queue_mod_fd(qfd, conn->handler.getClientSocket(),
							QUEUE_EVENT_WRITE, conn);
					break;
				case CONNECTION_RECV_HEADER: 
					assert(event_type == QUEUE_EVENT_READ);
					receiveHeader(conn, qfd);
					conn->last_heard_from_ms = now_ms();
					break;
				case CONNECTION_RECV_BODY:
					assert(event_type == QUEUE_EVENT_READ);
					receiveBody(conn, qfd);
					conn->last_heard_from_ms = now_ms();
					break;
				case CONNECTION_SEND_RESPONSE:
					assert(event_type == QUEUE_EVENT_WRITE);
					if (conn->handler.getErrorCode() != 0)
					{
						/* This assumes we can send the whole error
						 * response in one send() call */
						logDebug("Error with %d", conn->sockfd);
						std::string response = conn->handler
							.createHttpErrorResponse(conn->handler.getErrorCode());
						send(conn->sockfd, response.c_str(), response.size(), 0);
						disconnectClient(conn, qfd);
					}
					else
					{
						conn->handler.handleRequest();
						queue_mod_fd(qfd, conn->handler.getClientSocket(),
								QUEUE_EVENT_READ, conn);
						conn->state = CONNECTION_RECV_HEADER;
						conn->handler.resetObject();
						conn->began_sending_header_ms = now_ms();
					}
					break;
				case CONNECTION_ACTUALLY_A_SERVER:
					assert(event_type == QUEUE_EVENT_READ);
					{
						Endpoint *client =
							connectNewClient(endpoints, conn, qfd);
						if (client == nullptr)
							continue;
						queue_add_fd(qfd, client->handler.getClientSocket(),
								QUEUE_EVENT_READ, client);
						assert(client->sockfd ==
								client->handler.getClientSocket());
						assert(client->sockfd !=
								conn->handler.getClientSocket());
					}
					break;
				case CONNECTION_DISCONNECTED:
					continue;
			}
		}
	}

cleanup:
	cleanup(endpoints, qfd);
	if (error)
		return (1);
	return (0);
}

static int	start_servers(std::vector<Configuration> servers,
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
		logDebug("Opened socket IP: %s, port: %s",
				endpoints[*count].IP,
				endpoints[*count].port);
		assert(endpoints[*count].state == CONNECTION_ACTUALLY_A_SERVER);
		*count += 1;
	}
	assert(*count > 0);
	return (0);
}

static Endpoint	*connectNewClient(Endpoint *endpoints,
		const Endpoint *server, int qfd)
{
	assert(endpoints != nullptr);
	assert(server->sockfd > 0);

	int i = 0;
	while (i < MAXCONNS && endpoints[i].state != CONNECTION_DISCONNECTED)
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
			if (conn->state == CONNECTION_RECV_HEADER && too_slow)
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
	endpoints[i].state = CONNECTION_RECV_HEADER;
	endpoints[i].sockfd = clientSocket;
    memcpy(endpoints[i].IP, server->IP, INET6_ADDRSTRLEN);
    memcpy(endpoints[i].port, server->port, PORT_STRLEN);
	endpoints[i].handler.setClientSocket(clientSocket);
	endpoints[i].handler.setIP(server->IP);
	endpoints[i].handler.setPORT(server->port);
	endpoints[i].began_sending_header_ms = now_ms();
	endpoints[i].last_heard_from_ms = now_ms();
	logDebug("Connected client, socket: %d", clientSocket);

	if (i > max_client_id)
		max_client_id = i;
	return &endpoints[i];
}

static void	timeoutInactiveClients(Endpoint *conns, int qfd)
{
	for (int i = 0; i <= max_client_id; i++)
	{
		uint64_t idle_duration_ms = now_ms() - conns[i].last_heard_from_ms;
		switch (conns[i].state)
		{
			case CONNECTION_ACTUALLY_A_SERVER:
				continue;
			case CONNECTION_DISCONNECTED:
				continue;
			case CONNECTION_TIMED_OUT:
				assert(conns[i].last_heard_from_ms != 0);
				// Hard timeout: forcibly disconnect
				if (idle_duration_ms > 3 * CLIENT_TIMEOUT_THRESHOLD_MS)
				{
					logDebug("Hard timeout: %d", conns[i].sockfd);
					disconnectClient(&conns[i], qfd);
				}
				continue;
			default:
				assert(conns[i].last_heard_from_ms != 0);
				if (conns[i].handler.getErrorCode() == 408) // Already marked for timeout
					continue;
				// Soft timeout: mark client for 408
				if (idle_duration_ms > CLIENT_TIMEOUT_THRESHOLD_MS)
				{
					logDebug("Soft timeout: %d", conns[i].sockfd);
					conns[i].state = CONNECTION_TIMED_OUT;
				}
		}
	}
}

static void	cleanup(Endpoint *endpoints, int qfd)
{
	if (endpoints)
	{
		for (int i = 0; i <= max_client_id; i++)
		{
			if (endpoints[i].state != CONNECTION_DISCONNECTED)
			{
				assert(endpoints[i].sockfd > 0);
				logDebug("Closing socket %s:%s (%d)",
						endpoints[i].IP,
						endpoints[i].port,
						endpoints[i].sockfd);
				close(endpoints[i].sockfd);
			}
		}
		logDebug("Deleting endpoints array");
	}
	close(qfd);
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
	endpoint->state = CONNECTION_ACTUALLY_A_SERVER;
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
