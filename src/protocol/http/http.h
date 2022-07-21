#pragma once

#include "arch.h"
#include "config.h"
#include "fs.h"
#include "net.h"
#include "str.h"

struct emb_http_header {
  struct emb_str name;   // Header name
  struct emb_str value;  // Header value
};

struct emb_http_message {
  struct emb_str method, uri, query, proto;             // Request/response line
  struct emb_http_header headers[EMB_MAX_HTTP_HEADERS];  // Headers
  struct emb_str body;                                  // Body
  struct emb_str head;                                  // Request + headers
  struct emb_str chunk;    // Chunk for chunked encoding,  or partial body
  struct emb_str message;  // Request + headers + body
};

// Parameter for emb_http_serve_dir()
struct emb_http_serve_opts {
  const char *root_dir;       // Web root directory, must be non-NULL
  const char *ssi_pattern;    // SSI file name pattern, e.g. #.shtml
  const char *extra_headers;  // Extra HTTP headers to add in responses
  const char *mime_types;     // Extra mime types, ext1=type1,ext2=type2,..
  const char *page404;        // Path to the 404 page, or NULL by default
  struct emb_fs *fs;           // Filesystem implementation. Use NULL for POSIX
};

// Parameter for emb_http_next_multipart
struct emb_http_part {
  struct emb_str name;      // Form field name
  struct emb_str filename;  // Filename for file uploads
  struct emb_str body;      // Part contents
};

int emb_http_parse(const char *s, size_t len, struct emb_http_message *);
int emb_http_get_request_len(const unsigned char *buf, size_t buf_len);
void emb_http_printf_chunk(struct emb_connection *cnn, const char *fmt, ...);
void emb_http_write_chunk(struct emb_connection *c, const char *buf, size_t len);
void emb_http_delete_chunk(struct emb_connection *c, struct emb_http_message *hm);
struct emb_connection *emb_http_listen(struct emb_mgr *, const char *url,
                                     emb_event_handler_t fn, void *fn_data);
struct emb_connection *emb_http_connect(struct emb_mgr *, const char *url,
                                      emb_event_handler_t fn, void *fn_data);
void emb_http_serve_dir(struct emb_connection *, struct emb_http_message *hm,
                       const struct emb_http_serve_opts *);
void emb_http_serve_file(struct emb_connection *, struct emb_http_message *hm,
                        const char *path, const struct emb_http_serve_opts *);
void emb_http_reply(struct emb_connection *, int status_code, const char *headers,
                   const char *body_fmt, ...);
struct emb_str *emb_http_get_header(struct emb_http_message *, const char *name);
struct emb_str emb_http_var(struct emb_str buf, struct emb_str name);
int emb_http_get_var(const struct emb_str *, const char *name, char *, size_t);
int emb_url_decode(const char *s, size_t n, char *to, size_t to_len, int form);
size_t emb_url_encode(const char *s, size_t n, char *buf, size_t len);
void emb_http_creds(struct emb_http_message *, char *, size_t, char *, size_t);
bool emb_http_match_uri(const struct emb_http_message *, const char *glob);
int emb_http_upload(struct emb_connection *, struct emb_http_message *hm,
                   struct emb_fs *fs, const char *dir);
void emb_http_bauth(struct emb_connection *, const char *user, const char *pass);
struct emb_str emb_http_get_header_var(struct emb_str s, struct emb_str v);
size_t emb_http_next_multipart(struct emb_str, size_t, struct emb_http_part *);
int emb_http_status(const struct emb_http_message *hm);
