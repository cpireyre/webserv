#include "Server.hpp"
#include "Queue.hpp"

void	disconnectClient(Endpoint *client, int qfd);

void	serveConnection(Endpoint *conn, int qfd, queue_event_type event_type)
{
	switch (conn->state) {
		case C_TIMED_OUT:
			conn->state = C_SEND_RESPONSE;
			break;

		case C_RECV_HEADER: assert(event_type == READABLE);
			receiveHeader(conn, qfd);
			conn->last_heard_from_ms = now_ms();
			break;

		case C_RECV_BODY: assert(event_type == READABLE);
			receiveBody(conn, qfd);
			break;

		case C_SEND_RESPONSE: assert(event_type == WRITABLE);
			if (conn->handler.getErrorCode() != 0)
			{
				/* depending on handler.getfileServ() we already have the whole response
				 * or we are just sending everything but body */
				logDebug("Error with %d", conn->sockfd);
				std::string response = conn->handler
					.createErrorResponse(conn->handler.getErrorCode());
				send(conn->sockfd, response.c_str(), response.size(), 0);
				if (conn->handler.getFileServ()) //update time stamp?
					conn->state = C_FILE_SERVE;
				else
					disconnectClient(conn, qfd);
			}
			else
			{
				conn->handler.handleRequest();
				send(conn->sockfd, conn->handler.getResponse().c_str(), conn->handler.getResponse().size(), 0);
				if (conn->handler.getFileServ()) {
					conn->state = C_FILE_SERVE;
					break;
				}
				watch(qfd, conn, READABLE);
				conn->state = C_RECV_HEADER;
				conn->handler.resetObject();
				conn->began_sending_header_ms = now_ms();
			}
			break;

		case C_FILE_SERVE: assert(event_type == WRITABLE);
			 switch(conn->handler.serveFile()) {
				 case S_Done:
					 watch(qfd, conn, READABLE);
					 conn->state = C_RECV_HEADER;
					 if (conn->handler.getErrorCode() != 0) disconnectClient(conn, qfd);
					 conn->handler.resetObject();
					 break;

				 case S_Error: disconnectClient(conn, qfd);
					 break;

				 case S_Again:
				 	break;
				 case S_ClosedConnection:
				 	break;
				 case S_ReadBody:
					break;
			 }
			 break;

		case C_EXEC_CGI:
			switch (conn->handler.serveCgi(conn->cgiHandler))
			{
				case S_Error: disconnectClient(conn, qfd);
					 break;
				case S_Again: break;

				case S_Done:
					write(conn->handler.getClientSocket(), conn->handler.getResponse().c_str(), conn->handler.getResponse().size());
					watch(qfd, conn, READABLE);
					conn->state = C_RECV_HEADER;
					break;
				case S_ClosedConnection: break;
				case S_ReadBody: break;
			}
		break;

		case C_DISCONNECTED:
		  break;
	}
}

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
			if (client->handler.checkLocation() == true) {
				if (client->handler.checkCgi() != NONE) {
					client->cgiHandler = CgiHandler(client->handler);
					if (!std::filesystem::exists(client->cgiHandler._pathToScript)
							|| !std::filesystem::is_regular_file(client->cgiHandler._pathToScript)
)
					{
						client->handler.setErrorCode(404);
						client->state = C_SEND_RESPONSE;
					}
					else
					{
						client->cgiHandler.executeCgi();
						client->state = C_EXEC_CGI;
					}
				}
			}
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
			break;
		case S_Error:
			watch(qfd, client, WRITABLE);
			client->state = C_SEND_RESPONSE;
			break;
		case S_Done:
			watch(qfd, client, WRITABLE);
			client->state = C_SEND_RESPONSE;
			break;
		case S_ClosedConnection:
			disconnectClient(client, qfd);
			break;
		case S_ReadBody:
			assert(false); /* Unreachable */
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
	if (client->cgiHandler.cgiPid != 0)
	{
		kill(client->cgiHandler.cgiPid, SIGKILL);
		logDebug("SIGKILL -> %d", client->cgiHandler.cgiPid);
	}
}

bool	isLiveClient(Endpoint *conn)
{
	bool	isServer = conn->kind == Server;
	bool	isOffline = conn->state == C_DISCONNECTED;

	return (!isServer && !isOffline);
}
