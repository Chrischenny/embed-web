#include "ws.h"

#include "base64.h"
#include "http.h"
#include "log.h"
#include "sha1.h"
#include "url.h"
#include "util.h"

struct ws_msg {
  uint8_t flags;
  size_t header_len;
  size_t data_len;
};

size_t emb_ws_vprintf(struct emb_connection *c, int op, const char *fmt,
                     va_list ap) {
  char mem[256], *buf = mem;
  size_t len = emb_vasprintf(&buf, sizeof(mem), fmt, ap);
  len = emb_ws_send(c, buf, len, op);
  if (buf != mem) free(buf);
  return len;
}

size_t emb_ws_printf(struct emb_connection *c, int op, const char *fmt, ...) {
  size_t len = 0;
  va_list ap;
  va_start(ap, fmt);
  len = emb_ws_vprintf(c, op, fmt, ap);
  va_end(ap);
  return len;
}

static void ws_handshake(struct emb_connection *c, const struct emb_str *wskey,
                         const struct emb_str *wsproto, const char *fmt,
                         va_list ap) {
  const char *magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  unsigned char sha[20], b64_sha[30];
  char mem[128], *buf = mem;

  emb_sha1_ctx sha_ctx;
  emb_sha1_init(&sha_ctx);
  emb_sha1_update(&sha_ctx, (unsigned char *) wskey->ptr, wskey->len);
  emb_sha1_update(&sha_ctx, (unsigned char *) magic, 36);
  emb_sha1_final(sha, &sha_ctx);
  emb_base64_encode(sha, sizeof(sha), (char *) b64_sha);
  buf[0] = '\0';
  if (fmt != NULL) emb_vasprintf(&buf, sizeof(mem), fmt, ap);
  emb_printf(c,
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Accept: %s\r\n"
            "%s",
            b64_sha, buf);
  if (buf != mem) free(buf);
  if (wsproto != NULL) {
    emb_printf(c, "Sec-WebSocket-Protocol: %.*s\r\n", (int) wsproto->len,
              wsproto->ptr);
  }
  emb_send(c, "\r\n", 2);
}

static size_t ws_process(uint8_t *buf, size_t len, struct ws_msg *msg) {
  size_t i, n = 0, mask_len = 0;
  memset(msg, 0, sizeof(*msg));
  if (len >= 2) {
    n = buf[1] & 0x7f;                // Frame length
    mask_len = buf[1] & 128 ? 4 : 0;  // last bit is a mask bit
    msg->flags = buf[0];
    if (n < 126 && len >= mask_len) {
      msg->data_len = n;
      msg->header_len = 2 + mask_len;
    } else if (n == 126 && len >= 4 + mask_len) {
      msg->header_len = 4 + mask_len;
      msg->data_len = emb_ntohs(*(uint16_t *) &buf[2]);
    } else if (len >= 10 + mask_len) {
      msg->header_len = 10 + mask_len;
      msg->data_len =
          (size_t) (((uint64_t) emb_ntohl(*(uint32_t *) &buf[2])) << 32) +
          emb_ntohl(*(uint32_t *) &buf[6]);
    }
  }
  // Sanity check, and integer overflow protection for the boundary check below
  // data_len should not be larger than 1 Gb
  if (msg->data_len > 1024 * 1024 * 1024) return 0;
  if (msg->header_len + msg->data_len > len) return 0;
  if (mask_len > 0) {
    uint8_t *p = buf + msg->header_len, *m = p - mask_len;
    for (i = 0; i < msg->data_len; i++) p[i] ^= m[i & 3];
  }
  return msg->header_len + msg->data_len;
}

static size_t mkhdr(size_t len, int op, bool is_client, uint8_t *buf) {
  size_t n = 0;
  buf[0] = (uint8_t) (op | 128);
  if (len < 126) {
    buf[1] = (unsigned char) len;
    n = 2;
  } else if (len < 65536) {
    uint16_t tmp = emb_htons((uint16_t) len);
    buf[1] = 126;
    memcpy(&buf[2], &tmp, sizeof(tmp));
    n = 4;
  } else {
    uint32_t tmp;
    buf[1] = 127;
    tmp = emb_htonl((uint32_t) ((uint64_t) len >> 32));
    memcpy(&buf[2], &tmp, sizeof(tmp));
    tmp = emb_htonl((uint32_t) (len & 0xffffffff));
    memcpy(&buf[6], &tmp, sizeof(tmp));
    n = 10;
  }
  if (is_client) {
    buf[1] |= 1 << 7;  // Set masking flag
    emb_random(&buf[n], 4);
    n += 4;
  }
  return n;
}

static void emb_ws_mask(struct emb_connection *c, size_t len) {
  if (c->is_client && c->send.buf != NULL) {
    size_t i;
    uint8_t *p = c->send.buf + c->send.len - len, *mask = p - 4;
    for (i = 0; i < len; i++) p[i] ^= mask[i & 3];
  }
}

size_t emb_ws_send(struct emb_connection *c, const char *buf, size_t len,
                  int op) {
  uint8_t header[14];
  size_t header_len = mkhdr(len, op, c->is_client, header);
  emb_send(c, header, header_len);
  EMB_VERBOSE(("WS out: %d [%.*s]", (int) len, (int) len, buf));
  emb_send(c, buf, len);
  emb_ws_mask(c, len);
  return header_len + len;
}

static void emb_ws_cb(struct emb_connection *c, int ev, void *ev_data,
                     void *fn_data) {
  struct ws_msg msg;
  size_t ofs = (size_t) c->pfn_data;

  // assert(ofs < c->recv.len);
  if (ev == EMB_EV_READ) {
    if (!c->is_websocket && c->is_client) {
      int n = emb_http_get_request_len(c->recv.buf, c->recv.len);
      if (n < 0) {
        c->is_closing = 1;  // Some just, not an HTTP request
      } else if (n > 0) {
        if (n < 15 || memcmp(c->recv.buf + 9, "101", 3) != 0) {
          EMB_ERROR(("%lu WS handshake error: %.*s", c->id, 15, c->recv.buf));
          c->is_closing = 1;
        } else {
          struct emb_http_message hm;
          emb_http_parse((char *) c->recv.buf, c->recv.len, &hm);
          c->is_websocket = 1;
          emb_call(c, EMB_EV_WS_OPEN, &hm);
        }
        emb_iobuf_del(&c->recv, 0, (size_t) n);
      } else {
        return;  // A request is not yet received
      }
    }

    while (ws_process(c->recv.buf + ofs, c->recv.len - ofs, &msg) > 0) {
      char *s = (char *) c->recv.buf + ofs + msg.header_len;
      struct emb_ws_message m = {{s, msg.data_len}, msg.flags};
      size_t len = msg.header_len + msg.data_len;
      uint8_t final = msg.flags & 128, op = msg.flags & 15;
      // EMB_VERBOSE ("fin %d op %d len %d [%.*s]", final, op,
      //                       (int) m.data.len, (int) m.data.len, m.data.ptr));
      switch (op) {
        case WEBSOCKET_OP_CONTINUE:
          emb_call(c, EMB_EV_WS_CTL, &m);
          break;
        case WEBSOCKET_OP_PING:
          EMB_DEBUG(("%s", "WS PONG"));
          emb_ws_send(c, s, msg.data_len, WEBSOCKET_OP_PONG);
          emb_call(c, EMB_EV_WS_CTL, &m);
          break;
        case WEBSOCKET_OP_PONG:
          emb_call(c, EMB_EV_WS_CTL, &m);
          break;
        case WEBSOCKET_OP_TEXT:
        case WEBSOCKET_OP_BINARY:
          if (final) emb_call(c, EMB_EV_WS_MSG, &m);
          break;
        case WEBSOCKET_OP_CLOSE:
          EMB_DEBUG(("%lu Got WS CLOSE", c->id));
          emb_call(c, EMB_EV_WS_CTL, &m);
          emb_ws_send(c, "", 0, WEBSOCKET_OP_CLOSE);
          c->is_draining = 1;
          break;
        default:
          // Per RFC6455, close conn when an unknown op is recvd
          emb_error(c, "unknown WS op %d", op);
          break;
      }

      // Handle fragmented frames: strip header, keep in c->recv
      if (final == 0 || op == 0) {
        if (op) ofs++, len--, msg.header_len--;       // First frame
        emb_iobuf_del(&c->recv, ofs, msg.header_len);  // Strip header
        len -= msg.header_len;
        ofs += len;
        c->pfn_data = (void *) ofs;
        // EMB_INFO(("FRAG %d [%.*s]", (int) ofs, (int) ofs, c->recv.buf));
      }
      // Remove non-fragmented frame
      if (final && op) emb_iobuf_del(&c->recv, ofs, len);
      // Last chunk of the fragmented frame
      if (final && !op) {
        m.flags = c->recv.buf[0];
        m.data = emb_str_n((char *) &c->recv.buf[1], (size_t) (ofs - 1));
        emb_call(c, EMB_EV_WS_MSG, &m);
        emb_iobuf_del(&c->recv, 0, ofs);
        ofs = 0;
        c->pfn_data = NULL;
      }
    }
  }
  (void) fn_data;
  (void) ev_data;
}

struct emb_connection *emb_ws_connect(struct emb_mgr *mgr, const char *url,
                                    emb_event_handler_t fn, void *fn_data,
                                    const char *fmt, ...) {
  struct emb_connection *c = emb_connect(mgr, url, fn, fn_data);
  if (c != NULL) {
    char nonce[16], key[30], mem1[128], mem2[256], *buf1 = mem1, *buf2 = mem2;
    struct emb_str host = emb_url_host(url);
    size_t n1 = 0, n2 = 0;
    nonce[0] = key[0] = mem1[0] = mem2[0] = '\0';
    if (fmt != NULL) {
      va_list ap;
      va_start(ap, fmt);
      n1 = emb_vasprintf(&buf1, sizeof(mem1), fmt, ap);
      va_end(ap);
    }
    // Send handshake request
    emb_random(nonce, sizeof(nonce));
    emb_base64_encode((unsigned char *) nonce, sizeof(nonce), key);
    n2 = emb_asprintf(&buf2, sizeof(mem2),
                     "GET %s HTTP/1.1\r\n"
                     "Upgrade: websocket\r\n"
                     "Host: %.*s\r\n"
                     "Connection: Upgrade\r\n"
                     "%.*s"
                     "Sec-WebSocket-Version: 13\r\n"
                     "Sec-WebSocket-Key: %s\r\n"
                     "\r\n",
                     emb_url_uri(url), (int) host.len, host.ptr, (int) n1, buf1,
                     key);
    emb_send(c, buf2, n2);
    if (buf1 != mem1) free(buf1);
    if (buf2 != mem2) free(buf2);
    c->pfn = emb_ws_cb;
    c->pfn_data = NULL;
  }
  return c;
}

void emb_ws_upgrade(struct emb_connection *c, struct emb_http_message *hm,
                   const char *fmt, ...) {
  struct emb_str *wskey = emb_http_get_header(hm, "Sec-WebSocket-Key");
  c->pfn = emb_ws_cb;
  c->pfn_data = NULL;
  if (wskey == NULL) {
    emb_http_reply(c, 426, "", "WS upgrade expected\n");
    c->is_draining = 1;
  } else {
    struct emb_str *wsproto = emb_http_get_header(hm, "Sec-WebSocket-Protocol");
    va_list ap;
    va_start(ap, fmt);
    ws_handshake(c, wskey, wsproto, fmt, ap);
    va_end(ap);
    c->is_websocket = 1;
    emb_call(c, EMB_EV_WS_OPEN, hm);
  }
}

size_t emb_ws_wrap(struct emb_connection *c, size_t len, int op) {
  uint8_t header[14], *p;
  size_t header_len = mkhdr(len, op, c->is_client, header);

  // NOTE: order of operations is important!
  emb_iobuf_add(&c->send, c->send.len, NULL, header_len, EMB_IO_SIZE);
  p = &c->send.buf[c->send.len - len];         // p points to data
  memmove(p, p - header_len, len);             // Shift data
  memcpy(p - header_len, header, header_len);  // Prepend header
  emb_ws_mask(c, len);                          // Mask data

  return c->send.len;
}
