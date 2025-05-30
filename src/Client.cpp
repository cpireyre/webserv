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

				std::string response;
				if (conn->handler.getResponse().empty()) {
					response = conn->handler
					.createErrorResponse(conn->handler.getErrorCode());
				}
				else {
					response = conn->handler.getResponse();
				}
				ssize_t sent = send(conn->sockfd, response.c_str(), response.size(), 0);
				if (sent == -1) {
					disconnectClient(conn, qfd);
				}
				else if(static_cast<size_t>(sent) != response.size()) {
					conn->handler.setResponse(response.substr(sent));
					break;
				}
				if (conn->handler.getFileServ())
					conn->state = C_FILE_SERVE;
				else
          conn->state = C_MARKED_FOR_DISCONNECTION;
			}
			else
			{
				conn->handler.handleRequest();
				ssize_t sent = send(conn->sockfd, conn->handler.getResponse().c_str(), conn->handler.getResponse().size(), 0);
				if (sent == -1) {
					disconnectClient(conn, qfd);
				}
				else if(static_cast<size_t>(sent) != conn->handler.getResponse().size()) {
					conn->handler.setResponse(conn->handler.getResponse().substr(sent));
					break;
				}
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
					 if (conn->handler.getErrorCode() != 0) conn->state = C_MARKED_FOR_DISCONNECTION;
           else conn->handler.resetObject();
					 break;

				 case S_Error: conn->state = C_MARKED_FOR_DISCONNECTION;
					 break;

				 case S_Again: 						break;
				 case S_ClosedConnection: break;
				 case S_ReadBody: 				break;
			 }
			 break;

		case C_EXEC_CGI:
			switch (conn->handler.serveCgi(conn->cgiHandler))
			{
				case S_Error:
                conn->cgiHandler.CgiResetObject();
					      conn->handler.setErrorCode(500);
					      conn->handler.setResponse("");
					      conn->state = C_SEND_RESPONSE;
					 break;
				case S_Again: break;

				case S_Done:
                conn->cgiHandler.CgiResetObject();
					      conn->state = C_SEND_RESPONSE;
					break;
				case S_ClosedConnection: break;
				case S_ReadBody: break;
			}
		break;

		case C_DISCONNECTED:
		  break;

    case C_MARKED_FOR_DISCONNECTION:
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
      logDebug("Done receiving header");
			watch(qfd, client, WRITABLE);
			client->state = C_SEND_RESPONSE;
			if (client->handler.checkLocation() == true) {
				if (client->handler.checkCgi() != NONE) {
					client->cgiHandler.populate(client->handler);
          if (access(client->cgiHandler._pathToScript.c_str(), F_OK) == 0
              && access(client->cgiHandler._pathToScript.c_str(), R_OK) == 0)
          {
            client->cgiHandler.executeCgi();
            client->state = C_EXEC_CGI;
          }
          else
          {
            client->handler.setErrorCode(404);
            client->state = C_SEND_RESPONSE;
          }
				}
			}
			break;
		case S_ClosedConnection:
			client->state = C_MARKED_FOR_DISCONNECTION;
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
			if (client->handler.checkLocation() == true && client->handler.checkCgi() != NONE)
            {
                client->cgiHandler.populate(client->handler);
                if (access(client->cgiHandler._pathToScript.c_str(), F_OK) == 0
                && access(client->cgiHandler._pathToScript.c_str(), R_OK) == 0)
                {
                    logDebug("We have all permissions");
                    client->cgiHandler.executeCgi();
                    client->state = C_EXEC_CGI;
                }
                else
                {
                    client->handler.setErrorCode(404);
                    client->state = C_SEND_RESPONSE;
                }
            }
			break;
		case S_ClosedConnection:
			client->state = C_MARKED_FOR_DISCONNECTION;
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
	client->cgiHandler.CgiResetObject();
}

bool	isLiveClient(Endpoint *conn)
{
  return (conn->kind == Client && conn->state != C_DISCONNECTED);
}
