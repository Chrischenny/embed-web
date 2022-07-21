#include "fs.h"
#include "tls.h"

#if EMB_ENABLE_MBEDTLS

#if defined(MBEDTLS_VERSION_NUMBER) && MBEDTLS_VERSION_NUMBER >= 0x03000000
#define MGRNG , rng_get, NULL
#else
#define MGRNG
#endif

void emb_tls_free(struct emb_connection *c) {
  struct emb_tls *tls = (struct emb_tls *) c->tls;
  if (tls != NULL) {
    free(tls->cafile);
    mbedtls_ssl_free(&tls->ssl);
    mbedtls_pk_free(&tls->pk);
    mbedtls_x509_crt_free(&tls->ca);
    mbedtls_x509_crt_free(&tls->cert);
    mbedtls_ssl_config_free(&tls->conf);
    free(tls);
    c->tls = NULL;
  }
}

bool emb_sock_would_block(void);
bool emb_sock_conn_reset(void);

static int emb_net_send(void *ctx, const unsigned char *buf, size_t len) {
  struct emb_connection *c = (struct emb_connection *) ctx;
  int fd = (int) (size_t) c->fd;
  int n = (int) send(fd, buf, len, 0);
  EMB_VERBOSE(("%lu n=%d, errno=%d", c->id, n, errno));
  if (n < 0) {
    if (emb_sock_would_block()) return MBEDTLS_ERR_SSL_WANT_WRITE;
    if (emb_sock_conn_reset()) return MBEDTLS_ERR_NET_CONN_RESET;
    return MBEDTLS_ERR_NET_SEND_FAILED;
  }
  return n;
}

static int emb_net_recv(void *ctx, unsigned char *buf, size_t len) {
  struct emb_connection *c = (struct emb_connection *) ctx;
  int n, fd = (int) (size_t) c->fd;
  n = (int) recv(fd, buf, len, 0);
  EMB_VERBOSE(("%lu n=%d, errno=%d", c->id, n, errno));
  if (n < 0) {
    if (emb_sock_would_block()) return MBEDTLS_ERR_SSL_WANT_READ;
    if (emb_sock_conn_reset()) return MBEDTLS_ERR_NET_CONN_RESET;
    return MBEDTLS_ERR_NET_RECV_FAILED;
  }
  return n;
}

void emb_tls_handshake(struct emb_connection *c) {
  struct emb_tls *tls = (struct emb_tls *) c->tls;
  int rc;
  mbedtls_ssl_set_bio(&tls->ssl, c, emb_net_send, emb_net_recv, 0);
  rc = mbedtls_ssl_handshake(&tls->ssl);
  if (rc == 0) {  // Success
    EMB_DEBUG(("%lu success", c->id));
    c->is_tls_hs = 0;
  } else if (rc == MBEDTLS_ERR_SSL_WANT_READ ||
             rc == MBEDTLS_ERR_SSL_WANT_WRITE) {  // Still pending
    EMB_VERBOSE(("%lu pending, %d%d %d (-%#x)", c->id, c->is_connecting,
                c->is_tls_hs, rc, -rc));
  } else {
    emb_error(c, "TLS handshake: -%#x", -rc);  // Error
  }
}

static int mbed_rng(void *ctx, unsigned char *buf, size_t len) {
  emb_random(buf, len);
  (void) ctx;
  return 0;
}

static void debug_cb(void *c, int lev, const char *s, int n, const char *s2) {
  n = (int) strlen(s2) - 1;
  EMB_VERBOSE(("%lu %d %.*s", ((struct emb_connection *) c)->id, lev, n, s2));
  (void) s;
}

#if defined(MBEDTLS_VERSION_NUMBER) && MBEDTLS_VERSION_NUMBER >= 0x03000000
static int rng_get(void *p_rng, unsigned char *buf, size_t len) {
  (void) p_rng;
  emb_random(buf, len);
  return 0;
}
#endif

static struct emb_str emb_loadfile(struct emb_fs *fs, const char *path) {
  size_t n = 0;
  if (path[0] == '-') return emb_str(path);
  char *p = emb_file_read(fs, path, &n);
  return emb_str_n(p, n);
}

void emb_tls_init(struct emb_connection *c, const struct emb_tls_opts *opts) {
  struct emb_fs *fs = opts->fs == NULL ? &emb_fs_posix : opts->fs;
  struct emb_tls *tls = (struct emb_tls *) calloc(1, sizeof(*tls));
  int rc = 0;
  c->tls = tls;
  if (c->tls == NULL) {
    emb_error(c, "TLS OOM");
    goto fail;
  }
  EMB_DEBUG(("%lu Setting TLS", c->id));
  mbedtls_ssl_init(&tls->ssl);
  mbedtls_ssl_config_init(&tls->conf);
  mbedtls_x509_crt_init(&tls->ca);
  mbedtls_x509_crt_init(&tls->cert);
  mbedtls_pk_init(&tls->pk);
  mbedtls_ssl_conf_dbg(&tls->conf, debug_cb, c);
#if defined(EMB_MBEDTLS_DEBUG_LEVEL)
  mbedtls_debug_set_threshold(EMB_MBEDTLS_DEBUG_LEVEL);
#endif
  if ((rc = mbedtls_ssl_config_defaults(
           &tls->conf,
           c->is_client ? MBEDTLS_SSL_IS_CLIENT : MBEDTLS_SSL_IS_SERVER,
           MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
    emb_error(c, "tls defaults %#x", -rc);
    goto fail;
  }
  mbedtls_ssl_conf_rng(&tls->conf, mbed_rng, c);
  if (opts->ca == NULL || strcmp(opts->ca, "*") == 0) {
    mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_NONE);
  } else if (opts->ca != NULL && opts->ca[0] != '\0') {
#if defined(MBEDTLS_X509_CA_CHAIN_ON_DISK)
    tls->cafile = strdup(opts->ca);
    rc = mbedtls_ssl_conf_ca_chain_file(&tls->conf, tls->cafile, NULL);
    if (rc != 0) {
      emb_error(c, "parse on-disk chain(%s) err %#x", tls->cafile, -rc);
      goto fail;
    }
#else
    struct emb_str s = emb_loadfile(fs, opts->ca);
    rc = mbedtls_x509_crt_parse(&tls->ca, (uint8_t *) s.ptr, s.len + 1);
    if (opts->ca[0] != '-') free((char *) s.ptr);
    if (rc != 0) {
      emb_error(c, "parse(%s) err %#x", opts->ca, -rc);
      goto fail;
    }
    mbedtls_ssl_conf_ca_chain(&tls->conf, &tls->ca, NULL);
#endif
    if (opts->srvname.len > 0) {
      char mem[128], *buf = mem;
      emb_asprintf(&buf, sizeof(mem), "%.*s", (int) opts->srvname.len,
                  opts->srvname.ptr);
      mbedtls_ssl_set_hostname(&tls->ssl, buf);
      if (buf != mem) free(buf);
    }
    mbedtls_ssl_conf_authmode(&tls->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
  }
  if (opts->cert != NULL && opts->cert[0] != '\0') {
    struct emb_str s = emb_loadfile(fs, opts->cert);
    const char *key = opts->certkey == NULL ? opts->cert : opts->certkey;
    rc = mbedtls_x509_crt_parse(&tls->cert, (uint8_t *) s.ptr, s.len + 1);
    if (opts->cert[0] != '-') free((char *) s.ptr);
    if (rc != 0) {
      emb_error(c, "parse(%s) err %#x", opts->cert, -rc);
      goto fail;
    }
    s = emb_loadfile(fs, key);
    rc = mbedtls_pk_parse_key(&tls->pk, (uint8_t *) s.ptr, s.len + 1, NULL,
                              0 MGRNG);
    if (key[0] != '-') free((char *) s.ptr);
    if (rc != 0) {
      emb_error(c, "tls key(%s) %#x", key, -rc);
      goto fail;
    }
    rc = mbedtls_ssl_conf_own_cert(&tls->conf, &tls->cert, &tls->pk);
    if (rc != 0) {
      emb_error(c, "own cert %#x", -rc);
      goto fail;
    }
  }
  if ((rc = mbedtls_ssl_setup(&tls->ssl, &tls->conf)) != 0) {
    emb_error(c, "setup err %#x", -rc);
    goto fail;
  }
  c->tls = tls;
  c->is_tls = 1;
  c->is_tls_hs = 1;
  if (c->is_client && c->is_resolving == 0 && c->is_connecting == 0) {
    emb_tls_handshake(c);
  }
  return;
fail:
  emb_tls_free(c);
}

size_t emb_tls_pending(struct emb_connection *c) {
  struct emb_tls *tls = (struct emb_tls *) c->tls;
  return tls == NULL ? 0 : mbedtls_ssl_get_bytes_avail(&tls->ssl);
}

long emb_tls_recv(struct emb_connection *c, void *buf, size_t len) {
  struct emb_tls *tls = (struct emb_tls *) c->tls;
  long n = mbedtls_ssl_read(&tls->ssl, (unsigned char *) buf, len);
  return n == 0 ? -1 : n == MBEDTLS_ERR_SSL_WANT_READ ? 0 : n;
}

long emb_tls_send(struct emb_connection *c, const void *buf, size_t len) {
  struct emb_tls *tls = (struct emb_tls *) c->tls;
  long n = mbedtls_ssl_write(&tls->ssl, (unsigned char *) buf, len);
  return n == 0 ? -1 : n == MBEDTLS_ERR_SSL_WANT_WRITE ? 0 : n;
}
#endif
