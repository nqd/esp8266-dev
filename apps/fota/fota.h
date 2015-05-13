#ifndef _FOTA_VERSION_H_
#define _FOTA_VERSION_H_

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
#define CLIENT_LEN    4
#define VERSION_LEN VERSION_BYTE_STR

typedef struct {
  ip_addr_t server;
  uint16_t port;
  char uuid[UUID_LEN];
  char token[TOKEN_LEN];
  char client[CLIENT_LEN];
  char version[VERSION_LEN];
} fota_info_t;

void start_fota(fota_info_t *);

#endif