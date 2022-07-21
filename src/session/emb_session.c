/**
 * @file emb_session.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-07-02
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "emb_session.h"

typedef struct emb_session_ctl {
    uint32_t seq;
    uint32_t ack;

    emb_session_t session;
}emb_session_ctl_t;

#define GET_CTL(ptr) (char *ptr) - (emb_session_ctl_t)(0)

emb_session_t *emb_create_session(size_t size)
{
    if(size < sizeof(emb_session_t)) {
        return NULL;
    }
    
    emb_session_ctl_t *session_ctl = EMB_CALLOC(1, sizeof(emb_session_ctl_t) + size - sizeof(emb_session_t));



    return &session_ctl->session;
}

void emb_destroy_session(emb_session_t *sn)
{
    emb_session_ctl_t *sn_ctl = container_of(sn, emb_session_ctl_t, session);

    EMB_FREE(sn_ctl);

    return;
}