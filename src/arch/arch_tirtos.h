#pragma once

#if EMB_ARCH == EMB_ARCH_TIRTOS

#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <time.h>
#include <errno.h>

#include <sys/socket.h>

extern int SockStatus(SOCKET hSock, int request, int *results );
extern int SockSet(SOCKET hSock, int Type, int Prop, void *pbuf, int size);
#define EMB_SOCK_ERRNO errno
#define closesocket(x) close(x)

#endif
