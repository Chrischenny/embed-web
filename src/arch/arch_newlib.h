#pragma once

#if EMB_ARCH == EMB_ARCH_NEWLIB
#define _POSIX_TIMERS

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define EMB_PATH_MAX 100
#define EMB_ENABLE_SOCKET 0
#define EMB_ENABLE_DIRLIST 0

#endif
