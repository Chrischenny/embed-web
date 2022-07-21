#include "tls.h"

#if EMB_ENABLE_OPENSSL
static int emb_tls_err(struct emb_tls *tls, int res) {
  int err = SSL_get_error(tls->ssl, res);
  // We've just fetched the last error from the queue.
  // Now we need to clear the error queue. If we do not, then the following
  // can happen (actually reported):
  //  - A new connection is accept()-ed with cert error (e.g. self-signed cert)
  //  - Since all accept()-ed connections share listener's context,
  //  - *ALL* SSL accepted connection report read error on the next poll cycle.
  //    Thus a single errored connection can close all the rest, unrelated ones.
  // Clearing the error keeps the shared SSL_CTX in an OK state.

  if (err != 0) ERR_print_errors_fp(stderr);
  ERR_clear_error();
  if (err == SSL_ERROR_WANT_READ) return 0;
  if (err == SSL_ERROR_WANT_WRITE) return 0;
  return err;
}

void emb_tls_init(struct emb_connection *c, const struct emb_tls_opts *opts) {
  struct emb_tls *tls = (struct emb_tls *) calloc(1, sizeof(*tls));
  const char *id = "mongoose";
  static unsigned char s_initialised = 0;
  int rc;

  if (tls == NULL) {
    emb_error(c, "TLS OOM");
    goto fail;
  }

  if (!s_initialised) {
    SSL_library_init();
    s_initialised++;
  }
  EMB_DEBUG(("%lu Setting TLS, CA: %s, cert: %s, key: %s", c->id,
            opts->ca == NULL ? "null" : opts->ca,
            opts->cert == NULL ? "null" : opts->cert,
            opts->certkey == NULL ? "null" : opts->certkey));
  tls->ctx = c->is_client ? SSL_CTX_new(SSLv23_client_method())
                          : SSL_CTX_new(SSLv23_server_method());
  if ((tls->ssl = SSL_new(tls->ctx)) == NULL) {
    emb_error(c, "SSL_new");
    goto fail;
  }
  SSL_set_session_id_context(tls->ssl, (const uint8_t *) id,
                             (unsigned) strlen(id));
  // Disable deprecated protocols
  SSL_set_options(tls->ssl, SSL_OP_NO_SSLv2);
  SSL_set_options(tls->ssl, SSL_OP_NO_SSLv3);
  SSL_set_options(tls->ssl, SSL_OP_NO_TLSv1);
  SSL_set_options(tls->ssl, SSL_OP_NO_TLSv1_1);
#ifdef EMB_ENABLE_OPENSSL_NO_COMPRESSION
  SSL_set_options(tls->ssl, SSL_OP_NO_COMPRESSION);
#endif
#ifdef EMB_ENABLE_OPENSSL_CIPHER_SERVER_PREFERENCE
  SSL_set_options(tls->ssl, SSL_OP_CIPHER_SERVER_PREFERENCE);
#endif

  if (opts->ca != NULL && opts->ca[0] != '\0') {
    SSL_set_verify(tls->ssl, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
                   NULL);
    if ((rc = SSL_CTX_load_verify_locations(tls->ctx, opts->ca, NULL)) != 1) {
      emb_error(c, "parse(%s): err %d", opts->ca, emb_tls_err(tls, rc));
      goto fail;
    }
  }
  if (opts->cert != NULL && opts->cert[0] != '\0') {
    const char *key = opts->certkey;
    if (key == NULL) key = opts->cert;
    if ((rc = SSL_use_certificate_file(tls->ssl, opts->cert, 1)) != 1) {
      emb_error(c, "Invalid SSL cert, err %d", emb_tls_err(tls, rc));
      goto fail;
    } else if ((rc = SSL_use_PrivateKey_file(tls->ssl, key, 1)) != 1) {
      emb_error(c, "Invalid SSL key, err %d", emb_tls_err(tls, rc));
      goto fail;
#if OPENSSL_VERSION_NUMBER > 0x10100000L
    } else if ((rc = SSL_use_certificate_chain_file(tls->ssl, opts->cert)) !=
               1) {
      emb_error(c, "Invalid CA, err %d", emb_tls_err(tls, rc));
      goto fail;
#endif
    } else {
      SSL_set_mode(tls->ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
#if OPENSSL_VERSION_NUMBER > 0x10002000L
      SSL_set_ecdh_auto(tls->ssl, 1);
#endif
    }
  }
#if OPENSSL_VERSION_NUMBER > 0x10002000L
  if (opts->srvname.len > 0) {
    char mem[128], *buf = mem;
    size_t len = emb_asprintf(&buf, sizeof(mem), "%.*s", (int) opts->srvname.len,
                             opts->srvname.ptr);
    X509_VERIFY_PARAM_set1_host(SSL_get0_param(tls->ssl), buf, len);
    if (buf != mem) free(buf);
  }
#endif
  if (opts->ciphers != NULL) SSL_set_cipher_list(tls->ssl, opts->ciphers);
  if (opts->srvname.len > 0) {
    char mem[128], *buf = mem;
    emb_asprintf(&buf, sizeof(mem), "%.*s", (int) opts->srvname.len,
                opts->srvname.ptr);
    SSL_set_tlsext_host_name(tls->ssl, buf);
    if (buf != mem) free(buf);
  }
  c->tls = tls;
  c->is_tls = 1;
  c->is_tls_hs = 1;
  if (c->is_client && c->is_resolving == 0 && c->is_connecting == 0) {
    emb_tls_handshake(c);
  }
  EMB_DEBUG(("%lu SSL %s OK", c->id, c->is_accepted ? "accept" : "client"));
  return;
fail:
  c->is_closing = 1;
  free(tls);
}

void emb_tls_handshake(struct emb_connection *c) {
  struct emb_tls *tls = (struct emb_tls *) c->tls;
  int rc;
  SSL_set_fd(tls->ssl, (int) (size_t) c->fd);
  rc = c->is_client ? SSL_connect(tls->ssl) : SSL_accept(tls->ssl);
  if (rc == 1) {
    EMB_DEBUG(("%lu success", c->id));
    c->is_tls_hs = 0;
  } else {
    int code = emb_tls_err(tls, rc);
    if (code != 0) emb_error(c, "tls hs: rc %d, err %d", rc, code);
  }
}

void emb_tls_free(struct emb_connection *c) {
  struct emb_tls *tls = (struct emb_tls *) c->tls;
  if (tls == NULL) return;
  SSL_free(tls->ssl);
  SSL_CTX_free(tls->ctx);
  free(tls);
  c->tls = NULL;
}

size_t emb_tls_pending(struct emb_connection *c) {
  struct emb_tls *tls = (struct emb_tls *) c->tls;
  return tls == NULL ? 0 : (size_t) SSL_pending(tls->ssl);
}

long emb_tls_recv(struct emb_connection *c, void *buf, size_t len) {
  struct emb_tls *tls = (struct emb_tls *) c->tls;
  int n = SSL_read(tls->ssl, buf, (int) len);
  return n == 0 ? -1 : n < 0 && emb_tls_err(tls, n) == 0 ? 0 : n;
}

long emb_tls_send(struct emb_connection *c, const void *buf, size_t len) {
  struct emb_tls *tls = (struct emb_tls *) c->tls;
  int n = SSL_write(tls->ssl, buf, (int) len);
  return n == 0 ? -1 : n < 0 && emb_tls_err(tls, n) == 0 ? 0 : n;
}
#endif
