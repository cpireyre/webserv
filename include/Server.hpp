#pragma once

# include "Configuration.hpp"
# include "Socket.hpp"
# include "Logger.hpp"
# include "HttpConnectionHandler.hpp"
# include "Timeout.hpp"
# include <csignal>

#ifdef DEBUG
constexpr uint64_t	CLIENT_TIMEOUT_THRESHOLD_MS = 10 * 1000; // Ten (10) seconds
constexpr uint64_t	RECV_HEADER_TIMEOUT_MS = 1 * 1000; // One (1) second
#else
constexpr uint64_t	CLIENT_TIMEOUT_THRESHOLD_MS = 60 * 1000; // One minute, like Nginx
constexpr uint64_t	RECV_HEADER_TIMEOUT_MS = 1 * 1000; // One (1) second
#endif

#ifdef __linux__
constexpr int MAXCONNS = 1000;
static_assert(MAXCONNS <= 1000); /* cf. `ulimit -a` for the bottleneck */
#else
constexpr int MAXCONNS = 2000;
static_assert(MAXCONNS <= 2000); /* cf. `ulimit -a` for the bottleneck */
#endif


enum ConnectionState {
	CONNECTION_DISCONNECTED,
	CONNECTION_RECV_HEADER,
	CONNECTION_SEND_RESPONSE,
	CONNECTION_FILE_SERVE,
	CONNECTION_RECV_BODY,
	CONNECTION_TIMED_OUT,
	CONNECTION_ACTUALLY_A_SERVER,
};

constexpr int 		PORT_STRLEN = 12;
class Endpoint {
	public:
		int						sockfd;
		char					IP[INET6_ADDRSTRLEN];
		char					port[PORT_STRLEN];
		ConnectionState			state;
		uint64_t				began_sending_header_ms; // Client-only
		uint64_t				last_heard_from_ms; // Client-only
		HttpConnectionHandler	handler; // Client-only
};


extern int	run(const std::vector<Configuration> config);
void		receiveHeader(Endpoint *client, int qfd);
void		receiveBody(Endpoint *client, int qfd);
void		disconnectClient(Endpoint *client, int qfd);
