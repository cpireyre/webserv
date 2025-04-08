#include "Server.hpp"
#include "Queue.hpp"

volatile sig_atomic_t g_ServerShoudClose = false;
static void	initEndpoint(int sockfd, std::string host, std::string port, Endpoint_t *endpoint);
static bool endpointAlreadyBound(Endpoint_t *endpoints, int count_to_check, std::string IP, std::string port);

int	start_servers(std::vector<Configuration> servers, Endpoint_t *endpoints, int endpoints_count_max, int *endpoints_count)
{
	assert(servers.size() > 0);
	assert(endpoints != NULL);
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
		endpoints[*endpoints_count].type = ENDPOINT_SERVER;
		*endpoints_count += 1;
	}
	assert(*endpoints_count > 0);
	return (0);
}

// We run this after creating the server socket,
// so we already know the endpoint is valid.
static void	initEndpoint(int sockfd, std::string host, std::string port, Endpoint_t *endpoint)
{
	assert(endpoint != NULL);
	memset(endpoint, 0, sizeof(*endpoint));
	int i = 0;
	for (char c : host)
		endpoint->IP[i++] = c;
	endpoint->IP[i] = '\0';
	i = 0;
	for (char c : port)
		endpoint->port[i++] = c;
	endpoint->port[i] = '\0';
	endpoint->sockfd = sockfd;
	assert(endpoint->sockfd > 0);
	assert(strlen(endpoint->port) >= 1);
	assert(strlen(endpoint->IP) >= 7);
	assert(strlen(endpoint->IP) <= 16);
}

static bool endpointAlreadyBound(Endpoint_t *endpoints, int count_to_check, std::string IP, std::string port)
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

void	cleanup(Endpoint_t *endpoints, int endpoints_count, int qfd)
{
	if (endpoints)
	{
		for (int i = 0; i < endpoints_count; i++)
		{
			Logger::debug("Closing socket %s:%s", endpoints[i].IP, endpoints[i].port);
			close(endpoints[i].sockfd);
		}
		Logger::debug("Deleting endpoints array");
		delete[] endpoints;
	}
	close(qfd);
}

Connection	*connectNewClient(Connection *conns, const Endpoint_t *endp)
{
	assert(conns != NULL);
	assert(endp->sockfd > 0);
	struct sockaddr client_addr;
	socklen_t		client_addr_len = sizeof(client_addr);
	memset(&client_addr, 0, client_addr_len);
	int clientSocket = accept(endp->sockfd, &client_addr, &client_addr_len);
	if (clientSocket < 0)
	{
		perror(NULL);
		return NULL;
	}
	assert(clientSocket > 0); //TODO(colin): Error manage here
	int i = 0;
	while (conns[i].alive == true)
		i++;
	assert(i == 0);
	conns[i].alive = true;
	conns[i].endpoint.sockfd = clientSocket;
	conns[i].endpoint.type = ENDPOINT_CLIENT;
	Logger::debug("Connected client, socket: %d", clientSocket);
	return (&conns[i]);
}

static void sigcleanup(int sig)
{
	(void)sig;
	g_ServerShoudClose = true;
}

static void handlesignals(void(*hdl)(int))
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = hdl;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
}

int	run(std::vector<Configuration> serverMap)
{
	int	error = 1;

	handlesignals(sigcleanup);
	int qfd = queue_create();
	if (qfd < 0)
		return (1);

	const int	endpoints_count_max = serverMap.size();
	int	endpoints_count = 0;
	Endpoint_t	*endpoints = new(std::nothrow) Endpoint_t[endpoints_count_max];
	if (endpoints == NULL)
		goto cleanup;
	error = start_servers(serverMap, endpoints,
			endpoints_count_max, &endpoints_count);
	if (error)
		goto cleanup;
	for (int i = 0; i < endpoints_count; i++)
	{
		error = queue_add_fd(qfd, endpoints[i].sockfd, QUEUE_EVENT_READ, &endpoints[i]);
		if (error)
		{
			Logger::warn("Error registering server socket events, "
					"execution cannot proceed");
			goto cleanup;
		}
	}

	queue_event events[QUEUE_MAX_EVENTS];
	memset(events, 0, sizeof(events));
	Connection	conns[MAXCONNS];
	memset(conns, 0, sizeof(conns));
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
		for (int i = 0; i < nready; i++)
		{
			Endpoint_t *endp = (Endpoint_t *)queue_event_get_data(&events[i]);
			if (endp->type == ENDPOINT_SERVER)
			{
				Connection *client = connectNewClient(conns, endp);
				printf("Serve socket: %d\n", endp->sockfd);
				printf("Current Client socket: %d\n", client->endpoint.sockfd);
				queue_add_fd(qfd, client->endpoint.sockfd, QUEUE_EVENT_READ, client);
			}
			else if (endp->type == ENDPOINT_CLIENT)
			{
				int clientSocket = endp->sockfd;
				assert(clientSocket > 0);
				HttpConnectionHandler handler;
				handler.setClientSocket(clientSocket);
				if (handler.parseRequest() == true)
					handler.handleRequest();
				exit(0);
			}
		}
	}

cleanup:
	cleanup(endpoints, endpoints_count, qfd);
	if (error)
		return (1);
	return (0);
}
