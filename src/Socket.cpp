#include "Socket.hpp"
#include "Logger.hpp"

int	make_server_socket(const char *host, const char *port)
{
	assert(host != NULL);
	assert(port != NULL);

	const struct addrinfo	hints = {
		.ai_flags = AI_NUMERICSERV,
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
	};

	struct addrinfo *addr = NULL;
	int error = getaddrinfo(host, port, &hints, &addr);
	if (error)
	{
		dprintf(2, "die: getaddrinfo: %s\n", gai_strerror(error));
		return (-1);
	}

	int insock = -1;
	struct addrinfo *p = NULL;
	const int enable = 1;
	for (p = addr; p; p = p->ai_next)
	{
		insock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (insock < 0)
			continue ;
		if (setsockopt(insock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		{
			dprintf(2, "die: setsockopt\n");
			freeaddrinfo(addr);
			return (-1);
		}
		if (bind(insock, p->ai_addr, p->ai_addrlen) < 0)
		{
			close(insock);
			continue ;
		}
		break ;
	}
	freeaddrinfo(addr);
	if (!p)
	{
		Logger::warn("Error binding to %s:%s", host, port);
		return (-1);
	}
	if (socket_set_nonblocking(insock) < 0)
	{
		close(insock);
		dprintf(2, "die: fcntl\n");
		return (-1);
	}
	if (listen(insock, SOMAXCONN) < 0)
	{
		close(insock);
		dprintf(2, "die: listen\n");
		return (-1);
	}
	assert(insock >= 0);
	return (insock);
}

void	test_server_socket(int server)
{
	assert(server >= 0);
	struct sockaddr	address_server;
	memset(&address_server, 0, sizeof(address_server));
	socklen_t		address_server_len = sizeof(address_server);
	int status = getsockname(server, &address_server, &address_server_len);
	assert(status == 0);

	const int clients_count = SOMAXCONN;
	int clients[clients_count];
	for (int i = 0; i < clients_count; i++)
	{
		clients[i] = socket(AF_INET, SOCK_STREAM, 0);
		assert(clients[i] >= 0);
		status = connect(clients[i], &address_server, address_server_len);
		if (status != 0)
		{
			perror(NULL);
			dprintf(2, "errno: %d\n", errno);
		}
		assert(status == 0);
	}
	for (int i = 0; i < clients_count; i++)
		assert(close(clients[i]) == 0);
}

int	socket_set_nonblocking(int sock)
{
	assert(sock >= 0);
	int	error;

	error = fcntl(sock, F_SETFL, O_NONBLOCK);
	if (error)
		return (-1);
	error = fcntl(sock, F_SETFD, FD_CLOEXEC);
	if (error)
		return (-1);
	return (0);
}
