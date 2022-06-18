/**
 * @file iobuf.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-06-18
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __EMB_IOBUF_H__
#define __EMB_IOBUF_H__

#ifdef __cplusplus
extern "C" {
#endif
#include <stddef.h>

struct emb_iobuf {
  unsigned char *buf;  // Pointer to stored data
  size_t size;         // Total size available
  size_t len;          // Current number of bytes
};

int emb_iobuf_init(struct emb_iobuf *, size_t);
int emb_iobuf_resize(struct emb_iobuf *, size_t);
void emb_iobuf_free(struct emb_iobuf *);
size_t emb_iobuf_add(struct emb_iobuf *, size_t, const void *, size_t, size_t);
size_t emb_iobuf_del(struct emb_iobuf *, size_t ofs, size_t len);

#ifdef __cplusplus
}
#endif
#endif // __EMB_IOBUF_H__