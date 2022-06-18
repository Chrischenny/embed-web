#ifndef __EMB_HOOK_H__
#define __EMB_HOOK_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct emb_connection emb_conn_t;

typedef void (*emb_hook_fn)(emb_conn_t *, emb_hook_e ev,
                                   void *ev_data, void *fn_data);
void emb_resolve_hook(emb_conn_t *c, int ev, void *ev_data);
void emb_error(emb_conn_t *c, const char *fmt, ...);

typedef enum {
  MG_EV_ERROR,       // Error                        char *error_message
  MG_EV_OPEN,        // Connection created           NULL
  MG_EV_POLL,        // mg_mgr_poll iteration        uint64_t *milliseconds
  MG_EV_RESOLVE,     // Host name is resolved        NULL
  MG_EV_CONNECT,     // Connection established       NULL
  MG_EV_ACCEPT,      // Connection accepted          NULL
  MG_EV_READ,        // Data received from socket    struct mg_str *
  MG_EV_WRITE,       // Data written to socket       long *bytes_written
  MG_EV_CLOSE,       // Connection closed            NULL
  MG_EV_HTTP_MSG,    // HTTP request/response        struct mg_http_message *
  MG_EV_HTTP_CHUNK,  // HTTP chunk (partial msg)     struct mg_http_message *
  MG_EV_WS_OPEN,     // Websocket handshake done     struct mg_http_message *
  MG_EV_WS_MSG,      // Websocket msg, text or bin   struct mg_ws_message *
  MG_EV_WS_CTL,      // Websocket control msg        struct mg_ws_message *
  MG_EV_MQTT_CMD,    // MQTT low-level command       struct mg_mqtt_message *
  MG_EV_MQTT_MSG,    // MQTT PUBLISH received        struct mg_mqtt_message *
  MG_EV_MQTT_OPEN,   // MQTT CONNACK received        int *connack_status_code
  MG_EV_SNTP_TIME,   // SNTP time received           uint64_t *milliseconds
  MG_EV_USER,        // Starting ID for user events
}emb_hook_e;

#ifdef __cplusplus
}
#endif
#endif // __EMB_HOOK_H__