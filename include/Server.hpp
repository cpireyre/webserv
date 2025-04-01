#pragma once

# include "Configuration.hpp"
# include "Socket.hpp"
# include "Logger.hpp"

constexpr int PORT_STRLEN = 12;

enum endpoint_type_t {
	ENDPOINT_SERVER,
	ENDPOINT_CLIENT,
};

typedef struct Endpoint {
	int						sockfd;
	char					IP[INET6_ADDRSTRLEN];
	char					port[PORT_STRLEN];
	enum endpoint_type_t	type;
} Endpoint_t;


int		start_servers(std::vector<Configuration> servers, Endpoint_t *endpoints, int endpoints_count_max, int *endpoints_count);
void	cleanup(Endpoint_t *endpoints, int endpoints_count, int qfd);

constexpr int MAXCONNS = 1024;
static_assert((MAXCONNS < 2100000), "The Hive machine I tested this on "
	   	"could not handle this many.");

typedef struct Connection {
	Endpoint_t	endpoint;
	bool		alive;
} Connection;

Connection	*connectNewClient(Connection *conns, const Endpoint_t *endp);
