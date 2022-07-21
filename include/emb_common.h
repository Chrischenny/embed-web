/**
 * @file emb_common.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-07-02
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __EMB_COMMON_H__
#define __EMB_COMMON_H__

#ifdef __cplusplus
extern "C" {
#endif

struct emb_kv {
    unsigned char *key;
    unsigned char *val;
};

#ifdef __cplusplus
}
#endif
#endif // __EMB_COMMON_H__
