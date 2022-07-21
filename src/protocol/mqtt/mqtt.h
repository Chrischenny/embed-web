#pragma once

#include "net.h"
#include "str.h"

#define MQTT_CMD_CONNECT 1
#define MQTT_CMD_CONNACK 2
#define MQTT_CMD_PUBLISH 3
#define MQTT_CMD_PUBACK 4
#define MQTT_CMD_PUBREC 5
#define MQTT_CMD_PUBREL 6
#define MQTT_CMD_PUBCOMP 7
#define MQTT_CMD_SUBSCRIBE 8
#define MQTT_CMD_SUBACK 9
#define MQTT_CMD_UNSUBSCRIBE 10
#define MQTT_CMD_UNSUBACK 11
#define MQTT_CMD_PINGREQ 12
#define MQTT_CMD_PINGRESP 13
#define MQTT_CMD_DISCONNECT 14

struct emb_mqtt_opts {
  struct emb_str user;          // Username, can be empty
  struct emb_str pass;          // Password, can be empty
  struct emb_str client_id;     // Client ID
  struct emb_str will_topic;    // Will topic
  struct emb_str will_message;  // Will message
  uint8_t will_qos;            // Will message quality of service
  bool will_retain;            // Retain last will
  bool clean;                  // Use clean session, 0 or 1
  uint16_t keepalive;          // Keep-alive timer in seconds
};

struct emb_mqtt_message {
  struct emb_str topic;  // Parsed topic
  struct emb_str data;   // Parsed message
  struct emb_str dgram;  // Whole MQTT datagram, including headers
  uint16_t id;  // Set for PUBACK, PUBREC, PUBREL, PUBCOMP, SUBACK, PUBLISH
  uint8_t cmd;  // MQTT command, one of MQTT_CMD_*
  uint8_t qos;  // Quality of service
  uint8_t ack;  // Connack return code. 0 - success
};

struct emb_connection *emb_mqtt_connect(struct emb_mgr *, const char *url,
                                      const struct emb_mqtt_opts *opts,
                                      emb_event_handler_t fn, void *fn_data);
struct emb_connection *emb_mqtt_listen(struct emb_mgr *mgr, const char *url,
                                     emb_event_handler_t fn, void *fn_data);
void emb_mqtt_login(struct emb_connection *c, const struct emb_mqtt_opts *opts);
void emb_mqtt_pub(struct emb_connection *c, struct emb_str topic,
                 struct emb_str data, int qos, bool retain);
void emb_mqtt_sub(struct emb_connection *, struct emb_str topic, int qos);
int emb_mqtt_parse(const uint8_t *buf, size_t len, struct emb_mqtt_message *m);
void emb_mqtt_send_header(struct emb_connection *, uint8_t cmd, uint8_t flags,
                         uint32_t len);
size_t emb_mqtt_next_sub(struct emb_mqtt_message *msg, struct emb_str *topic,
                        uint8_t *qos, size_t pos);
size_t emb_mqtt_next_unsub(struct emb_mqtt_message *msg, struct emb_str *topic,
                          size_t pos);
void emb_mqtt_ping(struct emb_connection *);
void emb_mqtt_pong(struct emb_connection *);
void emb_mqtt_disconnect(struct emb_connection *);
