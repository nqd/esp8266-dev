#ifndef _FOTA_VERSION_H_
#define _FOTA_VERSION_H_

#include "osapi.h"
#include "fota-util.h"

#define pHeadStatic "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: esp\r\n\
Accept: */*\r\n"

#define pHeadAuthen "UUID: %s\r\n\
Token: %s\r\n\
Client: %s\r\n\
Version: %s\r\n"

#define UUID_LEN    15
#define TOKEN_LEN   161
#define VERSION_LEN VERSION_BYTE_STR

#define HOST_LEN    30
#define URL_LEN     100
typedef struct {
  ip_addr_t ip;
  uint16_t port;
  char *uuid;
  char *token;
  char *host;
  uint16_t interval;
  struct espconn *conn;
  os_timer_t periodic;
} fota_client_t;

typedef struct {
  ip_addr_t server;
  uint16_t port;
  char host[HOST_LEN];
  char url[URL_LEN];
} fota_cdn_t

void start_fota(fota_client_t *client, uint16_t interval, char *host, uint16_t port, char *id, char* token);

#endif