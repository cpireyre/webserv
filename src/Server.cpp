#include "Server.hpp"
#include "Queue.hpp"

volatile sig_atomic_t g_ServerShoudClose = false;
static void	initEndpoint(int sockfd, std::string host, std::string port, Endpoint *endpoint);
static bool endpointAlreadyBound(Endpoint *endpoints, int count_to_check, std::string IP, std::string port);

int	start_servers(std::vector<Configuration> servers, Endpoint *endpoints, int endpoints_count_max, int *endpoints_count)
{
	assert(servers.size() > 0);
	assert(endpoints != nullptr);
	*endpoints_count = 0;
	for (int i = 0; i < endpoints_count_max; i++)
	{
		const std::string host = servers[i].getHost();
		const std::string port = servers[i].getPort();
		if (endpointAlreadyBound(endpoints, i, host, port))
			continue;
		int	sockfd = make_server_socket(host.data(), port.data());
		if (sockfd <= 0)
			return (-1);
		initEndpoint(sockfd, host, port, &endpoints[*endpoints_count]);
		Logger::debug("Opened socket IP: %s, port: %s", endpoints[*endpoints_count].IP, endpoints[*endpoints_count].port);
		assert(endpoints[*endpoints_count].state == CONNECTION_ACTUALLY_A_SERVER);
		*endpoints_count += 1;
	}
	assert(*endpoints_count > 0);
	return (0);
}

// We run this after creating the server socket,
// so we already know the endpoint is valid.
static void	initEndpoint(int sockfd, std::string host, std::string port, Endpoint *endpoint)
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

static bool endpointAlreadyBound(Endpoint *endpoints, int count_to_check, std::string IP, std::string port)
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

void	cleanup(Endpoint *endpoints, int endpoints_count, int qfd)
{
	if (endpoints)
	{
		for (int i = 0; i < endpoints_count; i++)
		{
			Logger::debug("Closing socket %s:%s", endpoints[i].IP, endpoints[i].port);
			close(endpoints[i].sockfd);
		}
		Logger::debug("Deleting endpoints array");
	}
	close(qfd);
}

Endpoint	*connectNewClient(Endpoint *endpoints, const Endpoint *server)
{
	assert(endpoints != nullptr);
	assert(server->sockfd > 0);

	int i = 0;
	while (i < MAXCONNS && endpoints[i].state != CONNECTION_DISCONNECTED)
		i++;
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
	endpoints[i].error = 0;
	endpoints[i].state = CONNECTION_RECV_HEADER;
	endpoints[i].sockfd = clientSocket;
	endpoints[i].handler.setClientSocket(clientSocket);
	endpoints[i].handler.setIP(server->IP);
	endpoints[i].handler.setPORT(server->port);
	endpoints[i].lastHeardFrom_ms = now_ms();
	Logger::debug("Connected client, socket: %d", clientSocket);

	return &endpoints[i];
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
	error = start_servers(serverMap, endpoints, server_socket_count, &endpoints_count);
	int i = 0;
	if (error)
		goto cleanup;

	while (i < endpoints_count)
	{
		assert(endpoints[i].state == CONNECTION_ACTUALLY_A_SERVER);
		assert(endpoints[i].sockfd > 0);
		error = queue_add_fd(qfd, endpoints[i].sockfd, QUEUE_EVENT_READ, &endpoints[i]);
		if (error)
		{
			Logger::warn("Error registering server socket events, "
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
		int nready = queue_wait(qfd, events, QUEUE_MAX_EVENTS);
		if (nready < 0 && !g_ServerShoudClose)
		{
			Logger::warn("Error getting events from kernel");
			error = 1;
			break;
		}
		/* for (int i = 0; i < MAXCONNS; i++) */
		/* { */
		/* 	if (endpoints[i].alive == false || endpoints[i].kind != ENDPOINT_CLIENT) */
		/* 		continue; */
		/* 	assert(endpoints[i].alive == true); */
		/* 	if (now_ms() - endpoints[i].lastHeardFrom_ms > CLIENT_TIMEOUT_MS) */
		/* 	{ */
		/* 		endpoints[i].state = CONNECTION_TIMED_OUT; */
		/* 		Logger::debug("Timing out %d", endpoints[i].sockfd); */
		/* 	} */
		/* } */
		for (int event_id = 0; event_id < nready; event_id++)
		{
			Endpoint *conn = (Endpoint *)queue_event_get_data(&events[event_id]);
			assert(conn->sockfd > 0);
			assert(conn->state != CONNECTION_DISCONNECTED);
			conn->lastHeardFrom_ms = now_ms();
			switch (conn->state)
			{
				case CONNECTION_TIMED_OUT:
					conn->error = 408;
					conn->state = CONNECTION_SEND_RESPONSE;
					queue_mod_fd(qfd, conn->handler.getClientSocket(), QUEUE_EVENT_WRITE, conn);
					break;
				case CONNECTION_RECV_HEADER: 
					receiveHeader(conn, qfd);
					break;
				case CONNECTION_RECV_BODY:
					receiveBody(conn, qfd);
					break;
				case CONNECTION_SEND_RESPONSE:
					if (conn->error != 0)
					{
						std::string errorResponse = conn->handler.createHttpErrorResponse(conn->error);
						send(conn->sockfd, errorResponse.c_str(), errorResponse.size(), 0);
						conn->error = 0;
						disconnectClient(conn, qfd);
					}
					else
					{
						conn->handler.handleRequest();
						queue_mod_fd(qfd, conn->handler.getClientSocket(), QUEUE_EVENT_READ, conn);
						conn->state = CONNECTION_RECV_HEADER;
						conn->handler.resetObject();
					}
					break;
				case CONNECTION_ACTUALLY_A_SERVER:
					{
						Endpoint *newClient = connectNewClient(endpoints, conn);
						if (newClient == nullptr)
							continue;
						queue_add_fd(qfd, newClient->handler.getClientSocket(), QUEUE_EVENT_READ, newClient);
						assert(newClient->handler.getClientSocket() != conn->handler.getClientSocket());
					}
					break;
				case CONNECTION_DISCONNECTED:
					assert(false); /* Unreachable. Shouldn't happen */
					break;
			}
		}
	}


cleanup:
	cleanup(endpoints, endpoints_count, qfd);
	if (error)
		return (1);
	return (0);
}
