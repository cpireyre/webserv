#pragma once

# include <iostream>
# include <cstring>
# include <cstdarg>

class Logger final
{
	public:
		static void	warn(const char *fmt, ...);
		static void	die(const char *fmt, ...);
		static void	warn_short(const char *fmt, ...);
		static void	debug(const char *fmt, ...);
};
