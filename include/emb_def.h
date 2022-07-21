/**
 * @file emb_def.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-06-18
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#ifndef __EMB_DEF_H__
#define __EMB_DEF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef EMB_CUSTOM_SETTINGS
#include "emb_settings"
#endif

#ifndef EMB_MALLOC 
#define EMB_MALLOC malloc
#endif

#ifndef EMB_CALLOC
#define EMB_CALLOC calloc
#endif

#ifndef EMB_REALLOC
#define EMB_REALLOC realloc
#endif

#ifndef EMB_FREE
#define EMB_FREE free
#endif

#ifndef EMB_STRDUP
#define EMB_STRDUP strdup
#endif

#ifndef EMB_LOG_ERR(code, fmt, ...)
#define EMB_LOG_ERR(code, fmt, ...) printf("error[%d], "fmt"\n", ##__VA_ARGS__)
#endif

#ifndef EMB_LOG_WARN(fmt, ...)
#define EMB_LOG_WARN(fmt, ...) printf("warn, "fmt"\n", ##__VA_ARGS__)
#endif

#ifndef EMB_LOG_INFO(fmt, ...)
#define EMB_LOG_INFO(fmt, ...) printf("info, "fmt"\n", ##__VA_ARGS__)
#endif

#ifndef EMB_LOG_DBG(fmt, ...)
#define EMB_LOG_DBG(fmt, ...) printf("debug, "fmt"\n", ##__VA_ARGS__)
#endif

#undef offsetof
#ifdef __compiler_offsetof
#define offsetof(type, member) __compiler_offsetof(type, member)
#else
#define offsetof(type, member) ((size_t) &((type *)0)->member)
#endif

#define container_of(ptr, type, member) ({ \
    const typeof(((type *)0)->member) *_ptr = (ptr); \
    (type *)((char *)_ptr - offsetof(type, member)); \
})


#ifdef __cplusplus
}
#endif
#endif // __EMB_DEF_H__
