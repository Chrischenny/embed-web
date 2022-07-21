#pragma once

#if EMB_ARCH == EMB_ARCH_ZEPHYR

#include <zephyr.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <net/socket.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#define EMB_PUTCHAR(x) printk("%c", x)
#define strerror(x) zsock_gai_strerror(x)
#define FD_CLOEXEC 0
#define F_SETFD 0
#define EMB_ENABLE_SSI 0

int rand(void);
int sscanf(const char *, const char *, ...);

#endif
