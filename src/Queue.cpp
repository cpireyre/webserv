#include "Queue.hpp"
#include "Logger.hpp"

int	queue_create(void)
{
#ifdef __linux__
	// TODO(colin): epoll implementation
	Logger::warn("Linux support coming soon");
	return (-1);
#else
	int	qfd;

	qfd = kqueue();
	if (qfd < 0)
	{
		Logger::warn("Error creating kqueue");
		return (-1);
	}
	Logger::debug("Created event queue");
	return (qfd);
#endif
}

int	queue_add_fd(int qfd, int fd, enum queue_event_type t, const void *data)
{
	assert(qfd >= 0);
	assert(fd >= 0);
	assert(t == QUEUE_EVENT_READ || t == QUEUE_EVENT_WRITE);
#ifdef __linux__
	(void)data;
	// TODO(colin): epoll implementation
	Logger::warn("Linux support coming soon");
	return (-1);
#else
	struct kevent	e;
	int events = 0;
	switch (t) {
		case QUEUE_EVENT_READ:
			events |= EVFILT_READ;
			break;
		case QUEUE_EVENT_WRITE:
			events |= EVFILT_WRITE;
			break;
	}
	EV_SET(&e, fd, events, EV_ADD, 0, 0, (void *)data);
	if (kevent(qfd, &e, 1, NULL, 0, NULL) < 0)
	{
		Logger::warn("Error adding kevent");
		return (-1);
	}
	Logger::debug("Registered socket %d", fd);
	return (0);
#endif
}
