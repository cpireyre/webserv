/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 10:09:58 by copireyr          #+#    #+#             */
/*   Updated: 2025/04/01 15:18:37 by copireyr         ###   ########.fr       */
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

	serverMap = parser(argv[1]);
	if (serverMap.size() < 1)
		return (1);

	handlesignals(sigcleanup);
	int qfd = queue_create();
	if (qfd < 0)
		return (1);

	const int	endpoints_count_max = serverMap.size();
	int	endpoints_count = 0;
	Endpoint_t	*endpoints = new(std::nothrow) Endpoint_t[endpoints_count_max];
	if (endpoints == NULL)
		goto cleanup;
	error = start_servers(serverMap, endpoints,
			endpoints_count_max, &endpoints_count);
	if (error)
		goto cleanup;
	for (int i = 0; i < endpoints_count; i++)
	{
		error = queue_add_fd(qfd, endpoints[i].sockfd, QUEUE_EVENT_READ, &endpoints[i]);
		if (error)
		{
			Logger::warn("Error registering server socket events, "
					"execution cannot proceed");
			goto cleanup;
		}
	}

	queue_event events[QUEUE_MAX_EVENTS];
	memset(events, 0, sizeof(events));
	Connection	conns[MAXCONNS];
	memset(conns, 0, sizeof(conns));
	while (!g_ServerShoudClose)
	{
		assert(g_ServerShoudClose == false);
		int nready = queue_wait(qfd, events, QUEUE_MAX_EVENTS);
		if (nready < 0 && !g_ServerShoudClose)
		{
			Logger::warn("Error getting events from kernel");
			error = 1;
			break;
		}
		for (int i = 0; i < nready; i++)
		{
			Endpoint_t *endp = (Endpoint_t *)queue_event_get_data(&events[i]);
			if (endp->type == ENDPOINT_SERVER)
			{
				Connection client = connectNewClient(conns, endp);
				(void)client;
			}
			else
				puts("Client socket event");
		}
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
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = hdl;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
}
