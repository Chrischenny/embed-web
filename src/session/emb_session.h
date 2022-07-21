/**
 * @file emb_session.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-07-02
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#ifndef __EMB_SEESION_H__
#define __EMB_SEESION_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "emb_def.h"

typedef int (*emb_session_req_hook)(emb_session_t *, void *);
typedef int (*emb_session_res_hook)(emb_session_t *, void *);

typedef struct emb_session {
    emb_session_req_hook *app_req_fn;
    emb_session_res_hook *app_resp_fn;
}emb_session_t;

emb_session_t *emb_create_session(size_t size);

void emb_destroy_session(emb_session_t *);

#ifdef __cplusplus
}
#endif
#endif // __EMB_SEESION_H__