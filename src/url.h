#pragma once
#include "str.h"

unsigned short emb_url_port(const char *url);
int emb_url_is_ssl(const char *url);
struct emb_str emb_url_host(const char *url);
struct emb_str emb_url_user(const char *url);
struct emb_str emb_url_pass(const char *url);
const char *emb_url_uri(const char *url);
