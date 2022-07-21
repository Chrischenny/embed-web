#pragma once

#if EMB_ENABLE_OPENSSL

#include <openssl/err.h>
#include <openssl/ssl.h>

struct emb_tls {
  SSL_CTX *ctx;
  SSL *ssl;
};
#endif
