#pragma once

#include <cassert>

#ifdef __linux__
	#include <sys/epoll.h>
	typedef struct epoll_event queue_event;
#else
	#include <sys/types.h>
	#include <sys/event.h>
	#include <sys/time.h>
	typedef struct kevent queue_event;
#endif

# define QUEUE_MAX_EVENTS 64

enum queue_event_type {
	QUEUE_EVENT_READ,
	QUEUE_EVENT_WRITE,
};

int	queue_create(void);
int	queue_add_fd(int qfd, int fd, enum queue_event_type t, const void *data);
