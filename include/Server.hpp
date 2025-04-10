#pragma once

# include "Configuration.hpp"
# include "Socket.hpp"
# include "Logger.hpp"
# include "HttpConnectionHandler.hpp"
# include <csignal>

constexpr int PORT_STRLEN = 12;

enum EndpointKind {
	ENDPOINT_SERVER,
	ENDPOINT_CLIENT,
	ENDPOINT_UNKNOWN,
};

enum ConnectionState {
	CONNECTION_RECV_HEADER,
	CONNECTION_SEND_RESPONSE,
	CONNECTION_DISCONNECTED,
};

class Endpoint {
	public:
		int						sockfd;
		char					IP[INET6_ADDRSTRLEN];
		char					port[PORT_STRLEN];
		bool					alive;
		EndpointKind			kind;
		HttpConnectionHandler	handler; // Only for clients
		ConnectionState			state; // Only for clients
};


int		start_servers(std::vector<Configuration> servers, Endpoint *endpoints,
		int endpoints_count_max, int *endpoints_count);
void	cleanup(Endpoint *endpoints, int endpoints_count, int qfd);

constexpr int MAXCONNS = 4096;
static_assert((MAXCONNS < 2100000), "The Hive machine I tested this on "
	   	"could not handle this many.");

Endpoint	*connectNewClient(HttpConnectionHandler *handlers, const Endpoint *endp);
