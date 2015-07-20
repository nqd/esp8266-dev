#ifndef _FOTA_VERSION_H_
#define _FOTA_VERSION_H_

#include "osapi.h"

#define pHeadStatic "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: ESP8266\r\n\
Accept: */*\r\n\
Content-Type: application/json\r\n"

#define pHeadAuthen "api-key: %s\r\n"

#define FOTA_CLIENT   "ESP8266"
#define UUID_LEN      24
#define TOKEN_LEN     64

#define VERSION_DIGIT_MAX   255             // max value of 1B
#define VERSION_BYTE        3               // MAJOR.MINOR.PATCH

#ifndef FOTA_SECURE
#define FOTA_SECURE   0
#endif

enum fota_status {
  FOTA_POLLING,
  FOTA_GETTING_FIRMWARE,
  FOTA_IDLE
};

typedef struct {
  char *host;
  char *url;
  uint16_t port;
  uint8_t secure;
  struct espconn *conn;
  struct upgrade_server_info *up_server;
  void *reverse;
} fota_cdn_t;

typedef struct {
  uint16_t port;
  char *uuid;
  char *token;
  char *host;
  uint32_t interval;
  struct espconn *conn;
  os_timer_t periodic;
  os_timer_t request_timeout;
  fota_cdn_t fw_server;
  enum fota_status status;
} fota_client_t;

void start_fota(fota_client_t *client, uint32_t interval, char *host, uint16_t port, char *id, char* token);
void stop_fota(fota_client_t *client);

#endif