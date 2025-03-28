/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 10:09:58 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/28 15:55:27 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Logger.hpp"
#include "Socket.hpp"
#include "Parser.hpp"
#include "Connection.hpp"
#include <sys/select.h>
#include <unistd.h>

std::vector<Configuration> parser(std::string fileName);
int		make_server_socket(const char *host, const char *port);
void	test_server_socket(int server);

int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cout << "./webserv [configuration file]\n";
		return (1);
	}

	std::vector<Configuration> servers = parser(argv[1]);
	for (const auto& server : servers)
	{
		std::cout << server._host << ":" << server._port << "\n";
		int server_fd = make_server_socket(server._host.data(), std::to_string(server._port).data());
		test_server_socket(server_fd);
		close(server_fd);
		std::cout << "OK\n";
	}
}
