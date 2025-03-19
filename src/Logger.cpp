/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Logger.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: copireyr <copireyr@student.hive.fi>        +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/03/19 14:13:24 by copireyr          #+#    #+#             */
/*   Updated: 2025/03/19 14:43:23 by copireyr         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Logger.hpp"

static void	verr(const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
	if (fmt[0] && fmt[strlen(fmt) - 1] == ':')
		std::cerr << " " << strerror(errno) << "\n";
	else
		std::cerr << "\n";
}

void	Logger::warn(const char *fmt, ...)
{
	va_list	ap;
	va_start(ap, fmt);
	verr(fmt, ap);
	va_end(ap);
}

void	Logger::die(const char *fmt, ...)
{
	va_list	ap;
	va_start(ap, fmt);
	verr(fmt, ap);
	va_end(ap);
	exit(1);
}

static void	verr_short(const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
	std::cerr << "\n";
}

void	Logger::warn_short(const char *fmt, ...)
{
	va_list	ap;
	va_start(ap, fmt);
	verr_short(fmt, ap);
	va_end(ap);
}

void	Logger::debug(const char *fmt, ...)
{
#ifdef DEBUG
		std::cerr << "DEBUG: ";
		va_list	ap;
		va_start(ap, fmt);
		verr_short(fmt, ap);
		va_end(ap);
#endif
		(void)fmt;
}
