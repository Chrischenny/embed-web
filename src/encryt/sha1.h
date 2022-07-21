#pragma once

#include "arch.h"

typedef struct {
  uint32_t state[5];
  uint32_t count[2];
  unsigned char buffer[64];
} emb_sha1_ctx;

void emb_sha1_init(emb_sha1_ctx *);
void emb_sha1_update(emb_sha1_ctx *, const unsigned char *data, size_t len);
void emb_sha1_final(unsigned char digest[20], emb_sha1_ctx *);
