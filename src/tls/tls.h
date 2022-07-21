#pragma once

#include "net.h"
#include "tls_mbed.h"
#include "tls_openssl.h"

struct emb_tls_opts {
  const char *ca;         // CA certificate file. For both listeners and clients
  const char *crl;        // Certificate Revocation List. For clients
  const char *cert;       // Certificate
  const char *certkey;    // Certificate key
  const char *ciphers;    // Cipher list
  struct emb_str srvname;  // If not empty, enables server name verification
  struct emb_fs *fs;       // FS API for reading certificate files
};

void emb_tls_init(struct emb_connection *, const struct emb_tls_opts *);
void emb_tls_free(struct emb_connection *);
long emb_tls_send(struct emb_connection *, const void *buf, size_t len);
long emb_tls_recv(struct emb_connection *, void *buf, size_t len);
size_t emb_tls_pending(struct emb_connection *);
void emb_tls_handshake(struct emb_connection *);
