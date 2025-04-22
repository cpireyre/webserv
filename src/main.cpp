/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 10:09:58 by copireyr          #+#    #+#             */
/*   Updated: 2025/04/20 16:12:09 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Configuration.hpp"
#include "Server.hpp"
#include "Queue.hpp"
#include "Logger.hpp"

std::vector<Configuration> serverMap;

std::vector<Configuration> parser(std::string fileName);

int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cerr << "./webserv [configuration file]" << std::endl;
		return (1);
	}

	try {
		serverMap = parser(argv[1]);
	}
	catch (std::exception &e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	
	int status = run(serverMap);
	return (status);
}
