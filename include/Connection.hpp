/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: copireyr <copireyr@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 15:00:51 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/19 15:20:26 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include "Logger.hpp"
#include "Socket.hpp"
#include <unistd.h>

class Connection final
{
	public:
		int		fd;
		static Connection	accept_connection(int listening_socket);
		static int	echo(int fd);
};
