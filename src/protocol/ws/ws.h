#pragma once

#define WEBSOCKET_OP_CONTINUE 0
#define WEBSOCKET_OP_TEXT 1
#define WEBSOCKET_OP_BINARY 2
#define WEBSOCKET_OP_CLOSE 8
#define WEBSOCKET_OP_PING 9
#define WEBSOCKET_OP_PONG 10

#include "http.h"

struct emb_ws_message {
  struct emb_str data;  // Websocket message data
  uint8_t flags;       // Websocket message flags
};

struct emb_connection *emb_ws_connect(struct emb_mgr *, const char *url,
                                    emb_event_handler_t fn, void *fn_data,
                                    const char *fmt, ...);
void emb_ws_upgrade(struct emb_connection *, struct emb_http_message *,
                   const char *fmt, ...);
size_t emb_ws_send(struct emb_connection *, const char *buf, size_t len, int op);
size_t emb_ws_wrap(struct emb_connection *, size_t len, int op);
size_t emb_ws_printf(struct emb_connection *c, int op, const char *fmt, ...);
size_t emb_ws_vprintf(struct emb_connection *c, int op, const char *fmt, va_list);
