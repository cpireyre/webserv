/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: copireyr <copireyr@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 15:02:22 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/19 15:33:16 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"

Connection	Connection::accept_connection(int listening_socket)
{
	struct sockaddr client_address;
	socklen_t client_address_len = sizeof(client_address);

	int client_fd = accept(listening_socket, &client_address, &client_address_len);

	if (client_fd == -1)
		Logger::die("accept:");

	if (Socket::set_nonblocking(client_fd))
		Logger::die("Failed to set client socket non-blocking"); //TODO(colin): handle this, we shouldn't exit here

	Logger::debug("Opened client socket %d", client_fd);

	Connection	conn;
	conn.fd = client_fd;
	return (conn);
}

int	Connection::echo(int fd)
{
	char	buf[1000];
	int		msg_len = recv(fd, buf, 1000, 0);
	if (msg_len <= 0)
	{
		close(fd);
		if (msg_len == -1)
			Logger::warn("recv:");
	}
	else
	{
		if (send(fd, buf, msg_len, 0) < 0)
			Logger::warn("send:");
		Logger::debug("Echoed back to %d (bytes: %d)", fd, msg_len);
	}
	return (msg_len);
}
