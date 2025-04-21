#pragma once

# include <cstring>
# include <cstdio>
# include <cstdarg>
# include <iostream>

extern void	logDebug(const char *fmt, ...);
extern void logError(const std::string& message);
extern void logInfo(const std::string& message);
