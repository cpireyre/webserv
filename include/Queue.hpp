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

constexpr int QUEUE_MAX_EVENTS = 64;

enum queue_event_type {
	QUEUE_EVENT_READ,
	QUEUE_EVENT_WRITE,
};

int		queue_create(void);
int		queue_add_fd(int qfd, int fd, enum queue_event_type t, const void *data);
int		queue_wait(int qfd, queue_event *events, int events_count);
void	*queue_event_get_data(const queue_event *e);
