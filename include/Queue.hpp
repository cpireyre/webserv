#pragma once

#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <cassert>
typedef struct kevent queue_event;
int	queue_create(void);
