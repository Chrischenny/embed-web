/**
 * @file http_session.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2022-07-02
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include "emb_common.h"
#include "emb_svr.h"
#include "emb_session.h"
#include "list.h"

typedef struct emb_http_session {
    emb_session_t emb_common_sn;
    list_head_t query_list;
    list_head_t context;
    list_head_t http_request_header_list;
    list_head_t http_response_header_list;
}emb_http_sn_t;



typedef struct emb_http_connection {
    emb_conn_t emb_common_conn;
    
    // reserve
}emb_http_conn_t;

typedef struct emb_http_server_manager {
    emb_mgr_t emb_common_mgr;

    uint32_t max_request_line_len;
    uint32_t max_common_body_len;
    uint32_t max_file_serve_buf_len;
}emb_http_svr_mgr_t;

#define GET_HTTP_CONN(ptr) (container_of(ptr, emb_http_conn_t, emb_common_conn))
#define GET_HTTP_SN(ptr) (container_of(ptr, emb_http_sn_t, emb_common_sn))
#define GET_HTTP_MGR(ptr) (container_of(ptr, emb_http_svr_mgr_t, emb_common_mgr))

// Multipart POST example:
// --xyz
// Content-Disposition: form-data; name="val"
//
// abcdef
// --xyz
// Content-Disposition: form-data; name="foo"; filename="a.txt"
// Content-Type: text/plain
//
// hello world
//
// --xyz--
// size_t emb_http_next_multipart(struct emb_str body, size_t ofs,
//                               struct emb_http_part *part)
// {
//     struct emb_str cd = emb_str_n("Content-Disposition", 19);
//     const char *s = body.ptr;
//     size_t b = ofs, h1, h2, b1, b2, max = body.len;

//     // Init part params
//     if (part != NULL)
//         part->name = part->filename = part->body = emb_str_n(0, 0);

//     // Skip boundary
//     while (b + 2 < max && s[b] != '\r' && s[b + 1] != '\n')
//         b++;
//     if (b <= ofs || b + 2 >= max)
//         return 0;
//     // EMB_INFO(("B: %zu %zu [%.*s]", ofs, b - ofs, (int) (b - ofs), s));

//     // Skip headers
//     h1 = h2 = b + 2;
//     for (;;)
//     {
//         while (h2 + 2 < max && s[h2] != '\r' && s[h2 + 1] != '\n')
//             h2++;
//         if (h2 == h1)
//             break;
//         if (h2 + 2 >= max)
//             return 0;
//         // EMB_INFO(("Header: [%.*s]", (int) (h2 - h1), &s[h1]));
//         if (part != NULL && h1 + cd.len + 2 < h2 && s[h1 + cd.len] == ':' &&
//             emb_ncasecmp(&s[h1], cd.ptr, cd.len) == 0)
//         {
//             struct emb_str v = emb_str_n(&s[h1 + cd.len + 2], h2 - (h1 + cd.len + 2));
//             part->name = emb_http_get_header_var(v, emb_str_n("name", 4));
//             part->filename = emb_http_get_header_var(v, emb_str_n("filename", 8));
//         }
//         h1 = h2 = h2 + 2;
//     }
//     b1 = b2 = h2 + 2;
//     while (b2 + 2 + (b - ofs) + 2 < max && !(s[b2] == '\r' && s[b2 + 1] == '\n' &&
//                                              memcmp(&s[b2 + 2], s, b - ofs) == 0))
//         b2++;

//     if (b2 + 2 >= max)
//         return 0;
//     if (part != NULL)
//         part->body = emb_str_n(&s[b1], b2 - b1);
//     // EMB_INFO(("Body: [%.*s]", (int) (b2 - b1), &s[b1]));
//     return b2 + 2;
// }

// void emb_http_bauth(struct emb_connection *c, const char *user,
//                    const char *pass)
// {
//     struct emb_str u = emb_str(user), p = emb_str(pass);
//     size_t need = c->send.len + 36 + (u.len + p.len) * 2;
//     if (c->send.size < need)
//         emb_iobuf_resize(&c->send, need);
//     if (c->send.size >= need)
//     {
//         int i, n = 0;
//         char *buf = (char *)&c->send.buf[c->send.len + 21];
//         memcpy(&buf[-21], "Authorization: Basic ", 21); // DON'T use emb_send!
//         for (i = 0; i < (int)u.len; i++)
//         {
//             n = emb_base64_update(((unsigned char *)u.ptr)[i], buf, n);
//         }
//         if (p.len > 0)
//         {
//             n = emb_base64_update(':', buf, n);
//             for (i = 0; i < (int)p.len; i++)
//             {
//                 n = emb_base64_update(((unsigned char *)p.ptr)[i], buf, n);
//             }
//         }
//         n = emb_base64_final(buf, n);
//         c->send.len += 21 + (size_t)n + 2;
//         memcpy(&c->send.buf[c->send.len - 2], "\r\n", 2);
//     }
//     else
//     {
//         EMB_ERROR(("%lu %s cannot resize iobuf %d->%d ", c->id, c->label,
//                   (int)c->send.size, (int)need));
//     }
// }

struct emb_str emb_http_var(struct emb_str buf, struct emb_str name)
{
    struct emb_str k, v, result = emb_str_n(NULL, 0);
    while (emb_split(&buf, &k, &v, '&'))
    {
        if (name.len == k.len && emb_ncasecmp(name.ptr, k.ptr, k.len) == 0)
        {
            result = v;
            break;
        }
    }
    return result;
}

int emb_http_get_var(const struct emb_str *buf, const char *name, char *dst,
                    size_t dst_len)
{
    int len;
    if (dst == NULL || dst_len == 0)
    {
        len = -2; // Bad destination
    }
    else if (buf->ptr == NULL || name == NULL || buf->len == 0)
    {
        len = -1; // Bad source
        dst[0] = '\0';
    }
    else
    {
        struct emb_str v = emb_http_var(*buf, emb_str(name));
        if (v.ptr == NULL)
        {
            len = -4; // Name does not exist
        }
        else
        {
            len = emb_url_decode(v.ptr, v.len, dst, dst_len, 1);
            if (len < 0)
                len = -3; // Failed to decode
        }
    }
    return len;
}

static bool isx(int c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

int emb_url_decode(const char *src, size_t src_len, char *dst, size_t dst_len,
                  int is_form_url_encoded)
{
    size_t i, j;
    for (i = j = 0; i < src_len && j + 1 < dst_len; i++, j++)
    {
        if (src[i] == '%')
        {
            // Use `i + 2 < src_len`, not `i < src_len - 2`, note small src_len
            if (i + 2 < src_len && isx(src[i + 1]) && isx(src[i + 2]))
            {
                emb_unhex(src + i + 1, 2, (uint8_t *)&dst[j]);
                i += 2;
            }
            else
            {
                return -1;
            }
        }
        else if (is_form_url_encoded && src[i] == '+')
        {
            dst[j] = ' ';
        }
        else
        {
            dst[j] = src[i];
        }
    }
    if (j < dst_len)
        dst[j] = '\0'; // Null-terminate the destination
    return i >= src_len && j < dst_len ? (int)j : -1;
}

static bool is_valid_char(uint8_t c)
{
    return c == '\n' || c == '\r' || c >= ' ';
}

int get_http_request_line_len(const unsigned char *buf, size_t buf_len)
{
    size_t i;
    for (i = 0; i < buf_len; i++)
    {
        if (!is_valid_char(buf[i]))
            return -1;
        if ((i > 0 && buf[i] == '\n' && buf[i - 1] == '\n') ||
            (i > 3 && buf[i] == '\n' && buf[i - 1] == '\r' && buf[i - 2] == '\n'))
            return (int)i + 1;
    }
    return 0;
}

static const char *skip(const char *s, const char *e, const char *d,
                        struct emb_str *v)
{
    v->ptr = s;
    while (s < e && *s != '\n' && strchr(d, *s) == NULL)
        s++;
    v->len = (size_t)(s - v->ptr);
    while (s < e && strchr(d, *s) != NULL)
        s++;
    return s;
}

struct emb_str *emb_http_get_header(struct emb_http_message *h, const char *name)
{
    size_t i, n = strlen(name), max = sizeof(h->headers) / sizeof(h->headers[0]);
    for (i = 0; i < max && h->headers[i].name.len > 0; i++)
    {
        struct emb_str *k = &h->headers[i].name, *v = &h->headers[i].value;
        if (n == k->len && emb_ncasecmp(k->ptr, name, n) == 0)
            return v;
    }
    return NULL;
}

static void emb_http_parse_request_line(const char *s, const char *end, int max_headers)
{
    int i;
    for (i = 0; i < max_headers; i++)
    {
        struct emb_str k, v, tmp;
        const char *he = skip(s, end, "\n", &tmp);
        s = skip(s, he, ": \r\n", &k);
        s = skip(s, he, "\r\n", &v);
        if (k.len == tmp.len)
            continue;
        while (v.len > 0 && v.ptr[v.len - 1] == ' ')
            v.len--; // Trim spaces
        if (k.len == 0)
            break;
        // EMB_INFO(("--HH [%.*s] [%.*s] [%.*s]", (int) tmp.len - 1, tmp.ptr,
        //(int) k.len, k.ptr, (int) v.len, v.ptr));
        h[i].name = k;
        h[i].value = v;
    }
}

static int emb_http_parse_query(emb_http_sn_t *sn, char *s)
{

}

static int emb_http_parse_first_line(emb_http_sn_t *sn, char *s, char **head)
{

}

static int emb_http_parse_headers(emb_http_sn_t *sn, char *s, const char *end)
{

}

int emd_http_preparse(emb_conn_t *conn, emb_hook_e ev, void *ev_data, void *fn_data)
{
    char *s = conn->recv.buf;
    int ret = -1;
    emb_http_sn_t *sn = NULL;
    char method[16] = {0};
    char version[10] = {0};
    
    do
    {
        int req_len = get_http_request_line_len((unsigned char *)s, conn->recv.len);
        const char *end = s + req_len;

        if(req_len < 0 || req_len < GET_HTTP_MGR(conn->mgr)->max_request_line_len) {
            break;
        }
        ret = emb_get_method(s);
        if(ret) {break;}
        ret = emb_http_get_version(s);
        if(ret) {break;}
        sn = (emb_http_sn_t *)emb_create_session(sizeof(*sn));
        char *headers = emb_http_parse_first_line(sn, s, end);
        ret = emb_http_parse_headers(sn, headers, end);
    } while (false);
    
    if(ret) {
        if(sn) {
            emb_destroy_session((emb_session_t *)sn);
        }

        return ret;
    }
}

int emb_http_parse(const char *s, size_t len, struct emb_http_message *hm)
{
    int is_response, req_len = get_http_request_line_len((unsigned char *)s, len);
    const char *end = s + req_len, *qs;
    struct emb_str *cl;

    memset(hm, 0, sizeof(*hm));
    if (req_len <= 0)
        return req_len;

    hm->message.ptr = hm->head.ptr = s;
    hm->body.ptr = end;
    hm->head.len = (size_t)req_len;
    hm->chunk.ptr = end;
    hm->message.len = hm->body.len = (size_t)~0; // Set body length to infinite

    // Parse request line
    s = skip(s, end, " ", &hm->method);
    s = skip(s, end, " ", &hm->uri);
    s = skip(s, end, "\r\n", &hm->proto);

    // Sanity check. Allow protocol/reason to be empty
    if (hm->method.len == 0 || hm->uri.len == 0)
        return -1;

    // If URI contains '?' character, setup query string
    if ((qs = (const char *)memchr(hm->uri.ptr, '?', hm->uri.len)) != NULL)
    {
        hm->query.ptr = qs + 1;
        hm->query.len = (size_t)(&hm->uri.ptr[hm->uri.len] - (qs + 1));
        hm->uri.len = (size_t)(qs - hm->uri.ptr);
    }

    emb_http_parse_request_line(s, end, hm->headers,
                          sizeof(hm->headers) / sizeof(hm->headers[0]));
    if ((cl = emb_http_get_header(hm, "Content-Length")) != NULL)
    {
        hm->body.len = (size_t)emb_to64(*cl);
        hm->message.len = (size_t)req_len + hm->body.len;
    }

    // emb_http_parse() is used to parse both HTTP requests and HTTP
    // responses. If HTTP response does not have Content-Length set, then
    // body is read until socket is closed, i.e. body.len is infinite (~0).
    //
    // For HTTP requests though, according to
    // http://tools.ietf.org/html/rfc7231#section-8.1.3,
    // only POST and PUT methods have defined body semantics.
    // Therefore, if Content-Length is not specified and methods are
    // not one of PUT or POST, set body length to 0.
    //
    // So, if it is HTTP request, and Content-Length is not set,
    // and method is not (PUT or POST) then reset body length to zero.
    is_response = emb_ncasecmp(hm->method.ptr, "HTTP/", 5) == 0;
    if (hm->body.len == (size_t)~0 && !is_response &&
        emb_vcasecmp(&hm->method, "PUT") != 0 &&
        emb_vcasecmp(&hm->method, "POST") != 0)
    {
        hm->body.len = 0;
        hm->message.len = (size_t)req_len;
    }

    // The 204 (No content) responses also have 0 body length
    if (hm->body.len == (size_t)~0 && is_response &&
        emb_vcasecmp(&hm->uri, "204") == 0)
    {
        hm->body.len = 0;
        hm->message.len = (size_t)req_len;
    }

    return req_len;
}