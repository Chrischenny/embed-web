#ifndef __EMB_HOOK_H__
#define __EMB_HOOK_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct emb_connection emb_conn_t;

typedef void (emb_hook_fn)(emb_conn_t *, emb_hook_e ev,
                                   void *ev_data, void *fn_data);
void emb_resolve_hook(emb_conn_t *c, int ev, void *ev_data);
void emb_error(emb_conn_t *c, const char *fmt, ...);

typedef enum {
  EMB_EV_ERROR,       // Error                        char *error_message
  EMB_EV_OPEN,        // Connection created           NULL
  EMB_EV_POLL,        // emb_mgr_poll iteration        uint64_t *milliseconds
  EMB_EV_RESOLVE,     // Host name is resolved        NULL
  EMB_EV_CONNECT,     // Connection established       NULL
  EMB_EV_ACCEPT,      // Connection accepted          NULL
  EMB_EV_READ,        // Data received from socket    struct emb_str *
  EMB_EV_WRITE,       // Data written to socket       long *bytes_written
  EMB_EV_CLOSE,       // Connection closed            NULL
  EMB_EV_HTTP_MSG,    // HTTP request/response        struct emb_http_message *
  EMB_EV_HTTP_CHUNK,  // HTTP chunk (partial msg)     struct emb_http_message *
  EMB_EV_WS_OPEN,     // Websocket handshake done     struct emb_http_message *
  EMB_EV_WS_MSG,      // Websocket msg, text or bin   struct emb_ws_message *
  EMB_EV_WS_CTL,      // Websocket control msg        struct emb_ws_message *
  EMB_EV_MQTT_CMD,    // MQTT low-level command       struct emb_mqtt_message *
  EMB_EV_MQTT_MSG,    // MQTT PUBLISH received        struct emb_mqtt_message *
  EMB_EV_MQTT_OPEN,   // MQTT CONNACK received        int *connack_status_code
  EMB_EV_SNTP_TIME,   // SNTP time received           uint64_t *milliseconds
  EMB_EV_USER,        // Starting ID for user events
}emb_hook_e;

#ifdef __cplusplus
}
#endif
#endif // __EMB_HOOK_H__