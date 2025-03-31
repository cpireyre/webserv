/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 10:09:58 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/31 17:02:55 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Configuration.hpp"
#include "Server.hpp"
#include "Queue.hpp"
#include "Logger.hpp"
#include <signal.h>

std::vector<Configuration> serverMap;

std::vector<Configuration> parser(std::string fileName);
volatile sig_atomic_t g_ServerShoudClose = false;

static void sigcleanup(int sig);
static void handlesignals(void(*hdl)(int));

int	main(int argc, char **argv)
{
	int	error = 1;

	if (argc != 2)
	{
		std::cout << "./webserv [configuration file]\n";
		return (1);
	}

	std::vector<Configuration> servers = parser(argv[1]);
	if (servers.size() < 1)
		return (1);

	handlesignals(sigcleanup);
	int qfd = queue_create();
	if (qfd < 0)
		return (1);

	const int	endpoints_count_max = servers.size();
	int	endpoints_count = 0;
	IPAndPort_t	*endpoints = new(std::nothrow) IPAndPort_t[endpoints_count_max];
	if (endpoints == NULL)
		goto cleanup;
	error = start_servers(servers, endpoints,
			endpoints_count_max, &endpoints_count);
	if (error)
		goto cleanup;
	for (int i = 0; i < endpoints_count; i++)
	{
		error = queue_add_fd(qfd, endpoints[i].sockfd, QUEUE_EVENT_READ, NULL);
		if (error)
		{
			Logger::warn("Error registering server socket events, "
					"execution cannot proceed");
			goto cleanup;
		}
	}

	// queue_event events[QUEUE_MAX_EVENTS];
	while (!g_ServerShoudClose)
	{
		assert(g_ServerShoudClose == false);
		// TODO(colin)
		// int nready = kevent(qfd, NULL, 0, events, 64, NULL);
		// if (nready < 0 && !g_ServerShoudClose)
		// {
		// 	Logger::warn("Error getting events from kernel");
		// 	error = 1;
		// 	break;
		// }
		// for (int i = 0; i < nready; i++)
		// {
		// 	void *data = events[i].udata;
		// 	if (data == NULL)
		// 		puts("Server socket event");
		// 	int clientsock = accept();
		// }
	}

cleanup:
	cleanup(endpoints, endpoints_count, qfd);
	if (error)
		return (1);
	return (0);
}

static void sigcleanup(int sig)
{
	(void)sig;
	g_ServerShoudClose = true;
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
