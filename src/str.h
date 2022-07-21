
#include "arch/arch.h"

struct emb_str {
  const char *ptr;  // Pointer to string data
  size_t len;       // String len
};

#define EMB_NULL_STR \
  { NULL, 0 }

#define EMB_C_STR(a) \
  { (a), sizeof(a) - 1 }

// Using macro to avoid shadowing C++ struct constructor, see #1298
#define emb_str(s) emb_str_s(s)

struct emb_str emb_str(const char *s);
struct emb_str emb_str_n(const char *s, size_t n);
int emb_lower(const char *s);
int emb_ncasecmp(const char *s1, const char *s2, size_t len);
int emb_casecmp(const char *s1, const char *s2);
int emb_vcmp(const struct emb_str *s1, const char *s2);
int emb_vcasecmp(const struct emb_str *str1, const char *str2);
int emb_strcmp(const struct emb_str str1, const struct emb_str str2);
struct emb_str emb_strstrip(struct emb_str s);
struct emb_str emb_strdup(const struct emb_str s);
const char *emb_strstr(const struct emb_str haystack, const struct emb_str needle);
bool emb_match(struct emb_str str, struct emb_str pattern, struct emb_str *caps);
bool emb_globmatch(const char *pattern, size_t plen, const char *s, size_t n);
bool emb_commalist(struct emb_str *s, struct emb_str *k, struct emb_str *v);
bool emb_split(struct emb_str *s, struct emb_str *k, struct emb_str *v, char delim);
size_t emb_vsnprintf(char *buf, size_t len, const char *fmt, va_list *ap);
size_t emb_snprintf(char *, size_t, const char *fmt, ...);
char *emb_hex(const void *buf, size_t len, char *dst);
void emb_unhex(const char *buf, size_t len, unsigned char *to);
unsigned long emb_unhexn(const char *s, size_t len);
size_t emb_asprintf(char **, size_t, const char *fmt, ...);
size_t emb_vasprintf(char **buf, size_t size, const char *fmt, va_list ap);
char *emb_mprintf(const char *fmt, ...);
char *emb_vmprintf(const char *fmt, va_list ap);
int emb_check_ip_acl(struct emb_str acl, uint32_t remote_ip);
int64_t emb_to64(struct emb_str str);
uint64_t emb_tou64(struct emb_str str);
size_t emb_lld(char *buf, int64_t val, bool is_signed, bool is_hex);
double emb_atod(const char *buf, int len, int *numlen);
size_t emb_dtoa(char *buf, size_t len, double d, int width);
