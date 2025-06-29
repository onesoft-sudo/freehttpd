#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "compat.h"
#include "xpoll.h"

#if defined(__linux__)
	#include <sys/epoll.h>

static_assert (sizeof (xevent_t) == sizeof (struct epoll_event), "Invalid struct xevent");
#elif defined(__APPLE__) || defined(__FreeBSD__)
	#include <sys/event.h>
#else /* defined (__APPLE__) || defined (__FreeBSD__) */
	#error "Unsupported platform"
#endif /* not defined (__APPLE__) || defined (__FreeBSD__) */

#undef xpoll_wait

xpoll_t
xpoll_create (void)
{
#if defined(__linux__)
	return epoll_create1 (0);
#elif defined(__APPLE__) || defined(__FreeBSD__)
	return kqueue ();
#else /* defined (__APPLE__) || defined (__FreeBSD__) */
	#error "Unsupported platform"
#endif /* not defined (__APPLE__) || defined (__FreeBSD__) */
}

int
xpoll_wait (xpoll_t xpoll, xevent_t *events, int max_events, int timeout)
{
#if defined(__linux__)
	return epoll_wait (xpoll, events, max_events, timeout);
#elif defined(__APPLE__) || defined(__FreeBSD__)
	struct kevent kevents[max_events];
	const struct timespec ts = {
		.tv_sec = timeout / 1000,
		.tv_nsec = (timeout % 1000) * 1000000,
	};

	int nfds = kevent (xpoll, NULL, 0, kevents, max_events, &ts);

	if (nfds <= 0)
		return nfds;

	for (int i = 0; i < nfds; i++)
	{
		events[i].events = (uint32_t) kevents[i].filter;
		events[i].data.fd = kevents[i].ident;
	}

	return nfds;
#else /* defined (__APPLE__) || defined (__FreeBSD__) */
	#error "Unsupported platform"
#endif /* not defined (__APPLE__) || defined (__FreeBSD__) */
}

bool
xpoll_add (xpoll_t xpoll, fd_t fd, uint32_t flags, uint32_t fdflags)
{
	int ret;

#if defined(__linux__)
	struct epoll_event eev = { .data.fd = fd, .events = flags };
	ret = epoll_ctl (xpoll, EPOLL_CTL_ADD, fd, &eev) < 0;
#elif defined(__APPLE__) || defined(__FreeBSD__)
	struct kevent events[8];
	int n = 0;
	uint16_t ev_opt = EV_ADD | EV_ENABLE;
	int16_t filter = 0;

	if (flags & XPOLLET)
		ev_opt |= EV_CLEAR;

	if (flags & XPOLLIN)
		EV_SET (&events[n++], fd, EVFILT_READ, ev_opt, 0, 0, NULL);

	if (flags & XPOLLOUT)
		EV_SET (&events[n++], fd, EVFILT_WRITE, ev_opt, 0, 0, NULL);

	for (int i = 0; i < n; i++)
	{
		ret = kevent (xpoll, &event, 1, NULL, 0, NULL);

		if (ret < 0)
			return false;
	}
#else /* defined (__APPLE__) || defined (__FreeBSD__) */
	#error "Unsupported platform"
#endif /* not defined (__APPLE__) || defined (__FreeBSD__) */

	if (fdflags == 0)
		return ret == 0;

	if (ret < 0)
		return false;

	int existing_fdflags = fcntl (fd, F_GETFL);

	if (existing_fdflags < 0)
		return false;

	if (fcntl (fd, F_SETFL, existing_fdflags | fdflags) < 0)
		return false;

	return true;
}

bool
xpoll_del (xpoll_t xpoll, fd_t fd, uint32_t flags __attribute_maybe_unused__)
{
	int ret;

#if defined(__linux__)
	ret = epoll_ctl (xpoll, EPOLL_CTL_DEL, fd, NULL);
#elif defined(__APPLE__) || defined(__FreeBSD__)
	struct kevent events[8];
	int n = 0;
	uint16_t ev_opt = EV_DELETE;
	int16_t filter = 0;

	if (flags & XPOLLIN)
		EV_SET (&events[n++], fd, EVFILT_READ, ev_opt, 0, 0, NULL);

	if (flags & XPOLLOUT)
		EV_SET (&events[n++], fd, EVFILT_WRITE, ev_opt, 0, 0, NULL);

	for (int i = 0; i < n; i++)
	{
		ret = kevent (xpoll, &event, 1, NULL, 0, NULL);

		if (ret < 0)
			return false;
	}
#endif

	return ret == 0;
}

void
xpoll_destroy (xpoll_t xpoll)
{
	close (xpoll);
}
