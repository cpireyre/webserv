#include "Queue.hpp"
#include "Logger.hpp"

int	queue_create(void)
{
	int	qfd;

	qfd = kqueue();
	if (qfd < 0)
	{
		Logger::warn("Error creating kqueue");
		return (-1);
	}
	Logger::debug("Created event queue");
	return (qfd);
}

int	queue_add_fd(int qfd, int fd, enum queue_event_type t, const void *data)
{
	assert(qfd >= 0);
	assert(fd >= 0);
	struct kevent	e;
	int events = 0;
	events |= EVFILT_READ;
	EV_SET(&e, fd, events, EV_ADD, 0, 0, (void *)data);
	if (kevent(qdf, &e, 1, NULL, 0, NULL) < 0)
	{
		Logger::warn("Error adding kevent");
		return (-1);
	}
	return (0);
}
