#include "osapi.h"
#include "c_types.h"
#include "user_interface.h"
#include "mem.h"
#include "espconn.h"

// json parsing
#include "jsmn.h"

#include "fota.h"
#include "fota-util.h"

#define FAILED    -1
#define SUCCESS   0


LOCAL int8_t ICACHE_FLASH_ATTR
jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int) os_strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

LOCAL int8_t ICACHE_FLASH_ATTR
json_get_value(const char *json, jsmntok_t *tok, const char *key, char **value) {
  if (jsoneq(json, tok, key) == 0 && tok[1].type == JSMN_STRING) {
    uint32_t len = tok[1].end-tok[1].start;
    // clean memory if *value is allocated before
    if (*value != NULL)
      os_free(*value);

    *value = (char*) os_zalloc(len+1);
    os_strncpy(*value, (char*)(json+ tok[1].start), len);
    // INFO("%s: %s\n", key, *value);
    return 0;
  }
  return -1;
}

int8_t  ICACHE_FLASH_ATTR
parse_fota(const char *json, uint32_t len, char **version, char **host, char **url, char **protocol)
{
  int count = 0;
  int i;
  int r;
  jsmn_parser par;
  jsmntok_t tok[128]; /* We expect no more than 128 tokens */

  // prepare pointer
  *version = NULL;
  *host = NULL;
  *url = NULL;
  *protocol = NULL;

  jsmn_init(&par);
  r = jsmn_parse(&par, json, len, tok, sizeof(tok)/sizeof(tok[0]));
  if (r < 0) {
    INFO("Failed to parse JSON: %d\n", r);
    return FAILED;
  }
  // Assume the top-level element is an object 
  if (r < 1 || tok[0].type != JSMN_OBJECT) {
    INFO("Object expected\n");
    return FAILED;
  }
  // Loop over all keys of the root object 
  for (i = 1; i < r; i++) {
    if (jsoneq(json, &tok[i], "last") == 0) {
      // We expect groups to be an object
      if (tok[i+1].type != JSMN_OBJECT)
        continue;

      int j;
      int z=0;
      char value[100];
      for (j = 0; j < tok[i+1].size; j++) {
        if (json_get_value(json, &tok[i+z+2], "version", version) == 0) {
          count += 1;
        }
        else if (json_get_value(json, &tok[i+z+2], "host", host) == 0)
          count += 1;
        else if (json_get_value(json, &tok[i+z+2], "url", url) == 0)
          count += 1;
        else if (json_get_value(json, &tok[i+z+2], "protocol", protocol) == 0)
          count += 1;

        z += 2;     // we expect key: value under last object
      }
    }
  }
  // clean up malloc when parsing failed
  if (count < 4) {
    FREE(*version);
    FREE(*host);
    FREE(*url);
    FREE(*protocol);
    return FAILED;
  }
  else
    return SUCCESS;
}

int8_t ICACHE_FLASH_ATTR
convert_version(const char *version_str, uint32_t len, uint32_t *version_bin)
{
  uint32_t value = 0, digit;
  int8_t c;
  int8_t byte = 0;

  *version_bin = 0;

  uint32_t i;

  for (i=0; i<= len; i++) {             // yes, I want to see /0 character
    c = version_str[i];
    if('0' <= c && c <= '9') {
      digit = c - '0';
      value = value*10 + digit;
      if (value > VERSION_DIGIT_MAX)    // max char
        return FAILED;
    }
    else if(c == '.' || c == 0) {
      *version_bin = *version_bin*256 + value;
      byte++;

      if (byte >= VERSION_BYTE)
        return *version_bin;
      else if (c == '\0')
        return FAILED;

      value = 0;
    }
    else {
      return FAILED;
    }
  }
  return SUCCESS;
}

void ICACHE_FLASH_ATTR
start_esp_connect(struct espconn *conn, uint8_t secure, void *connect_cb, void *disconnect_cb)
{
  espconn_regist_connectcb(conn, connect_cb);
  espconn_regist_disconcb(conn, disconnect_cb);
  // INFO("Dest ip: %d.%d.%d.%d:%d\n",
  //   conn->proto.tcp->remote_ip[0],
  //   conn->proto.tcp->remote_ip[1],
  //   conn->proto.tcp->remote_ip[2],
  //   conn->proto.tcp->remote_ip[3],
  //   conn->proto.tcp->remote_port);

  if (secure) {
    os_printf("Secure connection\n");

    espconn_secure_set_size(ESPCONN_CLIENT, 8192); // set SSL buffer size, if your SSL packet larger than 2048 bytes
    if (espconn_secure_connect(conn) !=0 )
      INFO("Secure connect fail\n");
  }
  else {
    if (espconn_connect(conn) != 0)
      INFO("Connect fail\n");
  }
}

void ICACHE_FLASH_ATTR
clear_espconn(struct espconn *conn) {
  if (conn != NULL) {
    if (conn->proto.tcp != NULL) {
      FREE(conn->proto.tcp);
    }
    FREE(conn);
  }
}

