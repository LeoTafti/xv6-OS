#ifndef __INCLUDE_select_h
#define __INCLUDE_select_h

#include "param.h"

typedef unsigned int fd_set;
#define MAX_NFDS NOFILE

#define MAX_NB_SLEEPING 16

/* NOTE: static functions do not suffer from multiple definiton problems when
 * defined in headers, so this will compile.  In general you should only declare
 * very simple (e.g. one-line) functions in headers like this
 */
static inline void _fd_set(int fd, fd_set* set) {
  *set |= (1 << fd);
}

static inline int _fd_isset(int fd, fd_set* set) {
  return *set & (1 << fd);
}

static inline void _fd_clr(int fd, fd_set* set) {
  *set &= ~(1 << fd);
}

static inline void _fd_zero(fd_set* set) {
  *set = 0;
}

static inline int _fd_iszero(fd_set* set) {
  return *set == 0;
}

#define FD_SET(fd, set) _fd_set(fd, set)

#define FD_ISSET(fd, set) _fd_isset(fd,set)

#define FD_CLR(fd, set) _fd_clr(fd, set)

#define FD_ZERO(set) _fd_zero(set)

#define FD_ISZERO(set) _fd_iszero(set)

#endif  // __INCLUDE_select_h
