#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

static void	*talk_slow(void *ctx, uint32_t sleep_delay_s);
static int	Connect(char *hostname, int port);
static void	die(const char *err_msg);
static int	socket_set_nonblocking(int sock);

int	main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);
	if (argc != 3)
	{
		printf("Usage: %s IP port\n", argv[0]);
		printf("Example: %s 127.0.0.1 8080\n", argv[0]);
		return (0);
	}

	puts("Soft timeout test: should receive 408");
	talk_slow(argv, 11);
	puts("\nHard timeout test: disconnect with no response");
	talk_slow(argv, 32);
	return (0);
}

static void	*talk_slow(void *ctx, uint32_t sleep_delay_s)
{
	char **argv = (char **)ctx;
	const char *request = "GET /big.txt HTTP/1.1\r\nHost: default.com\r\n\r\n";
	const size_t		len = strlen(request);
	int	sockfd;

	sockfd = Connect(argv[1], atoi(argv[2]));
	for (size_t offset = 0; offset < len; offset++)
	{
		int err = write(sockfd, request + offset, 1);
		if (err < 0)
		{
			perror("write");
			break;
		}
		sleep(sleep_delay_s);
		char	buf[1024];
		int bytes_read = read(sockfd, buf, 1024);
		if (bytes_read > 0)
			write(1, buf, bytes_read);
	}
	close(sockfd);
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
	dprintf(2, "%s\n", err_msg);
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
