/*
 * This file is part of OSN freehttpd.
 *
 * Copyright (C) 2025  OSN Developers.
 *
 * OSN freehttpd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OSN freehttpd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with OSN freehttpd.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
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

#if !defined(__linux__)
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
		events[i].events = (kevents[i].filter == EVFILT_READ	? XPOLLIN
							: kevents[i].filter == EVFILT_WRITE ? XPOLLOUT
																: 0)
						   | (kevents[i].flags & EV_ERROR && kevents[i].data != 0 ? XPOLLERR : 0);
		events[i].data.fd = kevents[i].ident;
		events[i].kevent = kevents[i];
	}

	return nfds;
	#else /* defined (__APPLE__) || defined (__FreeBSD__) */
		#error "Unsupported platform"
	#endif /* not defined (__APPLE__) || defined (__FreeBSD__) */
}
#endif /* !defined(__linux__) */

bool
xpoll_add (xpoll_t xpoll, fd_t fd, uint32_t flags, uint32_t fdflags)
{
	int ret;

#if defined(__linux__)
	struct epoll_event eev = { .data.fd = fd, .events = flags };
	ret = epoll_ctl (xpoll, EPOLL_CTL_ADD, fd, &eev);
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

	ret = kevent (xpoll, &event, n, NULL, 0, NULL);
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
xpoll_mod (xpoll_t xpoll, fd_t fd, uint32_t flags)
{
	int ret;

#if defined(__linux__)
	struct epoll_event eev = { .data.fd = fd, .events = flags };
	ret = epoll_ctl (xpoll, EPOLL_CTL_MOD, fd, &eev);
#elif defined(__APPLE__) || defined(__FreeBSD__)
	struct kevent events[8];
	int n = 0;
	uint16_t ev_opt = EV_ADD | EV_ENABLE;
	int16_t filter = 0;

	if (flags & XPOLLET)
		ev_opt |= EV_CLEAR;

	EV_SET (&events[n++], fd, EVFILT_READ, flags & XPOLLIN ? ev_opt : EV_DELETE, 0, 0, NULL);
	EV_SET (&events[n++], fd, EVFILT_WRITE, flags & XPOLLOUT ? ev_opt : EV_DELETE, 0, 0, NULL);

	ret = kevent (xpoll, &event, n, NULL, 0, NULL);
#else /* defined (__APPLE__) || defined (__FreeBSD__) */
	#error "Unsupported platform"
#endif /* not defined (__APPLE__) || defined (__FreeBSD__) */

	return ret == 0;
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

	bool err = false;

	for (int i = 0; i < n; i++)
	{
		ret = kevent (xpoll, &event, 1, NULL, 0, NULL);

		if (ret < 0)
			err = true;
	}

	if (err)
		ret = -1;
#endif

	return ret == 0;
}

void
xpoll_destroy (xpoll_t xpoll)
{
	close (xpoll);
}

int
xpoll_get_error (xpoll_t xpoll_fd __attribute_maybe_unused__, const xevent_t *event __attribute_maybe_unused__, fd_t fd)
{
#ifdef __linux__
	int err = 0;
	socklen_t len = sizeof err;

	if (getsockopt (fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
		return -1;
	
	return err;
#else /* not __linux__ */
	if (event->kevent.flags & EV_ERROR)
		return event->kevent.data;

	return -1;
#endif /* __linux__ */
}