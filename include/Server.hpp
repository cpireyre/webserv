#pragma once

# include "Configuration.hpp"
# include "Socket.hpp"
# include "Logger.hpp"

# define PORT_STRLEN 12

typedef struct IPAndPort {
	int		sockfd;
	char	IP[INET6_ADDRSTRLEN];
	char	port[PORT_STRLEN];
} IPAndPort_t;

int	start_servers(std::vector<Configuration> servers, IPAndPort_t *endpoints, int endpoints_count_max, int *endpoints_count);
void	cleanup(IPAndPort_t *endpoints, int endpoints_count, int qfd);
