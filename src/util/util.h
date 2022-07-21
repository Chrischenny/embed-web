/**
 * @file util.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-06-18
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "arch.h"
#include "config.h"
#include "str.h"

void emb_random(void *buf, size_t len);
uint16_t emb_ntohs(uint16_t net);
uint32_t emb_ntohl(uint32_t net);
uint32_t emb_crc32(uint32_t crc, const char *buf, size_t len);
uint64_t emb_millis(void);

#define emb_htons(x) emb_ntohs(x)
#define emb_htonl(x) emb_ntohl(x)

