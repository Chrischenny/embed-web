/**
 * @file arch_unix.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-07-16
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __ARCH_UNIX_H__
#define __ARCH_UNIX_H__

#ifdef __cplusplus
extern "C" {
#endif

#if EMB_ARCH == EMB_ARCH_UNIX

#define _DARWIN_UNLIMITED_SELECT 1  // No limit on file descriptors

#if !defined(EMB_ENABLE_POLL) && (defined(__linux__) || defined(__APPLE__))
#define EMB_ENABLE_POLL 1
#endif

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(EMB_ENABLE_POLL) && EMB_ENABLE_POLL
#include <poll.h>
#else
#include <sys/select.h>
#endif
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef EMB_ENABLE_DIRLIST
#define EMB_ENABLE_DIRLIST 1
#endif


#define EMB_SOCK_ERRNO errno
#ifndef closesocket
#define closesocket(x) close(x)
#endif
#define INVALID_SOCKET (-1)
typedef int SOCKET;

#endif

#ifdef __cplusplus
}
#endif
#endif // __ARCH_UNIX_H__


