#pragma once

# include "Configuration.hpp"
# include "Socket.hpp"
# include "Logger.hpp"
# include "HttpConnectionHandler.hpp"
# include "Timeout.hpp"
# include <csignal>

constexpr int 		PORT_STRLEN = 12;
constexpr uint64_t	CLIENT_TIMEOUT_THRESHOLD_MS = 2 * 1000; // Ten (10) seconds
constexpr uint64_t	RECV_HEADER_TIMEOUT_MS = 1 * 1000; // One (1) second

#ifdef __linux__
constexpr int MAXCONNS = 1024;
static_assert(MAXCONNS <= 1024); // Haven't tested more than 1024 on Linux
#else
constexpr int MAXCONNS = 2048;
static_assert(MAXCONNS <= 2048); // It won't go past this on macOS?
#endif


enum ConnectionState {
	CONNECTION_DISCONNECTED,
	CONNECTION_RECV_HEADER,
	CONNECTION_SEND_RESPONSE,
	CONNECTION_RECV_BODY,
	CONNECTION_TIMED_OUT,
	CONNECTION_ACTUALLY_A_SERVER,
};

class Endpoint {
	public:
		int						sockfd;
		char					IP[INET6_ADDRSTRLEN];
		char					port[PORT_STRLEN];
		ConnectionState			state;
		int						error; // Client-only
		uint64_t				began_sending_header_ms; // Client-only
		uint64_t				last_heard_from_ms; // Client-only
		HttpConnectionHandler	handler; // Client-only
};


void		receiveHeader(Endpoint *client, int qfd);
void		receiveBody(Endpoint *client, int qfd);
void		disconnectClient(Endpoint *client, int qfd);
