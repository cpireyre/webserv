#pragma once

# include <chrono>

inline uint64_t now_ms()
{
    return duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
