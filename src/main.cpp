/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: upolat <upolat@student.hive.fi>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 10:09:58 by copireyr          #+#    #+#             */
/*   Updated: 2025/05/21 16:04:30 by jhirvone         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Configuration.hpp"
#include "Server.hpp"
#include "Queue.hpp"
#include "Logger.hpp"

volatile sig_atomic_t g_ShouldStop = false;
static void stop(int sig) { (void)sig; g_ShouldStop = true; }
static void handlesignals(void(*hdl)(int));

std::vector<Configuration> serverMap;

std::vector<Configuration> parser(std::string fileName);

int	main(int argc, char **argv)
{
	handlesignals(stop);

	string inputConf;
	argc == 2 ? inputConf = argv[1] : inputConf = "complete.conf";

	try {
		serverMap = parser(inputConf);
	}
	catch (std::exception &e) {
		std::cerr << e.what() << std::endl;
		return 1;
	}
	
  if (g_ShouldStop) return (0);
	int status = run(serverMap);
	return (status);
}

static void handlesignals(void(*hdl)(int))
{
	struct sigaction sa;

	signal(SIGPIPE, SIG_IGN);
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = hdl;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, nullptr);
	sigaction(SIGHUP, &sa, nullptr);
	sigaction(SIGINT, &sa, nullptr);
	sigaction(SIGQUIT, &sa, nullptr);
}
