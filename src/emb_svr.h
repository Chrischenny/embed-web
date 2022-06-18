/**
 * @file emb_svr.h
 * @author Chris Chen (chrischennt123@gmail.com)
 * @brief 
 * @version 0.1
 * @date 2022-06-18
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __EMB_SVR_H__
#define __EMB_SVR_H__
#include "arch.h"
#include "emb_hook.h"
#include "buf/iobuf.h"

#ifdef __cplusplus
extern "C" {
#endif

struct emb_addr {
  uint16_t port;    // TCP or UDP port in network byte order
  uint32_t ip;      // IP address in network byte order
  uint8_t ip6[16];  // IPv6 address
  bool is_ip6;      // True when address is IPv6 address
};

typedef struct emb_mgr {
  emb_conn_t *conns;  // List of active connections
  int dnstimeout;               // DNS resolve timeout in milliseconds
  bool use_dns6;                // Use DNS6 server by default, see #1532
  unsigned long nextid;         // Next connection ID
  void *userdata;               // Arbitrary user data pointer
  uint16_t mqtt_id;             // MQTT IDs for pub/sub
  void *active_dns_requests;    // DNS requests in progress
  struct mg_timer *timers;      // Active timers
  void *priv;                   // Used by the experimental stack
  size_t extraconnsize;         // Used by the experimental stack
#if MG_ARCH == MG_ARCH_FREERTOS_TCP
  SocketSet_t ss;  // NOTE(lsm): referenced from socket struct
#endif
}emb_mgr_t;

typedef struct emb_connection {
  emb_conn_t *next;             // Linkage in struct emb_mgr :: connections
  emb_mgr_t *mgr;          // Our container
  struct emb_addr local;          // Local address
  struct emb_addr remote;          // Remote address
  void *fd;
  unsigned long id;             // Auto-incrementing unique connection ID
  struct emb_iobuf recv;        // read buffer
  struct emb_iobuf send;        // write buffer
  emb_hook_fn rd_hook;          // read hook
  emb_hook_fn wt_hook;          // write hook
  void *tls;                   // TLS specific data
  unsigned is_listening : 1;   // Listening connection
  unsigned is_client : 1;      // Outbound (client) connection
  unsigned is_accepted : 1;    // Accepted (server) connection
  unsigned is_resolving : 1;   // Non-blocking DNS resolution is in progress
  unsigned is_connecting : 1;  // Non-blocking connect is in progress
  unsigned is_tls : 1;         // TLS-enabled connection
  unsigned is_tls_hs : 1;      // TLS handshake is in progress
  unsigned is_udp : 1;         // UDP connection
  unsigned is_websocket : 1;   // WebSocket connection
  unsigned is_hexdumping : 1;  // Hexdump in/out traffic
  unsigned is_draining : 1;    // Send remaining data, then close and free
  unsigned is_closing : 1;     // Close and free the connection immediately
  unsigned is_full : 1;        // Stop reads, until cleared
  unsigned is_readable : 1;    // Connection is ready to read
  unsigned is_writable : 1;    // Connection is ready to write
}emb_conn_t;

void emb_mgr_poll(emb_mgr_t *, int ms);
void emb_mgr_init(emb_mgr_t *);
void emb_mgr_free(emb_mgr_t *);

emb_conn_t *emb_listen(struct emb_mgr *, const char *url, emb_hook_fn fn, void *fn_data);
emb_conn_t *emb_connect(struct emb_mgr *, const char *url, emb_hook_fn fn, void *fn_data);
emb_conn_t *emb_wrapfd(struct emb_mgr *mgr, int fd, emb_hook_fn fn, void *fn_data);

void mg_connect_resolved(emb_conn_t *);
bool mg_send(emb_conn_t *, const void *, size_t);
size_t mg_printf(emb_conn_t *, const char *fmt, ...);
size_t mg_vprintf(emb_conn_t *, const char *fmt, va_list ap);
char *mg_straddr(struct mg_addr *, char *, size_t);
bool mg_aton(struct mg_str str, struct mg_addr *addr);
char *mg_ntoa(const struct mg_addr *addr, char *buf, size_t len);
int mg_mkpipe(struct emb_mgr *, emb_hook_fn, void *, bool udp);

// These functions are used to integrate with custom network stacks
emb_conn_t *mg_alloc_conn(struct emb_mgr *);
void mg_close_conn(emb_conn_t *c);
bool mg_open_listener(emb_conn_t *c, const char *url);
struct mg_timer *mg_timer_add(struct emb_mgr *mgr, uint64_t milliseconds,
                              unsigned flags, void (*fn)(void *), void *arg);
#ifdef __cplusplus
}
#endif
#endif // __EMB_SVR_H__