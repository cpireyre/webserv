#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

static void	*talk_slow(void *ctx);
static void	*talk(void *ctx);
static int	Connect(char *hostname, int port);
static void	die(const char *err_msg);
static int	socket_set_nonblocking(int sock);

# define WORKERS_COUNT 300
# define REQUESTS_PER_WORKER 200
# define CONN_DELAY_MICROSECONDS 50

int	main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);
	if (argc != 3) {
		printf("Usage: %s IP port\n", argv[0]);
		printf("Example: %s 127.0.0.1 8080\n", argv[0]);
		return (0);
	}

	(void)talk;
	(void)talk_slow;
	pthread_t	workers[WORKERS_COUNT];
	for (int i = 0; i < WORKERS_COUNT; i++)
	{
		pthread_create(&workers[i], NULL, talk_slow, argv);
		usleep(CONN_DELAY_MICROSECONDS);
	}
	for (int i = 0; i < WORKERS_COUNT; i++)
		pthread_join(workers[i], NULL);

	return (0);
}

static void	*talk_slow(void *ctx)
{
	char **argv = (char **)ctx;
	const char *request = "GET /big.txt HTTP/1.1\r\nHost: default.com\r\n\r\n";
	const size_t		len = strlen(request);
	for (int i = 0; i < REQUESTS_PER_WORKER; i++)
	{
		int	sockfd;

		sockfd = Connect(argv[1], atoi(argv[2]));
		for (size_t offset = 0; offset < len; offset++)
		{
			int err = write(sockfd, request + offset, 1);
			sleep(10);
			if (err < 0)
				perror("write");
		}
		close(sockfd);
	}
	return (NULL);
}

static void	*talk(void *ctx)
{
	char **argv = (char **)ctx;
	const char *request = "GET /index.html HTTP/1.1\r\nHost: default.com\r\n\r\n";
	const size_t		len = strlen(request);
	for (int i = 0; i < REQUESTS_PER_WORKER; i++)
	{
		int	sockfd;

		sockfd = Connect(argv[1], atoi(argv[2]));
		int err = write(sockfd, request, len);
		if (err < 0)
			perror("write");
		close(sockfd);
	}
	return (NULL);
}

static int	Connect(char *hostname, int port)
{
	int					sockfd;
	struct sockaddr_in	servaddr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		die("socket");
	socket_set_nonblocking(sockfd);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons((uint16_t)port);
	if (inet_pton(AF_INET, hostname, &servaddr.sin_addr) <= 0)
		die("inet_pton");

	if (connect(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		if (errno != EINPROGRESS)
			die("connect");
	}

	fd_set	fds;
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);
	select(sockfd + 1, NULL, &fds, NULL, NULL);

	int err = 0;
	socklen_t len = sizeof(err);
	getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &err, &len);
	if (err != 0)
	{
		errno = err;
		perror("connect");
		die("non-blocking connect failed");
	}
	return (sockfd);
}

static void	die(const char *err_msg)
{
	perror(err_msg);
	exit(1);
}

static int	socket_set_nonblocking(int sock)
{
	int	error;

	error = fcntl(sock, F_SETFL, O_NONBLOCK);
	if (error)
		die("fcntl");
	error = fcntl(sock, F_SETFD, FD_CLOEXEC);
	if (error)
		die("fcntl");
	return (0);
}

