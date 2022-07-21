#pragma once

#ifndef EMB_ENABLE_MIP
#define EMB_ENABLE_MIP 0
#endif

#ifndef EMB_ENABLE_POLL
#define EMB_ENABLE_POLL 0
#endif

#ifndef EMB_ENABLE_FATFS
#define EMB_ENABLE_FATFS 0
#endif

#ifndef EMB_ENABLE_SOCKET
#define EMB_ENABLE_SOCKET 1
#endif

#ifndef EMB_ENABLE_MBEDTLS
#define EMB_ENABLE_MBEDTLS 0
#endif

#ifndef EMB_ENABLE_OPENSSL
#define EMB_ENABLE_OPENSSL 0
#endif

#ifndef EMB_ENABLE_CUSTOM_TLS
#define EMB_ENABLE_CUSTOM_TLS 0
#endif

#ifndef EMB_ENABLE_SSI
#define EMB_ENABLE_SSI 0
#endif

#ifndef EMB_ENABLE_IPV6
#define EMB_ENABLE_IPV6 0
#endif

#ifndef EMB_ENABLE_MD5
#define EMB_ENABLE_MD5 0
#endif

// Set EMB_ENABLE_WINSOCK=0 for Win32 builds with external IP stack (like LWIP)
#ifndef EMB_ENABLE_WINSOCK
#define EMB_ENABLE_WINSOCK 1
#endif

#ifndef EMB_ENABLE_DIRLIST
#define EMB_ENABLE_DIRLIST 0
#endif

#ifndef EMB_ENABLE_CUSTOM_RANDOM
#define EMB_ENABLE_CUSTOM_RANDOM 0
#endif

#ifndef EMB_ENABLE_CUSTOM_MILLIS
#define EMB_ENABLE_CUSTOM_MILLIS 0
#endif

#ifndef EMB_ENABLE_PACKED_FS
#define EMB_ENABLE_PACKED_FS 0
#endif

// Granularity of the send/recv IO buffer growth
#ifndef EMB_IO_SIZE
#define EMB_IO_SIZE 2048
#endif

// Maximum size of the recv IO buffer
#ifndef EMB_MAX_RECV_SIZE
#define EMB_MAX_RECV_SIZE (3 * 1024 * 1024)
#endif

#ifndef EMB_MAX_HTTP_HEADERS
#define EMB_MAX_HTTP_HEADERS 40
#endif

#ifndef EMB_HTTP_INDEX
#define EMB_HTTP_INDEX "index.html"
#endif

#ifndef EMB_PATH_MAX
#ifdef PATH_MAX
#define EMB_PATH_MAX PATH_MAX
#else
#define EMB_PATH_MAX 128
#endif
#endif

#ifndef EMB_SOCK_LISTEN_BACKLOG_SIZE
#define EMB_SOCK_LISTEN_BACKLOG_SIZE 3
#endif

#ifndef EMB_DIRSEP
#define EMB_DIRSEP '/'
#endif

#ifndef EMB_ENABLE_FILE
#if defined(FOPEN_MAX)
#define EMB_ENABLE_FILE 1
#else
#define EMB_ENABLE_FILE 0
#endif
#endif

#ifndef EMB_PUTCHAR
#define EMB_PUTCHAR(x) putchar(x)
#endif
