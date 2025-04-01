#include "Server.hpp"

static void	initEndpoint(int sockfd, std::string host, std::string port, IPAndPort_t *endpoint);
static bool endpointAlreadyBound(IPAndPort_t *endpoints, int count_to_check, std::string IP, std::string port);

int	start_servers(std::vector<Configuration> servers, IPAndPort_t *endpoints, int endpoints_count_max, int *endpoints_count)
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
		*endpoints_count += 1;
	}
	assert(*endpoints_count > 0);
	return (0);
}

// We run this after creating the server socket,
// so we already know the endpoint is valid.
static void	initEndpoint(int sockfd, std::string host, std::string port, IPAndPort_t *endpoint)
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
	assert(strlen(endpoint->port) >= 1);
	assert(strlen(endpoint->IP) >= 7);
	assert(strlen(endpoint->IP) <= 16);
}

static bool endpointAlreadyBound(IPAndPort_t *endpoints, int count_to_check, std::string IP, std::string port)
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

void	cleanup(IPAndPort_t *endpoints, int endpoints_count, int qfd)
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
