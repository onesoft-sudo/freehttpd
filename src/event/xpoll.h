#ifndef FH_XPOLL_H
#define FH_XPOLL_H

#include <stdbool.h>

#include "types.h"

#if defined (__linux__)
#include <sys/epoll.h>

typedef struct epoll_event xevent_t;
#elif defined (__APPLE__) || defined (__FreeBSD__)
#include <sys/event.h>

union xevent_data
{
    void *ptr;
    fd_t fd;
};

struct xevent
{
    uint32_t events;
    union xevent_data data;
    struct kevent *kevent;
};

typedef struct xevent xevent_t;
#endif /* defined (__APPLE__) || defined (__FreeBSD__) */

enum xpoll_evflags
{
#if defined (__linux__)
    XPOLLIN = EPOLLIN,
    XPOLLOUT = EPOLLOUT,
    XPOLLET = EPOLLET,
    XPOLLHUP = EPOLLHUP,
    XPOLLRDHUP = EPOLLRDHUP,
#elif defined (__APPLE__) || defined (__FreeBSD__)
    XPOLLIN = 0x1,
    XPOLLOUT = 0x2,
    XPOLLET = 0x4,
    XPOLLHUP = 0,
    XPOLLRDHUP = 0,
#endif /* defined (__APPLE__) || defined (__FreeBSD__) */
};

typedef int xpoll_t;

xpoll_t xpoll_create (void);
void xpoll_destroy (xpoll_t xpoll);
bool xpoll_add (xpoll_t xpoll, fd_t fd, uint32_t flags, uint32_t fdflags);
bool xpoll_del (xpoll_t xpoll, fd_t fd, uint32_t flags);

#if defined (__linux__)
#define xpoll_wait epoll_wait
#else /* not defined (__linux__) */
int xpoll_wait (xpoll_t xpoll, xevent_t *events, int max_events, int timeout);
#endif /* defined (__linux__) */

#endif /* FH_XPOLL_H */
