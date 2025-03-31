#pragma once

#include <netdb.h>
#include <cassert>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>

int		make_server_socket(const char *host, const char *port);
void	test_server_socket(int server);
int		socket_set_nonblocking(int sock);
