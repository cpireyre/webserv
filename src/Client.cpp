#include "Server.hpp"
#include "Queue.hpp"

void	disconnectClient(Endpoint *client, int qfd);

void	receiveHeader(Endpoint *client, int qfd)
{
	HandlerStatus status = client->handler.parseRequest();
	switch (status)
	{
		case S_Again:
			break;
		case S_Done:
			watch(qfd, client, WRITABLE);
			client->state = C_SEND_RESPONSE;
			break;
		case S_ClosedConnection:
			disconnectClient(client, qfd);
			break;
		case S_Error:
			assert(watch(qfd, client, WRITABLE) == 0);
			client->state = C_SEND_RESPONSE;
			break;
		case S_ReadBody:
			client->state = C_RECV_BODY;
			break;
	}
}

void	receiveBody(Endpoint *client, int qfd)
{
	HandlerStatus status = client->handler.readBody();
	switch (status)
	{
		case S_Again:
			// probably do nothing? maybe some safety checks
			break;
		case S_Error:
			watch(qfd, client, WRITABLE); // error handle here
			client->state = C_SEND_RESPONSE;
			break;
		case S_Done:
			watch(qfd, client, WRITABLE); // error handle here
			client->state = C_SEND_RESPONSE;
			break;
		case S_ClosedConnection:
			disconnectClient(client, qfd);
			break;
		default:
			break;
	}
}

void	disconnectClient(Endpoint *client, int qfd)
{
	assert(client->state != C_DISCONNECTED);
	logDebug("Disconnecting %d", client->handler.getClientSocket());
	queue_rem_fd(qfd, client->handler.getClientSocket());
	close(client->handler.getClientSocket());
	client->state = C_DISCONNECTED;
	client->sockfd = -1;
	bzero(client->IP, INET6_ADDRSTRLEN);
	bzero(client->port, PORT_STRLEN);
	client->began_sending_header_ms = 0;
	client->last_heard_from_ms = 0;
	client->handler.setClientSocket(-1);
	client->handler.resetObject();
}

bool	isLiveClient(Endpoint *conn)
{
	bool	isServer = conn->state == C_ACTUALLY_A_SERVER;
	bool	isOffline = conn->state == C_DISCONNECTED;

	return (!isServer && !isOffline);
}
