#include "dns.h"
#include "event.h"
#include "log.h"
#include "net.h"
#include "str.h"
#include "timer.h"
#include "tls.h"
#include "url.h"
#include "util.h"

#if EMB_ENABLE_SOCKET
#if EMB_ARCH == EMB_ARCH_WIN32 && EMB_ENABLE_WINSOCK
#define EMB_SOCK_ERRNO WSAGetLastError()
#ifndef SO_EXCLUSIVEADDRUSE
#define SO_EXCLUSIVEADDRUSE ((int) (~SO_REUSEADDR))
#pragma comment(lib, "ws2_32.lib")
#endif
#elif EMB_ARCH == EMB_ARCH_FREERTOS_TCP
#define EMB_SOCK_ERRNO errno
typedef Socket_t SOCKET;
#define INVALID_SOCKET FREERTOS_INVALID_SOCKET
#elif EMB_ARCH == EMB_ARCH_TIRTOS
#define EMB_SOCK_ERRNO errno
#define closesocket(x) close(x)
#else
#define EMB_SOCK_ERRNO errno
#ifndef closesocket
#define closesocket(x) close(x)
#endif
#define INVALID_SOCKET (-1)
typedef int SOCKET;
#endif

#define FD(c_) ((SOCKET) (size_t) (c_)->fd)
#define S2PTR(s_) ((void *) (size_t) (s_))

#ifndef MSG_NONBLOCKING
#define MSG_NONBLOCKING 0
#endif

#ifndef AF_INET6
#define AF_INET6 10
#endif

union usa {
  struct sockaddr sa;
  struct sockaddr_in sin;
#if EMB_ENABLE_IPV6
  struct sockaddr_in6 sin6;
#endif
};

static socklen_t tousa(struct emb_addr *a, union usa *usa) {
  socklen_t len = sizeof(usa->sin);
  memset(usa, 0, sizeof(*usa));
  usa->sin.sin_family = AF_INET;
  usa->sin.sin_port = a->port;
  *(uint32_t *) &usa->sin.sin_addr = a->ip;
#if EMB_ENABLE_IPV6
  if (a->is_ip6) {
    usa->sin.sin_family = AF_INET6;
    usa->sin6.sin6_port = a->port;
    memcpy(&usa->sin6.sin6_addr, a->ip6, sizeof(a->ip6));
    len = sizeof(usa->sin6);
  }
#endif
  return len;
}

static void tomgaddr(union usa *usa, struct emb_addr *a, bool is_ip6) {
  a->is_ip6 = is_ip6;
  a->port = usa->sin.sin_port;
  memcpy(&a->ip, &usa->sin.sin_addr, sizeof(a->ip));
#if EMB_ENABLE_IPV6
  if (is_ip6) {
    memcpy(a->ip6, &usa->sin6.sin6_addr, sizeof(a->ip6));
    a->port = usa->sin6.sin6_port;
  }
#endif
}

bool emb_sock_would_block(void);
bool emb_sock_would_block(void) {
  int err = EMB_SOCK_ERRNO;
  return err == EINPROGRESS || err == EWOULDBLOCK
#ifndef WINCE
         || err == EAGAIN || err == EINTR
#endif
#if EMB_ARCH == EMB_ARCH_WIN32 && EMB_ENABLE_WINSOCK
         || err == WSAEINTR || err == WSAEWOULDBLOCK
#endif
      ;
}

bool emb_sock_conn_reset(void);
bool emb_sock_conn_reset(void) {
  int err = EMB_SOCK_ERRNO;
#if EMB_ARCH == EMB_ARCH_WIN32 && EMB_ENABLE_WINSOCK
  return err == WSAECONNRESET;
#else
  return err == EPIPE || err == ECONNRESET;
#endif
}

static void setlocaddr(SOCKET fd, struct emb_addr *addr) {
  union usa usa;
  socklen_t n = sizeof(usa);
  if (getsockname(fd, &usa.sa, &n) == 0) {
    tomgaddr(&usa, addr, n != sizeof(usa.sin));
  }
}

static void iolog(struct emb_connection *c, char *buf, long n, bool r) {
  if (n == 0) {
    // Do nothing
  } else if (n < 0) {
    c->is_closing = 1;  // Termination. Don't call emb_error(): #1529
  } else if (n > 0) {
    if (c->is_hexdumping) {
      union usa usa;
      char t1[50] = "", t2[50] = "";
      socklen_t slen = sizeof(usa.sin);
      struct emb_addr a;
      memset(&usa, 0, sizeof(usa));
      memset(&a, 0, sizeof(a));
      if (getsockname(FD(c), &usa.sa, &slen) < 0) (void) 0;  // Ignore result
      tomgaddr(&usa, &a, c->rem.is_ip6);
      EMB_INFO(("\n-- %lu %s %s %s %s %ld", c->id,
               emb_straddr(&a, t1, sizeof(t1)), r ? "<-" : "->",
               emb_straddr(&c->rem, t2, sizeof(t2)), c->label, n));

      emb_hexdump(buf, (size_t) n);
    }
    if (r) {
      struct emb_str evd = emb_str_n(buf, (size_t) n);
      c->recv.len += (size_t) n;
      emb_call(c, EMB_EV_READ, &evd);
    } else {
      emb_iobuf_del(&c->send, 0, (size_t) n);
      // if (c->send.len == 0) emb_iobuf_resize(&c->send, 0);
      emb_call(c, EMB_EV_WRITE, &n);
    }
  }
}

static long emb_sock_send(struct emb_connection *c, const void *buf, size_t len) {
  long n;
  if (c->is_udp) {
    union usa usa;
    socklen_t slen = tousa(&c->rem, &usa);
    n = sendto(FD(c), (char *) buf, len, 0, &usa.sa, slen);
    if (n > 0) setlocaddr(FD(c), &c->loc);
  } else {
    n = send(FD(c), (char *) buf, len, MSG_NONBLOCKING);
#if EMB_ARCH == EMB_ARCH_RTX
    if (n == BSD_EWOULDBLOCK) return 0;
#endif
  }
  return n == 0 ? -1 : n < 0 && emb_sock_would_block() ? 0 : n;
}

bool emb_send(struct emb_connection *c, const void *buf, size_t len) {
  if (c->is_udp) {
    long n = emb_sock_send(c, buf, len);
    EMB_DEBUG(("%lu %p %d:%d %ld err %d", c->id, c->fd, (int) c->send.len,
              (int) c->recv.len, n, EMB_SOCK_ERRNO));
    iolog(c, (char *) buf, n, false);
    return n > 0;
  } else {
    return emb_iobuf_add(&c->send, c->send.len, buf, len, EMB_IO_SIZE);
  }
}


static long emb_sock_recv(struct emb_connection *c, void *buf, size_t len) {
  long n = 0;
  if (c->is_udp) {
    union usa usa;
    socklen_t slen = tousa(&c->rem, &usa);
    n = recvfrom(FD(c), (char *) buf, len, 0, &usa.sa, &slen);
    if (n > 0) tomgaddr(&usa, &c->rem, slen != sizeof(usa.sin));
  } else {
    n = recv(FD(c), (char *) buf, len, MSG_NONBLOCKING);
  }
  return n == 0 ? -1 : n < 0 && emb_sock_would_block() ? 0 : n;
}

// NOTE(lsm): do only one iteration of reads, cause some systems
// (e.g. FreeRTOS stack) return 0 instead of -1/EWOULDBLOCK when no data
static void read_conn(struct emb_connection *c) {
  long n = -1;
  if (c->recv.len >= EMB_MAX_RECV_SIZE) {
    emb_error(c, "max_recv_buf_size reached");
  } else if (c->recv.size <= c->recv.len &&
             !emb_iobuf_resize(&c->recv, c->recv.size + EMB_IO_SIZE)) {
    emb_error(c, "oom");
  } else {
    char *buf = (char *) &c->recv.buf[c->recv.len];
    size_t len = c->recv.size - c->recv.len;
    n = c->is_tls ? emb_tls_recv(c, buf, len) : emb_sock_recv(c, buf, len);
    EMB_DEBUG(("%lu %p %d:%d %ld err %d", c->id, c->fd, (int) c->send.len,
              (int) c->recv.len, n, EMB_SOCK_ERRNO));
    iolog(c, buf, n, true);
  }
}

static void write_conn(struct emb_connection *c) {
  char *buf = (char *) c->send.buf;
  size_t len = c->send.len;
  long n = c->is_tls ? emb_tls_send(c, buf, len) : emb_sock_send(c, buf, len);
  EMB_DEBUG(("%lu %p %d:%d %ld err %d", c->id, c->fd, (int) c->send.len,
            (int) c->recv.len, n, EMB_SOCK_ERRNO));
  iolog(c, buf, n, false);
}

static void close_conn(struct emb_connection *c) {
  if (FD(c) != INVALID_SOCKET) {
    closesocket(FD(c));
#if EMB_ARCH == EMB_ARCH_FREERTOS_TCP
    FreeRTOS_FD_CLR(c->fd, c->mgr->ss, eSELECT_ALL);
#endif
    c->fd = NULL;
  }
  emb_close_conn(c);
}

static void setsockopts(struct emb_connection *c) {
#if EMB_ARCH == EMB_ARCH_FREERTOS_TCP || EMB_ARCH == EMB_ARCH_AZURERTOS || \
    EMB_ARCH == EMB_ARCH_TIRTOS
  (void) c;
#else
  int on = 1;
#if !defined(SOL_TCP)
#define SOL_TCP IPPROTO_TCP
#endif
  if (setsockopt(FD(c), SOL_TCP, TCP_NODELAY, (char *) &on, sizeof(on)) != 0)
    (void) 0;
  if (setsockopt(FD(c), SOL_SOCKET, SO_KEEPALIVE, (char *) &on, sizeof(on)) !=
      0)
    (void) 0;
#endif
}

void emb_connect_resolved(struct emb_connection *c) {
  // char buf[40];
  int type = c->is_udp ? SOCK_DGRAM : SOCK_STREAM;
  int rc, af = c->rem.is_ip6 ? AF_INET6 : AF_INET;
  // emb_straddr(&c->rem, buf, sizeof(buf));
  c->fd = S2PTR(socket(af, type, 0));
  c->is_resolving = 0;
  if (FD(c) == INVALID_SOCKET) {
    emb_error(c, "socket(): %d", EMB_SOCK_ERRNO);
  } else if (c->is_udp) {
    emb_call(c, EMB_EV_RESOLVE, NULL);
    emb_call(c, EMB_EV_CONNECT, NULL);
  } else {
    union usa usa;
    socklen_t slen = tousa(&c->rem, &usa);
    emb_set_non_blocking_mode(FD(c));
    setsockopts(c);
    emb_call(c, EMB_EV_RESOLVE, NULL);
    if ((rc = connect(FD(c), &usa.sa, slen)) == 0) {
      emb_call(c, EMB_EV_CONNECT, NULL);
    } else if (emb_sock_would_block()) {
      EMB_DEBUG(("%lu %p connect in progress...", c->id, c->fd));
      c->is_connecting = 1;
    } else {
      emb_error(c, "connect: %d", EMB_SOCK_ERRNO);
    }
  }
}

static SOCKET raccept(SOCKET sock, union usa *usa, socklen_t len) {
  SOCKET s = INVALID_SOCKET;
  do {
    memset(usa, 0, sizeof(*usa));
    s = accept(sock, &usa->sa, &len);
  } while (s == INVALID_SOCKET && errno == EINTR);
  return s;
}

static void accept_conn(struct emb_mgr *mgr, struct emb_connection *lsn) {
  struct emb_connection *c = NULL;
  union usa usa;
  socklen_t sa_len = sizeof(usa);
  SOCKET fd = raccept(FD(lsn), &usa, sa_len);
  if (fd == INVALID_SOCKET) {
#if EMB_ARCH == EMB_ARCH_AZURERTOS
    // AzureRTOS, in non-block socket mode can mark listening socket readable
    // even it is not. See comment for 'select' func implementation in
    // nx_bsd.c That's not an error, just should try later
    if (EMB_SOCK_ERRNO != EAGAIN)
#endif
      EMB_ERROR(("%lu accept failed, errno %d", lsn->id, EMB_SOCK_ERRNO));
#if (EMB_ARCH != EMB_ARCH_WIN32) && (EMB_ARCH != EMB_ARCH_FREERTOS_TCP) && \
    (EMB_ARCH != EMB_ARCH_TIRTOS) && !(EMB_ENABLE_POLL)
  } else if ((long) fd >= FD_SETSIZE) {
    EMB_ERROR(("%ld > %ld", (long) fd, (long) FD_SETSIZE));
    closesocket(fd);
#endif
  } else if ((c = emb_alloc_conn(mgr)) == NULL) {
    EMB_ERROR(("%lu OOM", lsn->id));
    closesocket(fd);
  } else {
    char buf[40];
    tomgaddr(&usa, &c->rem, sa_len != sizeof(usa.sin));
    emb_straddr(&c->rem, buf, sizeof(buf));
    EMB_DEBUG(("%lu accepted %s", c->id, buf));
    LIST_ADD_HEAD(struct emb_connection, &mgr->conns, c);
    c->fd = S2PTR(fd);
    emb_set_non_blocking_mode(FD(c));
    setsockopts(c);
    c->is_accepted = 1;
    c->is_hexdumping = lsn->is_hexdumping;
    c->loc = lsn->loc;
    c->pfn = lsn->pfn;
    c->pfn_data = lsn->pfn_data;
    c->fn = lsn->fn;
    c->fn_data = lsn->fn_data;
    emb_call(c, EMB_EV_OPEN, NULL);
    emb_call(c, EMB_EV_ACCEPT, NULL);
  }
}

static bool emb_socketpair(SOCKET sp[2], union usa usa[2], bool udp) {
  SOCKET sock;
  socklen_t n = sizeof(usa[0].sin);
  bool success = false;

  sock = sp[0] = sp[1] = INVALID_SOCKET;
  (void) memset(&usa[0], 0, sizeof(usa[0]));
  usa[0].sin.sin_family = AF_INET;
  *(uint32_t *) &usa->sin.sin_addr = emb_htonl(0x7f000001U);  // 127.0.0.1
  usa[1] = usa[0];

  if (udp && (sp[0] = socket(AF_INET, SOCK_DGRAM, 0)) != INVALID_SOCKET &&
      (sp[1] = socket(AF_INET, SOCK_DGRAM, 0)) != INVALID_SOCKET &&
      bind(sp[0], &usa[0].sa, n) == 0 && bind(sp[1], &usa[1].sa, n) == 0 &&
      getsockname(sp[0], &usa[0].sa, &n) == 0 &&
      getsockname(sp[1], &usa[1].sa, &n) == 0 &&
      connect(sp[0], &usa[1].sa, n) == 0 &&
      connect(sp[1], &usa[0].sa, n) == 0) {
    success = true;
  } else if (!udp &&
             (sock = socket(AF_INET, SOCK_STREAM, 0)) != INVALID_SOCKET &&
             bind(sock, &usa[0].sa, n) == 0 &&
             listen(sock, EMB_SOCK_LISTEN_BACKLOG_SIZE) == 0 &&
             getsockname(sock, &usa[0].sa, &n) == 0 &&
             (sp[0] = socket(AF_INET, SOCK_STREAM, 0)) != INVALID_SOCKET &&
             connect(sp[0], &usa[0].sa, n) == 0 &&
             (sp[1] = raccept(sock, &usa[1], n)) != INVALID_SOCKET) {
    success = true;
  }
  if (success) {
    emb_set_non_blocking_mode(sp[1]);
  } else {
    if (sp[0] != INVALID_SOCKET) closesocket(sp[0]);
    if (sp[1] != INVALID_SOCKET) closesocket(sp[1]);
    sp[0] = sp[1] = INVALID_SOCKET;
  }
  if (sock != INVALID_SOCKET) closesocket(sock);
  return success;
}

int emb_mkpipe(struct emb_mgr *mgr, emb_event_handler_t fn, void *fn_data,
              bool udp) {
  union usa usa[2];
  SOCKET sp[2] = {INVALID_SOCKET, INVALID_SOCKET};
  struct emb_connection *c = NULL;
  if (!emb_socketpair(sp, usa, udp)) {
    EMB_ERROR(("Cannot create socket pair"));
  } else if ((c = emb_wrapfd(mgr, (int) sp[1], fn, fn_data)) == NULL) {
    closesocket(sp[0]);
    closesocket(sp[1]);
    sp[0] = sp[1] = INVALID_SOCKET;
  } else {
    tomgaddr(&usa[0], &c->rem, false);
    EMB_DEBUG(("%lu %p pipe %lu", c->id, c->fd, (unsigned long) sp[0]));
  }
  return (int) sp[0];
}

static bool can_read(const struct emb_connection *c) {
  return c->is_full == false;
}

static bool can_write(const struct emb_connection *c) {
  return c->is_connecting || (c->send.len > 0 && c->is_tls_hs == 0);
}

static bool skip_iotest(const struct emb_connection *c) {
  return (c->is_closing || c->is_resolving || FD(c) == INVALID_SOCKET) ||
         (can_read(c) == false && can_write(c) == false);
}

static void emb_iotest(struct emb_mgr *mgr, int ms) {
#if EMB_ARCH == EMB_ARCH_FREERTOS_TCP
  struct emb_connection *c;
  for (c = mgr->conns; c != NULL; c = c->next) {
    c->is_readable = c->is_writable = 0;
    if (skip_iotest(c)) continue;
    if (can_read(c))
      FreeRTOS_FD_SET(c->fd, mgr->ss, eSELECT_READ | eSELECT_EXCEPT);
    if (can_write(c)) FreeRTOS_FD_SET(c->fd, mgr->ss, eSELECT_WRITE);
  }
  FreeRTOS_select(mgr->ss, pdMS_TO_TICKS(ms));
  for (c = mgr->conns; c != NULL; c = c->next) {
    EventBits_t bits = FreeRTOS_FD_ISSET(c->fd, mgr->ss);
    c->is_readable = bits & (eSELECT_READ | eSELECT_EXCEPT) ? 1 : 0;
    c->is_writable = bits & eSELECT_WRITE ? 1 : 0;
    FreeRTOS_FD_CLR(c->fd, mgr->ss,
                    eSELECT_READ | eSELECT_EXCEPT | eSELECT_WRITE);
  }
#elif EMB_ENABLE_POLL
  nfds_t n = 0;
  for (struct emb_connection *c = mgr->conns; c != NULL; c = c->next) n++;
  struct pollfd fds[n == 0 ? 1 : n];  // Avoid zero-length VLA

  memset(fds, 0, sizeof(fds));
  n = 0;
  for (struct emb_connection *c = mgr->conns; c != NULL; c = c->next) {
    c->is_readable = c->is_writable = 0;
    if (skip_iotest(c)) {
      // Socket not valid, ignore
    } else {
      fds[n].fd = FD(c);
      if (can_read(c)) fds[n].events = POLLIN;
      if (can_write(c)) fds[n].events |= POLLOUT;
      n++;
      if (emb_tls_pending(c) > 0) ms = 0;  // Don't wait if TLS is ready
    }
  }

  if (poll(fds, n, ms) < 0) {
    EMB_ERROR(("poll failed, errno: %d", EMB_SOCK_ERRNO));
  } else {
    n = 0;
    for (struct emb_connection *c = mgr->conns; c != NULL; c = c->next) {
      if (skip_iotest(c)) {
        // Socket not valid, ignore
      } else {
        c->is_readable = (unsigned) (fds[n].revents & POLLIN ? 1 : 0);
        c->is_writable = (unsigned) (fds[n].revents & POLLOUT ? 1 : 0);
        if (emb_tls_pending(c) > 0) c->is_readable = 1;
        fds[n].revents = 0;
        n++;
      }
    }
  }
#else
  struct timeval tv = {ms / 1000, (ms % 1000) * 1000}, tv_zero = {0, 0};
  struct emb_connection *c;
  fd_set rset, wset;
  SOCKET maxfd = 0;
  int rc;

  FD_ZERO(&rset);
  FD_ZERO(&wset);
  
  for (c = mgr->conns; c != NULL; c = c->next) {
    c->is_readable = c->is_writable = 0;
    if (skip_iotest(c)) continue;
    if (can_read(c)) FD_SET(FD(c), &rset);
    if (can_write(c)) FD_SET(FD(c), &wset);
    if (emb_tls_pending(c) > 0) tv = tv_zero;
    if (FD(c) > maxfd) maxfd = FD(c);
  }

  if ((rc = select((int) maxfd + 1, &rset, &wset, NULL, &tv)) < 0) {
#if EMB_ARCH == EMB_ARCH_WIN32
    if (maxfd == 0) Sleep(ms);  // On Windows, select fails if no sockets
#else
    EMB_ERROR(("select: %d %d", rc, EMB_SOCK_ERRNO));
#endif
    FD_ZERO(&rset);
    FD_ZERO(&wset);
  }

  for (c = mgr->conns; c != NULL; c = c->next) {
    c->is_readable = FD(c) != INVALID_SOCKET && FD_ISSET(FD(c), &rset);
    c->is_writable = FD(c) != INVALID_SOCKET && FD_ISSET(FD(c), &wset);
    if (emb_tls_pending(c) > 0) c->is_readable = 1;
  }
#endif
}

static void connect_conn(struct emb_connection *c) {
  int rc = 0;
#if (EMB_ARCH != EMB_ARCH_FREERTOS_TCP) && (EMB_ARCH != EMB_ARCH_RTX)
  socklen_t len = sizeof(rc);
  if (getsockopt(FD(c), SOL_SOCKET, SO_ERROR, (char *) &rc, &len)) rc = 1;
#endif
  if (rc == EAGAIN || rc == EWOULDBLOCK) rc = 0;
  c->is_connecting = 0;
  if (rc) {
    char buf[50];
    emb_error(c, "error connecting to %s",
             emb_straddr(&c->rem, buf, sizeof(buf)));
  } else {
    if (c->is_tls_hs) emb_tls_handshake(c);
    emb_call(c, EMB_EV_CONNECT, NULL);
  }
}

void emb_mgr_poll(struct emb_mgr *mgr, int ms) {
  struct emb_connection *c, *tmp;
  uint64_t now;

  emb_iotest(mgr, ms);
  now = emb_millis();
  emb_timer_poll(&mgr->timers, now);

  for (c = mgr->conns; c != NULL; c = tmp) {
    tmp = c->next;
    emb_call(c, EMB_EV_POLL, &now);
    EMB_VERBOSE(("%lu %c%c %c%c%c%c%c", c->id, c->is_readable ? 'r' : '-',
                c->is_writable ? 'w' : '-', c->is_tls ? 'T' : 't',
                c->is_connecting ? 'C' : 'c', c->is_tls_hs ? 'H' : 'h',
                c->is_resolving ? 'R' : 'r', c->is_closing ? 'C' : 'c'));
    if (c->is_resolving || c->is_closing) {
      // Do nothing
    } else if (c->is_listening && c->is_udp == 0) {
      if (c->is_readable) accept_conn(mgr, c);
    } else if (c->is_connecting) {
      if (c->is_readable || c->is_writable) connect_conn(c);
    } else if (c->is_tls_hs) {
      if ((c->is_readable || c->is_writable)) emb_tls_handshake(c);
    } else {
      if (c->is_readable) read_conn(c);
      if (c->is_writable) write_conn(c);
    }

    if (c->is_draining && c->send.len == 0) c->is_closing = 1;
    if (c->is_closing) close_conn(c);
  }
}
#endif
