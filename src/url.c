#include "url.h"
#include <stdlib.h>

struct url_el_off
{
    int16_t key;
    int16_t user;
    int16_t pass;
    int16_t host;
    int16_t port;
    int16_t uri;
    int16_t end;
};

int emb_url_is_ssl(const char *url)
{
    return strncmp(url, "wss:", 4) == 0 || strncmp(url, "https:", 6) == 0 ||
           strncmp(url, "mqtts:", 6) == 0 || strncmp(url, "ssl:", 4) == 0 ||
           strncmp(url, "tls:", 4) == 0;
}

static struct url_el_off url_parse(const char *url)
{
    uint16_t i;
    struct url_el_off u;
    memset(&u, 0, sizeof(u));
    for (i = 0; url[i] != '\0'; i++)
    {
        if (i > 0 && u.host == 0 && url[i - 1] == '/' && url[i] == '/')
        {
            u.host = i + 1;
            u.port = 0;
        }
        else if (url[i] == ']')
        {
            u.port = 0; // IPv6 URLs, like http://[::1]/bar
        }
        else if (url[i] == ':' && u.port == 0 && u.uri == 0)
        {
            u.port = i + 1;
        }
        else if (url[i] == '@' && u.user == 0 && u.pass == 0)
        {
            u.user = u.host;
            u.pass = u.port;
            u.host = i + 1;
            u.port = 0;
        }
        else if (u.host && u.uri == 0 && url[i] == '/')
        {
            u.uri = i;
        }
    }
    u.end = i;
#if 0
  printf("[%s] %d %d %d %d %d\n", url, u.user, u.pass, u.host, u.port, u.uri);
#endif
    return u;
}

struct emb_str emb_url_host(const char *url)
{
    struct url_el_off u = url_parse(url);
    uint16_t n = u.port  ? u.port - u.host - 1
               : u.uri ? u.uri - u.host
                       : u.end - u.host;
    struct emb_str s = emb_str_n(url + u.host, n);
    return s;
}

const char *emb_url_uri(const char *url)
{
    struct url_el_off u = url_parse(url);
    return u.uri ? url + u.uri : "/";
}

unsigned short emb_url_port(const char *url)
{
    struct url_el_off u = url_parse(url);
    unsigned short port = 0;
    if (strncmp(url, "http:", 5) == 0 || strncmp(url, "ws:", 3) == 0)
        port = 80;
    if (strncmp(url, "wss:", 4) == 0 || strncmp(url, "https:", 6) == 0)
        port = 443;
    if (strncmp(url, "mqtt:", 5) == 0)
        port = 1883;
    if (strncmp(url, "mqtts:", 6) == 0)
        port = 8883;
    if (u.port)
        port = (unsigned short)atoi(url + u.port);
    return port;
}

struct emb_str emb_url_user(const char *url)
{
    struct url_el_off u = url_parse(url);
    struct emb_str s = emb_str("");
    if (u.user && (u.pass || u.host))
    {
        size_t n = u.pass ? u.pass - u.user - 1 : u.host - u.user - 1;
        s = emb_str_n(url + u.user, n);
    }
    return s;
}

struct emb_str emb_url_pass(const char *url)
{
    struct url_el_off u = url_parse(url);
    struct emb_str s = emb_str_n("", 0UL);
    if (u.pass && u.host)
    {
        size_t n = u.host - u.pass - 1;
        s = emb_str_n(url + u.pass, n);
    }
    return s;
}
