/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 10:09:58 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/31 13:02:55 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Configuration.hpp"
#include "Server.hpp"
#include "Logger.hpp"

std::vector<Configuration> parser(std::string fileName);

int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cout << "./webserv [configuration file]\n";
		return (1);
	}
	std::vector<Configuration> servers = parser(argv[1]);
	if (servers.size() < 1)
		return (1);
	const int	endpoints_count_max = servers.size();
	IPAndPort_t	*endpoints = new(std::nothrow) IPAndPort_t[endpoints_count_max];
	if (endpoints == NULL)
		Logger::die("Couldn't allocate memory for vhosts");
	int	endpoints_count;
	int error = start_servers(servers, endpoints, endpoints_count_max, &endpoints_count);
	if (error != 0)
	{
		std::cerr << "Error starting server\n";
		cleanup_servers(endpoints, endpoints_count);
		return (1);
	}
	cleanup_servers(endpoints, endpoints_count);
	return (0);
}
