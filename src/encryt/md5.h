#pragma once

#include "arch.h"

typedef struct {
  uint32_t buf[4];
  uint32_t bits[2];
  unsigned char in[64];
} emb_md5_ctx;

void emb_md5_init(emb_md5_ctx *c);
void emb_md5_update(emb_md5_ctx *c, const unsigned char *data, size_t len);
void emb_md5_final(emb_md5_ctx *c, unsigned char[16]);
