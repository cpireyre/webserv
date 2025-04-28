#include "Queue.hpp"
#include "Logger.hpp"
#include <cerrno>

int	queue_create(void)
{
	int	qfd;

#ifdef __linux__
	qfd = epoll_create1(0);
	if (qfd < 0)
	{
		logDebug("Error: epoll_create1");
		return (-1);
	}
#else
	qfd = kqueue();
	if (qfd < 0)
	{
		logError("Error creating kqueue");
		return (-1);
	}
#endif

	assert(qfd >= 0);
	return (qfd);
}

int	queue_add_fd(int qfd, int fd, enum queue_event_type t, const void *data)
{
	assert(qfd >= 0);
	assert(fd >= 0);
	assert(t == READABLE || t == WRITABLE);

#ifdef __linux__
	struct epoll_event	e;
	memset(&e, 0, sizeof(e));
	switch (t) {
		case READABLE:
			e.events |= EPOLLIN;
			break;
		case WRITABLE:
			e.events |= EPOLLOUT;
			break;
	}
	e.data.ptr = (void*) data;
	if (epoll_ctl(qfd, EPOLL_CTL_ADD, fd, &e) < 0)
	{
		perror("epoll");
		logDebug("Error adding epoll event");
		return (-1);
	}
#else
	struct kevent	e;
	int events = 0;
	switch (t) {
		case READABLE:
			events |= EVFILT_READ;
			break;
		case WRITABLE:
			events |= EVFILT_WRITE;
			break;
	}
	EV_SET(&e, fd, events, EV_ADD, 0, 0, (void *)data);
	if (kevent(qfd, &e, 1, NULL, 0, NULL) < 0)
	{
		logError("Error adding kevent");
		return (-1);
	}
#endif

	return (0);
}

int	queue_mod_fd(int qfd, int fd, enum queue_event_type t, const void *data)
{
	assert(qfd >= 0);
	assert(fd >= 0);
	assert(t == READABLE || t == WRITABLE);

#ifdef __linux__
	struct epoll_event e;
	memset(&e, 0, sizeof(e));
	e.events = 0;

	switch (t) {
		case READABLE:
			e.events |= EPOLLIN;
			break;
		case WRITABLE:
			e.events |= EPOLLOUT;
			break;
	}
	e.data.ptr = (void *)data;
	if (epoll_ctl(qfd, EPOLL_CTL_MOD, fd, &e) < 0)
	{
		logDebug("Error in epoll_ctl");
		return (-1);
	}
#else
	struct kevent ev[2];
	int n = 0;

	switch (t) {
		case READABLE:
			EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD, 0, 0, (void *)data);
			EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
			break;
		case WRITABLE:
			EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD, 0, 0, (void *)data);
			EV_SET(&ev[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
			break;
	}

	if (kevent(qfd, ev, n, NULL, 0, NULL) < 0)
	{
		perror("kevent");
		return (-1);
	}

#endif
	return (0);
}

int	queue_rem_fd(int qfd, int fd)
{
	assert(qfd >= 0);
	assert(fd >= 0);

#ifdef __linux__
	struct epoll_event e;

	if (epoll_ctl(qfd, EPOLL_CTL_DEL, fd, &e) < 0)
	{
		logDebug("Error deleting fd: epoll_ctl");
		return (-1);
	}
#else
	struct kevent e[2];
	struct kevent result[2];

	EV_SET(&e[0], fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
	EV_SET(&e[1], fd, EVFILT_READ, EV_DELETE, 0, 0, 0);

	if (kevent(qfd, e, 2, result, 2, NULL) < 0)
	{
		if (errno == ENOENT)
			return (0);
		logError("Error deleting fd: kevent");
		return (-1);
	}

#endif
	return (0);
}

int	queue_wait(int qfd, queue_event *events, int events_count)
{
	assert(qfd >= 0);
	assert(events != NULL);
	assert(events_count > 0);
	int	nready = 0;

#ifdef __linux__
	nready = epoll_wait(qfd, events, events_count, 1000);
	if (nready < 0)
	{
		logDebug("Error: epoll_wait");
		return (-1);
	}
#else
	//TODO mac time out
	struct timespec timeout = {
		.tv_sec = 1,
		.tv_nsec = 0,
	};
	nready = kevent(qfd, NULL, 0, events, events_count, &timeout);
	if (nready < 0)
	{
		if (errno != EINTR)
			logError("Error: kevent");
		return (-1);
	}
	if (0) /* Show kevent debug information */
	{
		for (int i = 0; i < nready; ++i)
		{
			const struct kevent *e = &events[i];

			const char *filter_str = "?";
			if (e->filter == EVFILT_READ)
				filter_str = "READ";
			else if (e->filter == EVFILT_WRITE)
				filter_str = "WRITE";

			logDebug("[kevent] fd=%lu filter=%s flags=0x%x data=%ld udata=%p",
					(unsigned long)e->ident,
					filter_str,
					e->flags,
					(long)e->data,
					e->udata);
		}
	}
#endif

	return (nready);
}

void	*queue_event_get_data(const queue_event *e)
{
	assert(e != NULL);
#ifdef __linux__
	return (e->data.ptr);
#else
	return (e->udata);
#endif
}

bool	queue_event_is_error(const queue_event *e)
{
#ifdef __linux__
	return (e->events & ~(EPOLLIN | EPOLLOUT)) ? true : false;
#else
	return (e->flags & EV_EOF) ? true : false;
#endif
}

queue_event_type	queue_event_get_type(const queue_event *e)
{
#ifdef __linux__
	if (e->events & EPOLLIN)
		return (READABLE);
	else
		return (WRITABLE);
#else
	if (e->filter == EVFILT_READ)
		return (READABLE);
	else
		return (WRITABLE);
#endif
}
