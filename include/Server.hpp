#pragma once

# include "Configuration.hpp"
# include "Socket.hpp"
# include "Logger.hpp"
# include "HttpConnectionHandler.hpp"
# include "Timeout.hpp"
# include <csignal>

constexpr int PORT_STRLEN = 12;
constexpr uint64_t	CLIENT_TIMEOUT_MS = 1000; /* One (1) second */

enum EndpointKind {
	ENDPOINT_SERVER,
	ENDPOINT_CLIENT,
	ENDPOINT_UNKNOWN,
};

enum ConnectionState {
	CONNECTION_RECV_HEADER,
	CONNECTION_SEND_RESPONSE,
	CONNECTION_DISCONNECTED,
	CONNECTION_TIMED_OUT,
};

class Endpoint {
	public:
		int						sockfd;
		char					IP[INET6_ADDRSTRLEN];
		char					port[PORT_STRLEN];
		bool					alive;
		int						error;
		uint64_t				lastHeardFrom_ms;
		EndpointKind			kind;
		HttpConnectionHandler	handler; // Only for clients
		ConnectionState			state; // Only for clients
};


int		start_servers(std::vector<Configuration> servers, Endpoint *endpoints,
		int endpoints_count_max, int *endpoints_count);
void	cleanup(Endpoint *endpoints, int endpoints_count, int qfd);

constexpr int MAXCONNS = 1024;
static_assert(MAXCONNS <= 1024); /* Might not make sense to go past FD limit */

Endpoint	*connectNewClient(HttpConnectionHandler *handlers, const Endpoint *endp);
