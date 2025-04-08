/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 10:09:58 by copireyr          #+#    #+#             */
/*   Updated: 2025/04/08 10:14:11 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Configuration.hpp"
#include "Server.hpp"
#include "Queue.hpp"
#include "Logger.hpp"

std::vector<Configuration> serverMap;

std::vector<Configuration> parser(std::string fileName);

int	run(std::vector<Configuration> serverMap);

int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cout << "./webserv [configuration file]\n";
		return (1);
	}

	try {
		serverMap = parser(argv[1]);
		if (serverMap.size() < 1)
			return (1);
	}
	catch (std::exception &e) {
		e.what();
	}
	
	int status = run(serverMap);
	return (status);
}
