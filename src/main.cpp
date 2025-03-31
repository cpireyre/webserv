/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 10:09:58 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/31 14:23:46 by upolat           ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Logger.hpp"
#include "Socket.hpp"
#include "Parser.hpp"
#include "Connection.hpp"
#include <sys/select.h>
#include <unistd.h>

std::vector<Configuration> serverMap;

std::vector<Configuration> parser(std::string fileName);
void	run_echo_server(void);

int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cout << "./webserv [configuration file]\n";
		return (1);
	}

	std::vector<Configuration> servers = parser(argv[1]);
	run_echo_server();

}
