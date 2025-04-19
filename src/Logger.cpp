/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Logger.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: copireyr <copireyr@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 14:13:24 by copireyr          #+#    #+#             */
/*   Updated: 2025/04/19 21:12:40 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Logger.hpp"
#include <cerrno>
#include <cstdlib>

static void	verr_short(const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
	dprintf(2, "\n");
}

void	logDebug(const char *fmt, ...)
{
#ifdef DEBUG
	dprintf(2, "DEBUG: ");
	va_list	ap;
	va_start(ap, fmt);
	verr_short(fmt, ap);
	va_end(ap);
#endif
	(void)fmt;
}

void logError(const std::string& message)
{
	std::time_t now = std::time(nullptr);
	char timeBuf[20];
	strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
	std::cerr << "[" << timeBuf << "] ERROR: " << message << std::endl;
}

void logInfo(const std::string& message)
{
	std::time_t now = std::time(nullptr);
	char timeBuf[20];
	strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
	std::cout << "[" << timeBuf << "] INFO: " << message << std::endl;
}
