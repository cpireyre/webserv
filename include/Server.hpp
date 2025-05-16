#pragma once

# include "Configuration.hpp"
# include "Socket.hpp"
# include "Logger.hpp"
# include "Queue.hpp"
# include "HttpConnectionHandler.hpp"
# include "Timeout.hpp"
# include <csignal>

#ifdef DEBUG
constexpr uint64_t	CLIENT_TIMEOUT_THRESHOLD_MS = 15 * 1000; // Ten (10) seconds
constexpr uint64_t	RECV_HEADER_TIMEOUT_MS = 1 * 1000; // One (1) second
#else
constexpr uint64_t	CLIENT_TIMEOUT_THRESHOLD_MS = 60 * 1000; // One minute, like Nginx
constexpr uint64_t	RECV_HEADER_TIMEOUT_MS = 1 * 1000; // One (1) second
#endif

constexpr int MAXCONNS = 1000;
static_assert(MAXCONNS <= 1000, "cf. `ulimit -a`");
constexpr int MAX_SERVERS = 100;
static_assert(MAX_SERVERS < MAXCONNS, "have to leave room for client FDs!");

enum Kind {
	Client,
	Server,
  None
};

enum ConnectionState {
	C_DISCONNECTED,
	C_RECV_HEADER,
	C_SEND_RESPONSE,
	C_FILE_SERVE,
	C_RECV_BODY,
	C_TIMED_OUT,
	C_EXEC_CGI
};

constexpr int 		PORT_STRLEN = 12;
typedef struct {
		enum Kind			kind;
		int						sockfd;
		char					IP[INET6_ADDRSTRLEN];
		char					port[PORT_STRLEN];
		ConnectionState			state;
		uint64_t				began_sending_header_ms; // Client-only
		uint64_t				last_heard_from_ms; // Client-only
		HttpConnectionHandler	handler; // Client-only
		CgiHandler				cgiHandler;
} Endpoint;

extern int	run(const std::vector<Configuration> config);
extern void	serveConnection(Endpoint *conn, int qfd, queue_event_type event_type);
void		receiveHeader(Endpoint *client, int qfd);
void		receiveBody(Endpoint *client, int qfd);
void		disconnectClient(Endpoint *client, int qfd);
bool		isLiveClient(Endpoint *conn);
int			watch(int qfd, Endpoint *conn, enum queue_event_type t);
