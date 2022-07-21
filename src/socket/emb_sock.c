// #include "dns.h"
// #include "event.h"
// #include "log.h"
#include "emb_svr.h"
#include "str.h"
// #include "tls.h"
#include "url.h"
#include "util/util.h"



#define FD(c_) ((SOCKET) (size_t) (c_)->fd)
#define S2PTR(s_) ((void *) (size_t) (s_))

#ifndef MSG_NONBLOCKING
#define MSG_NONBLOCKING 0
#endif

#ifndef AF_INET6
#define AF_INET6 10
#endif

union emb_sa {
  struct sockaddr sa;
  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;
};

static socklen_t addr_to_union_sa(struct emb_addr *a, union emb_sa *usa) {
  socklen_t len = sizeof(usa->sin);
  memset(usa, 0, sizeof(*usa));
  usa->sin.sin_family = AF_INET;
  usa->sin.sin_port = a->port;

  *(uint32_t *) &usa->sin.sin_addr = a->ip;
  if (a->is_ip6) {
    usa->sin.sin_family = AF_INET6;
    usa->sin6.sin6_port = a->port;
    memcpy(&usa->sin6.sin6_addr, a->ip6, sizeof(a->ip6));
    len = sizeof(usa->sin6);
  }
  return len;
}

static void emb_set_non_blocking_mode(SOCKET fd) {
#if defined(EMB_CUSTOM_NONBLOCK)
  EMB_CUSTOM_NONBLOCK(fd);
#elif EMB_ARCH == EMB_ARCH_WIN32 && EMB_ENABLE_WINSOCK
  unsigned long on = 1;
  ioctlsocket(fd, FIONBIO, &on);
#elif EMB_ARCH == EMB_ARCH_RTX
  unsigned long on = 1;
  ioctlsocket(fd, FIONBIO, &on);
#elif EMB_ARCH == EMB_ARCH_FREERTOS_TCP
  const BaseType_t off = 0;
  if (setsockopt(fd, 0, FREERTOS_SO_RCVTIMEO, &off, sizeof(off)) != 0) (void) 0;
  if (setsockopt(fd, 0, FREERTOS_SO_SNDTIMEO, &off, sizeof(off)) != 0) (void) 0;
#elif EMB_ARCH == EMB_ARCH_FREERTOS_LWIP || EMB_ARCH == EMB_ARCH_RTX_LWIP
  lwip_fcntl(fd, F_SETFL, O_NONBLOCK);
#elif EMB_ARCH == EMB_ARCH_AZURERTOS
  fcntl(fd, F_SETFL, O_NONBLOCK);
#elif EMB_ARCH == EMB_ARCH_TIRTOS
  int val = 0;
  setsockopt(fd, 0, SO_BLOCKING, &val, sizeof(val));
  int status = 0;
  int res = SockStatus(fd, FDSTATUS_SEND, &status);
  if (res == 0 && status > 0) {
    val = status / 2;
    int val_size = sizeof(val);
    res = SockSet(fd, SOL_SOCKET, SO_SNDLOWAT, &val, val_size);
  }
#else
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);  // Non-blocking mode
  fcntl(fd, F_SETFD, FD_CLOEXEC);                          // Set close-on-exec
#endif
}

bool emb_open_listener(struct emb_connection *c, const char *url) {
  SOCKET fd = INVALID_SOCKET;
  bool success = false;
  c->loc.port = emb_htons(emb_url_port(url));
  if (!emb_aton(emb_url_host(url), &c->loc)) {
    EMB_ERROR(("invalid listening URL: %s", url));
  } else {
    union emb_sa usa;
    socklen_t slen = addr_to_union_sa(&c->loc, &usa);
    int on = 1, af = c->loc.is_ip6 ? AF_INET6 : AF_INET;
    int type = strncmp(url, "udp:", 4) == 0 ? SOCK_DGRAM : SOCK_STREAM;
    int proto = type == SOCK_DGRAM ? IPPROTO_UDP : IPPROTO_TCP;
    (void) on;

    if ((fd = socket(af, type, proto)) == INVALID_SOCKET) {
      EMB_ERROR(("socket: %d", EMB_SOCK_ERRNO));
#if ((EMB_ARCH == EMB_ARCH_WIN32) || (EMB_ARCH == EMB_ARCH_UNIX) || \
     (defined(LWIP_SOCKET) && SO_REUSE == 1))
    } else if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &on,
                          sizeof(on)) != 0) {
      // 1. SO_RESUSEADDR is not enabled on Windows because the semantics of
      //    SO_REUSEADDR on UNIX and Windows is different. On Windows,
      //    SO_REUSEADDR allows to bind a socket to a port without error even
      //    if the port is already open by another program. This is not the
      //    behavior SO_REUSEADDR was designed for, and leads to hard-to-track
      //    failure scenarios. Therefore, SO_REUSEADDR was disabled on Windows
      //    unless SO_EXCLUSIVEADDRUSE is supported and set on a socket.
      // 2. In case of LWIP, SO_REUSEADDR should be explicitly enabled, by
      // defining
      //    SO_REUSE (in lwipopts.h), otherwise the code below will compile
      //    but won't work! (setsockopt will return EINVAL)
      EMB_ERROR(("reuseaddr: %d", EMB_SOCK_ERRNO));
#endif
#if EMB_ARCH == EMB_ARCH_WIN32 && !defined(SO_EXCLUSIVEADDRUSE) && !defined(WINCE)
    } else if (setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *) &on,
                          sizeof(on)) != 0) {
      // "Using SO_REUSEADDR and SO_EXCLUSIVEADDRUSE"
      EMB_ERROR(("exclusiveaddruse: %d", EMB_SOCK_ERRNO));
#endif
    } else if (bind(fd, &usa.sa, slen) != 0) {
      EMB_ERROR(("bind: %d", EMB_SOCK_ERRNO));
    } else if ((type == SOCK_STREAM &&
                listen(fd, EMB_SOCK_LISTEN_BACKLOG_SIZE) != 0)) {
      // NOTE(lsm): FreeRTOS uses backlog value as a connection limit
      // In case port was set to 0, get the real port number
      EMB_ERROR(("listen: %d", EMB_SOCK_ERRNO));
    } else {
      setlocaddr(fd, &c->loc);
      emb_set_non_blocking_mode(fd);
      c->fd = S2PTR(fd);
      success = true;
    }
  }
  if (success == false && fd != INVALID_SOCKET) closesocket(fd);
  return success;
}