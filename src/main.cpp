/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: copireyr <copireyr@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 10:09:58 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/19 15:24:34 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Logger.hpp"
#include "Socket.hpp"
#include "Connection.hpp"
#include <sys/select.h>
#include <unistd.h>


int	main(int argc, char **argv)
{
	if (argc != 2)
	{
		std::cout << "./webserv [configuration file]\n";
		return (1);
	}
	Logger::debug("Config path: %s", argv[1]);

	int listening_socket = Socket::make_listening_socket(4242);
	fd_set	fds_to_read;
	FD_ZERO(&fds_to_read);
	int	max_fd = listening_socket;
	while (true)
	{
		FD_SET(listening_socket, &fds_to_read);
		struct timeval timeout = {
			.tv_sec = 1,
			.tv_usec = 0,
		};
		fd_set	tmp_read = fds_to_read;
		int activity = select(max_fd + 1, &tmp_read, NULL, NULL, &timeout);
		if (activity == -1)
			Logger::die("select:");
		if (FD_ISSET(listening_socket, &tmp_read))
		{
			Connection conn = Connection::accept_connection(listening_socket);
			FD_SET(conn.fd, &fds_to_read);
			if (conn.fd > max_fd)
				max_fd = conn.fd;
		}
		for (int fd = 3; fd <= max_fd; fd++) // start at 3 to skip stdin, stdout, stderr?
		{
			if (FD_ISSET(fd, &tmp_read) && fd != listening_socket)
			{
				int status = Connection::echo(fd);
				if (status <= 0) // if error in receiving or sending
				{
					FD_CLR(fd, &fds_to_read); // we've closed the connection, so forget it
					if (fd == max_fd)
						while (max_fd > 0 && !FD_ISSET(max_fd, &fds_to_read))
							max_fd--;
				}
			}
		}
	}
	return (0);
}
