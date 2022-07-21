#include "event.h"
#include "log.h"
#include "net.h"
#include "util.h"

void emb_call(struct emb_connection *c, int ev, void *ev_data) {
  // Run user-defined handler first, in order to give it an ability
  // to intercept processing (e.g. clean input buffer) before the
  // protocol handler kicks in
  if (c->fn != NULL) c->fn(c, ev, ev_data, c->fn_data);
  if (c->pfn != NULL) c->pfn(c, ev, ev_data, c->pfn_data);
}

void emb_error(struct emb_connection *c, const char *fmt, ...) {
  char mem[256], *buf = mem;
  va_list ap;
  va_start(ap, fmt);
  emb_vasprintf(&buf, sizeof(mem), fmt, ap);
  va_end(ap);
  EMB_ERROR(("%lu %p %s", c->id, c->fd, buf));
  c->is_closing = 1;             // Set is_closing before sending EMB_EV_CALL
  emb_call(c, EMB_EV_ERROR, buf);  // Let user handler to override it
  if (buf != mem) free(buf);
}
