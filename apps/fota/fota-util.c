#include "osapi.h"
#include "c_types.h"
#include "user_interface.h"
#include "mem.h"

// json parsing
#include "jsmn.h"

#include "fota.h"
#include "fota-util.h"

#define FAILED    -1
#define SUCCESS   0


LOCAL int ICACHE_FLASH_ATTR
jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int) os_strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

int8_t  ICACHE_FLASH_ATTR
parse_fota(const char *json, uint32_t len, char *version, char *host, char *url, char *protocol)
{
  int count = 0;
  int i;
  int r;
  jsmn_parser par;
  jsmntok_t tok[128]; /* We expect no more than 128 tokens */

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
      if (tok[i+1].type != JSMN_OBJECT) {
        continue;
      }
      int j;
      for (j = 1; j < tok[i+1].size; j++) {
        if (jsoneq(json, &tok[i+j+1], "version") == 0) {
          uint32_t len = tok[i+j+2].end-tok[i+j+2].start;
          version = (char*) os_zalloc(len+1);
          os_strncpy(version, (char*)(json+ tok[i+j+2].start), len);
          count += 1;
        }
        if (jsoneq(json, &tok[i+j+1], "host") == 0) {
          uint32_t len = tok[i+j+2].end-tok[i+j+2].start;
          host = (char*) os_zalloc(len+1);
          os_strncpy(host, (char*)(json+ tok[i+j+2].start), len);          
          count += 1;
        }
        if (jsoneq(json, &tok[i+j+1], "url") == 0) {
          uint32_t len = tok[i+j+2].end-tok[i+j+2].start;
          url = (char*) os_zalloc(len+1);
          os_strncpy(url, (char*)(json+ tok[i+j+2].start), len);          
          count += 1;
        }
        if (jsoneq(json, &tok[i+j+1], "protocol") == 0) {
          uint32_t len = tok[i+j+2].end-tok[i+j+2].start;
          protocol = (char*) os_zalloc(len+1);
          os_strncpy(protocol, (char*)(json+ tok[i+j+2].start), len);          
          count += 1;
        }
        j++;
      }
      i += tok[i+1].size + 1;
    }
  }

  return (count == 4)?SUCCESS:FAILED;
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