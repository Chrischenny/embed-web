#include "tls.h"

#if !EMB_ENABLE_MBEDTLS && !EMB_ENABLE_OPENSSL && !EMB_ENABLE_CUSTOM_TLS
void emb_tls_init(struct emb_connection *c, const struct emb_tls_opts *opts) {
  (void) opts;
  emb_error(c, "TLS is not enabled");
}
void emb_tls_handshake(struct emb_connection *c) {
  (void) c;
}
void emb_tls_free(struct emb_connection *c) {
  (void) c;
}
long emb_tls_recv(struct emb_connection *c, void *buf, size_t len) {
  return c == NULL || buf == NULL || len == 0 ? 0 : -1;
}
long emb_tls_send(struct emb_connection *c, const void *buf, size_t len) {
  return c == NULL || buf == NULL || len == 0 ? 0 : -1;
}
size_t emb_tls_pending(struct emb_connection *c) {
  (void) c;
  return 0;
}
#endif
