#include "Server.hpp"
#include "Queue.hpp"

void	disconnectClient(Endpoint *client, int qfd);

void	receiveHeader(Endpoint *client, int qfd)
{
	HandlerStatus status = client->handler.parseRequest();
	switch (status)
	{
		case S_Again:
			// Here might be a good place to check for Slowloris behaviour
			break;
		case S_Done:
			queue_mod_fd(qfd, client->handler.getClientSocket(), QUEUE_EVENT_WRITE, client);
			client->state = CONNECTION_SEND_RESPONSE;
			break;
		case S_ClosedConnection:
			disconnectClient(client, qfd);
			break;
		case S_Error:
			assert(queue_mod_fd(qfd, client->handler.getClientSocket(), QUEUE_EVENT_WRITE, client) == 0);
			client->state = CONNECTION_SEND_RESPONSE;
			break;
		case S_ReadBody:
			client->state = CONNECTION_RECV_BODY;
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
			queue_mod_fd(qfd, client->handler.getClientSocket(), QUEUE_EVENT_WRITE, client); // error handle here
			client->state = CONNECTION_SEND_RESPONSE;
			break;
		case S_Done:
			queue_mod_fd(qfd, client->handler.getClientSocket(), QUEUE_EVENT_WRITE, client); // error handle here
			client->state = CONNECTION_SEND_RESPONSE;
			break;
		case S_ClosedConnection:
			disconnectClient(client, qfd);
			break;
		case S_ReadBody:
			/* Unreachable */
			assert(false);
			break;
	}
}

void	disconnectClient(Endpoint *client, int qfd)
{
	assert(client->state != CONNECTION_DISCONNECTED);
	logDebug("Disconnecting %d", client->handler.getClientSocket());
	queue_rem_fd(qfd, client->handler.getClientSocket());
	close(client->handler.getClientSocket());
	client->state = CONNECTION_DISCONNECTED;
	client->sockfd = -1;
	bzero(client->IP, INET6_ADDRSTRLEN);
	bzero(client->port, PORT_STRLEN);
	client->began_sending_header_ms = 0;
	client->last_heard_from_ms = 0;
	client->handler.setClientSocket(-1);
	client->handler.resetObject();
}
