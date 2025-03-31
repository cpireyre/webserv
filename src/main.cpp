/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 10:09:58 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/31 14:40:41 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Configuration.hpp"
#include "Server.hpp"
#include "Queue.hpp"
#include "Logger.hpp"
#include <signal.h>

std::vector<Configuration> parser(std::string fileName);
volatile sig_atomic_t ServerShouldClose = false;

static void sigcleanup(int sig);
static void handlesignals(void(*hdl)(int));

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
	handlesignals(sigcleanup);
	int qfd = queue_create();
	if (qfd < 0)
	{
		cleanup_servers(endpoints, endpoints_count);
		return (1);
	}
	while (!ServerShouldClose)
	{
	}
	Logger::debug("Received termination signal, cleaning up");
	cleanup_servers(endpoints, endpoints_count);
	return (0);
}

static void sigcleanup(int sig)
{
	(void)sig;
	ServerShouldClose = true;
}

static void handlesignals(void(*hdl)(int))
{
	struct sigaction sa = {
		.sa_handler = hdl,
	};

	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
}
