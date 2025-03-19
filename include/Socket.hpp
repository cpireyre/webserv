/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Socket.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: copireyr <copireyr@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 14:31:23 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/19 14:52:00 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "Logger.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

class Socket final
{
	private:
		static constexpr int INCOMING_QUEUE_LIMIT = 32;

	public:
		static int	set_nonblocking(int fd);
		static int	make_listening_socket(int port);
};
