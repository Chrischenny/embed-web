/**
 * @file emb_svr.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2022-06-18
 *
 * @copyright Copyright (c) 2022
 *
 */

// #include "net.h"
// #include "dns.h"
// #include "log.h"
// #include "timer.h"
// #include "tls.h"
// #include "util.h"
#include "emb_def.h"
#include "emb_svr.h"
#include "str.h"

size_t emb_vprintf(struct emb_connection *c, const char *fmt, va_list ap)
{
    char mem[256], *buf = mem;
    size_t len = emb_vasprintf(&buf, sizeof(mem), fmt, ap);
    len = emb_send(c, buf, len);
    if (buf != mem)
        free(buf);
    return len;
}

size_t emb_printf(struct emb_connection *c, const char *fmt, ...)
{
    size_t len = 0;
    va_list ap;
    va_start(ap, fmt);
    len = emb_vprintf(c, fmt, ap);
    va_end(ap);
    return len;
}

char *emb_straddr(struct emb_addr *a, char *buf, size_t len)
{
    char tmp[30];
    const char *fmt = a->is_ip6 ? "[%s]:%d" : "%s:%d";
    emb_ntoa(a, tmp, sizeof(tmp));
    emb_snprintf(buf, len, fmt, tmp, (int)emb_ntohs(a->port));
    return buf;
}

char *emb_ntoa(const struct emb_addr *addr, char *buf, size_t len)
{
    if (addr->is_ip6)
    {
        uint16_t *p = (uint16_t *)addr->ip6;
        emb_snprintf(buf, len, "%x:%x:%x:%x:%x:%x:%x:%x", emb_htons(p[0]),
                    emb_htons(p[1]), emb_htons(p[2]), emb_htons(p[3]), emb_htons(p[4]),
                    emb_htons(p[5]), emb_htons(p[6]), emb_htons(p[7]));
    }
    else
    {
        uint8_t p[4];
        memcpy(p, &addr->ip, sizeof(p));
        emb_snprintf(buf, len, "%d.%d.%d.%d", (int)p[0], (int)p[1], (int)p[2],
                    (int)p[3]);
    }
    return buf;
}

static bool emb_atonl(struct emb_str str, struct emb_addr *addr)
{
    if (emb_vcasecmp(&str, "localhost") != 0)
        return false;
    addr->ip = emb_htonl(0x7f000001);
    addr->is_ip6 = false;
    return true;
}

static bool emb_atone(struct emb_str str, struct emb_addr *addr)
{
    if (str.len > 0)
        return false;
    addr->ip = 0;
    addr->is_ip6 = false;
    return true;
}

static bool emb_aton4(struct emb_str str, struct emb_addr *addr)
{
    uint8_t data[4] = {0, 0, 0, 0};
    size_t i, num_dots = 0;
    for (i = 0; i < str.len; i++)
    {
        if (str.ptr[i] >= '0' && str.ptr[i] <= '9')
        {
            int octet = data[num_dots] * 10 + (str.ptr[i] - '0');
            if (octet > 255)
                return false;
            data[num_dots] = (uint8_t)octet;
        }
        else if (str.ptr[i] == '.')
        {
            if (num_dots >= 3 || i == 0 || str.ptr[i - 1] == '.')
                return false;
            num_dots++;
        }
        else
        {
            return false;
        }
    }
    if (num_dots != 3 || str.ptr[i - 1] == '.')
        return false;
    memcpy(&addr->ip, data, sizeof(data));
    addr->is_ip6 = false;
    return true;
}

static bool emb_v4mapped(struct emb_str str, struct emb_addr *addr)
{
    int i;
    if (str.len < 14)
        return false;
    if (str.ptr[0] != ':' || str.ptr[1] != ':' || str.ptr[6] != ':')
        return false;
    for (i = 2; i < 6; i++)
    {
        if (str.ptr[i] != 'f' && str.ptr[i] != 'F')
            return false;
    }
    if (!emb_aton4(emb_str_n(&str.ptr[7], str.len - 7), addr))
        return false;
    memset(addr->ip6, 0, sizeof(addr->ip6));
    addr->ip6[10] = addr->ip6[11] = 255;
    memcpy(&addr->ip6[12], &addr->ip, 4);
    addr->is_ip6 = true;
    return true;
}

static bool emb_aton6(struct emb_str str, struct emb_addr *addr)
{
    size_t i, j = 0, n = 0, dc = 42;
    if (str.len > 2 && str.ptr[0] == '[')
        str.ptr++, str.len -= 2;
    if (emb_v4mapped(str, addr))
        return true;
    for (i = 0; i < str.len; i++)
    {
        if ((str.ptr[i] >= '0' && str.ptr[i] <= '9') ||
            (str.ptr[i] >= 'a' && str.ptr[i] <= 'f') ||
            (str.ptr[i] >= 'A' && str.ptr[i] <= 'F'))
        {
            unsigned long val;
            if (i > j + 3)
                return false;
            // EMB_DEBUG(("%zu %zu [%.*s]", i, j, (int) (i - j + 1), &str.ptr[j]));
            val = emb_unhexn(&str.ptr[j], i - j + 1);
            addr->ip6[n] = (uint8_t)((val >> 8) & 255);
            addr->ip6[n + 1] = (uint8_t)(val & 255);
        }
        else if (str.ptr[i] == ':')
        {
            j = i + 1;
            if (i > 0 && str.ptr[i - 1] == ':')
            {
                dc = n; // Double colon
                if (i > 1 && str.ptr[i - 2] == ':')
                    return false;
            }
            else if (i > 0)
            {
                n += 2;
            }
            if (n > 14)
                return false;
            addr->ip6[n] = addr->ip6[n + 1] = 0; // For trailing ::
        }
        else
        {
            return false;
        }
    }
    if (n < 14 && dc == 42)
        return false;
    if (n < 14)
    {
        memmove(&addr->ip6[dc + (14 - n)], &addr->ip6[dc], n - dc + 2);
        memset(&addr->ip6[dc], 0, 14 - n);
    }
    addr->is_ip6 = true;
    return true;
}

bool emb_aton(struct emb_str str, struct emb_addr *addr)
{
    // EMB_INFO(("[%.*s]", (int) str.len, str.ptr));
    return emb_atone(str, addr) || emb_atonl(str, addr) || emb_aton4(str, addr) ||
           emb_aton6(str, addr);
}

emb_conn_t *emb_alloc_conn(emb_mgr_t *mgr, size_t size)
{
    if (size < (sizeof(emb_conn_t)))
    {
        // TODO:log
        return NULL;
    }
    emb_conn_t *c = (emb_conn_t *)EMB_CALLOC(1, size);
    if (c != NULL)
    {
        c->mgr = mgr;
        c->id = ++mgr->nextid;
    }
    return c;
}

void emb_close_conn(struct emb_connection *c)
{
    emb_resolve_cancel(c); // Close any pending DNS query
    LIST_DELETE(struct emb_connection, &c->mgr->conns, c);
    if (c == c->mgr->dns4.c)
        c->mgr->dns4.c = NULL;
    if (c == c->mgr->dns6.c)
        c->mgr->dns6.c = NULL;
    // Order of operations is important. `EMB_EV_CLOSE` event must be fired
    // before we deallocate received data, see #1331
    emb_call(c, EMB_EV_CLOSE, NULL);
    EMB_DEBUG(("%lu closed", c->id));

    emb_tls_free(c);
    emb_iobuf_free(&c->recv);
    emb_iobuf_free(&c->send);
    memset(c, 0, sizeof(*c));
    free(c);
}

struct emb_connection *emb_connect(struct emb_mgr *mgr, const char *url,
                                 emb_event_handler_t fn, void *fn_data)
{
    struct emb_connection *c = NULL;
    if (url == NULL || url[0] == '\0')
    {
        EMB_ERROR(("null url"));
    }
    else if ((c = emb_alloc_conn(mgr)) == NULL)
    {
        EMB_ERROR(("OOM"));
    }
    else
    {
        LIST_ADD_HEAD(struct emb_connection, &mgr->conns, c);
        c->is_udp = (strncmp(url, "udp:", 4) == 0);
        c->fn = fn;
        c->is_client = true;
        c->fd = (void *)(size_t)-1; // Set to invalid socket
        c->fn_data = fn_data;
        EMB_DEBUG(("%lu -1 %s", c->id, url));
        emb_call(c, EMB_EV_OPEN, NULL);
        emb_resolve(c, url);
    }
    return c;
}

emb_conn_t *emb_listen(emb_mgr_t *mgr, const char *url, emb_event_handler_t fn, void *fn_data)
{
    emb_conn_t *c = NULL;
    if ((c = emb_alloc_conn(mgr, sizeof(emb_conn_t))) == NULL)
    {
        EMB_ERROR(("OOM %s", url));
    }
    else if (!emb_open_listener(c, url))
    {
        EMB_ERROR(("Failed: %s, errno %d", url, errno));
        free(c);
        c = NULL;
    }
    else
    {
        c->is_listening = 1;
        c->is_udp = strncmp(url, "udp:", 4) == 0;
        LIST_ADD_HEAD(struct emb_connection, &mgr->conns, c);
        c->fn = fn;
        c->fn_data = fn_data;
        emb_call(c, EMB_EV_OPEN, NULL);
        EMB_DEBUG(("%lu %p %s", c->id, c->fd, url));
    }
    return c;
}

struct emb_connection *emb_wrapfd(struct emb_mgr *mgr, int fd,
                                emb_event_handler_t fn, void *fn_data)
{
    struct emb_connection *c = emb_alloc_conn(mgr);
    if (c != NULL)
    {
        c->fd = (void *)(size_t)fd;
        c->fn = fn;
        c->fn_data = fn_data;
        emb_call(c, EMB_EV_OPEN, NULL);
        LIST_ADD_HEAD(struct emb_connection, &mgr->conns, c);
    }
    return c;
}

struct emb_timer *emb_timer_add(struct emb_mgr *mgr, uint64_t milliseconds,
                              unsigned flags, void (*fn)(void *), void *arg)
{
    struct emb_timer *t = (struct emb_timer *)calloc(1, sizeof(*t));
    emb_timer_init(&mgr->timers, t, milliseconds, flags, fn, arg);
    return t;
}

void emb_mgr_free(struct emb_mgr *mgr)
{
    struct emb_connection *c;
    struct emb_timer *tmp, *t = mgr->timers;
    while (t != NULL)
        tmp = t->next, free(t), t = tmp;
    mgr->timers = NULL; // Important. Next call to poll won't touch timers
    for (c = mgr->conns; c != NULL; c = c->next)
        c->is_closing = 1;
    emb_mgr_poll(mgr, 0);
#if EMB_ARCH == EMB_ARCH_FREERTOS_TCP
    FreeRTOS_DeleteSocketSet(mgr->ss);
#endif
    EMB_DEBUG(("All connections closed"));
}

void emb_mgr_init(struct emb_mgr *mgr)
{
    memset(mgr, 0, sizeof(*mgr));
#if EMB_ARCH == EMB_ARCH_WIN32 && EMB_ENABLE_WINSOCK
    // clang-format off
  { WSADATA data; WSAStartup(MAKEWORD(2, 2), &data); }
    // clang-format on
#elif EMB_ARCH == EMB_ARCH_FREERTOS_TCP
    mgr->ss = FreeRTOS_CreateSocketSet();
#elif defined(__unix) || defined(__unix__) || defined(__APPLE__)
    // Ignore SIGPIPE signal, so if client cancels the request, it
    // won't kill the whole process.
    signal(SIGPIPE, SIG_IGN);
#endif
    mgr->dnstimeout = 3000;
    mgr->dns4.url = "udp://8.8.8.8:53";
    mgr->dns6.url = "udp://[2001:4860:4860::8888]:53";
}
