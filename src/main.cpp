/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 10:09:58 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/28 15:45:09 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Logger.hpp"
#include "Socket.hpp"
#include "Parser.hpp"
#include "Connection.hpp"
#include <sys/select.h>
#include <unistd.h>

std::vector<Configuration> parser(std::string fileName);

int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cout << "./webserv [configuration file]\n";
		return (1);
	}

	std::vector<Configuration> servers = parser(argv[1]);
}
