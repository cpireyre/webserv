#pragma once

#include <netdb.h>
#include <cassert>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <cstdio>
#include <string>

int		make_server_socket(const char *host, const char *port);
void	test_server_socket(int server);
int		socket_set_nonblocking(int sock);
void	connect_and_make_test_request(std::string host, std::string port);
