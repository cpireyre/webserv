#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <sys/time.h>

static void	*repeat(void *ctx, int requests_count);
static int	Connect(char *hostname, int port);
static void	die(const char *err_msg);
static int64_t	now_ms(void);

int	main(int argc, char **argv)
{
	signal(SIGPIPE, SIG_IGN);
	if (argc != 3)
	{
		printf("Usage: %s IP port\n", argv[0]);
		printf("Example: %s 127.0.0.1 8080\n", argv[0]);
		return (0);
	}

	int requests_count = 1;
	for (int i = 0; i < 6; i++)
	{
		uint64_t start_time_ms = now_ms();
		repeat(argv, requests_count);
		uint64_t	total_time_ms = now_ms() - start_time_ms;
		printf("Successfully completed %d requests in %lums\n",
				requests_count,
				total_time_ms);
		requests_count *= 10;
	}
	return (0);
}

static void	*repeat(void *ctx, int requests_count)
{
	char **argv = (char **)ctx;
	const char *request = "GET /hi.txt HTTP/1.1\r\nHost: default.com\r\n\r\n";
	const size_t		len = strlen(request);
	int	sockfd;

	char *response_exemplar = "HTTP/1.1 200 OK\r\n"\
							   "Content-Length: 3\r\n"\
							   "Content-Type: text/plain\r\n"\
							   "Connection: Keep-Alive\r\n"\
							   "\r\n"\
							   "Hi\n";

	int	response_exemplar_len = strlen(response_exemplar);
	sockfd = Connect(argv[1], atoi(argv[2]));
	for (int i = 0; i < requests_count; i++)
	{
		int err = write(sockfd, request, len);
		if (err < 0)
		{
			perror("write");
			break;
		}
		char buf[100];
		bzero(buf, sizeof(buf));
		int total_read_bytes = 0;
		while (total_read_bytes < response_exemplar_len)
		{
			int response_len = read(sockfd, buf + total_read_bytes, 100);
			if (response_len < 0)
			{
				perror("read");
				break;
			}
			total_read_bytes += response_len;
		}
		if (memcmp(response_exemplar, buf, response_exemplar_len) != 0)
		{
			printf("failed %d, read %d\n", i, total_read_bytes);
			write(1, buf, total_read_bytes);
			break;
		}
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

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons((uint16_t)port);
	if (inet_pton(AF_INET, hostname, &servaddr.sin_addr) <= 0)
		die("inet_pton");

	if (connect(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		if (errno != EINPROGRESS)
			die("connect");
	}

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

static int64_t	now_ms(void)
{
	struct timeval	timeofday;

	gettimeofday(&timeofday, NULL);
	return ((timeofday.tv_sec * 1000LL) + (timeofday.tv_usec / 1000LL));
}
