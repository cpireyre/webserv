/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Socket.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: copireyr <copireyr@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 14:28:31 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/19 14:51:50 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Socket.hpp"

int Socket::set_nonblocking(int fd)
{
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
	{
        Logger::warn("fcntl SETFL:");
        return (1);
    }
    if (fcntl(fd, F_SETFD, FD_CLOEXEC) < 0)
	{
        Logger::warn("fcntl CLOEXEC:");
        return (1);
    }
    return (0);
}

int	Socket::make_listening_socket(int port)
{
	int	err;
	int insock = socket(AF_INET, SOCK_STREAM, 0);
	if (insock == -1)
		Logger::die("socket:");
	int opt = 1;
	if (setsockopt(insock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		Logger::die("setsockopt:");
	if (Socket::set_nonblocking(insock))
		exit(1);
	struct sockaddr_in address = {
		.sin_family = AF_INET,
		.sin_addr.s_addr = INADDR_ANY,
		.sin_port = htons(port)
	};
	err = bind(insock, (sockaddr*)&address, sizeof(address));
	if (err)
		Logger::die("bind:");
	err = listen(insock, INCOMING_QUEUE_LIMIT);
	if (err)
		Logger::die("listen:");
	return (insock);
}
