#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "mem.h"

#include "jsmn.h"
#include "json_example.h"

#define PRINTF os_printf

LOCAL char* ICACHE_FLASH_ATTR
json_get_value_as_string(const char *json, jsmntok_t *tok) {
  if (tok->type != JSMN_STRING)
    return NULL;

  uint32_t len = tok->end-tok->start;
  char *s = (char*)os_zalloc(len+1);
  os_strncpy(s, (char*)(json + tok->start), len);
  return s;
}

LOCAL char* ICACHE_FLASH_ATTR
json_get_value_as_primitive(const char *json, jsmntok_t *tok) {
  if (tok->type != JSMN_PRIMITIVE)
    return NULL;
  
  uint32_t len = tok->end-tok->start;
  char *s = (char*)os_zalloc(len+1);
  os_strncpy(s, (char*)(json + tok->start), len);
  return s;
}

LOCAL int8_t ICACHE_FLASH_ATTR
jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int) os_strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

const char *JSON_STRING =
  "{\"user\": \"johndoe\", \"admin\": false, \"uid\": 1000,\n  "
  "\"groups\": [\"users\", \"wheel\", \"audio\", \"video\"]}";

void ICACHE_FLASH_ATTR
parsing_example()
{
  int i;
  int r;
  jsmn_parser p;
  jsmntok_t t[128]; /* We expect no more than 128 tokens */

  jsmn_init(&p);
  r = jsmn_parse(&p, JSON_STRING, os_strlen(JSON_STRING), t, sizeof(t)/sizeof(t[0]));
  if (r < 0) {
    os_printf("Failed to parse JSON: %d\n", r);
    return;
  }

  /* Assume the top-level element is an object */
  if (r < 1 || t[0].type != JSMN_OBJECT) {
    os_printf("Object expected\n");
    return;
  }

  /* Loop over all keys of the root object */
  for (i = 1; i < r; i++) {
    if (jsoneq(JSON_STRING, &t[i], "user") == 0) {
      char *s = json_get_value_as_string(JSON_STRING, &t[i+1]);
      if (s != NULL) {
        os_printf("- User: %s\n", s);
        os_free(s);
      }
      i++;
    } else if (jsoneq(JSON_STRING, &t[i], "admin") == 0) {
      char *s = json_get_value_as_primitive(JSON_STRING, &t[i+1]);
      if (s != NULL) {
        os_printf("- Admin: %s\n", s);
        os_free(s);
      }
      i++;
    } else if (jsoneq(JSON_STRING, &t[i], "uid") == 0) {
      char *s = json_get_value_as_primitive(JSON_STRING, &t[i+1]);
      if (s != NULL) {
        os_printf("- UID: %s\n", s);
        os_free(s);
      }
      i++;
    } else if (jsoneq(JSON_STRING, &t[i], "groups") == 0) {
      int j;
      os_printf("- Groups:\n");
      if (t[i+1].type != JSMN_ARRAY) {
        continue; /* We expect groups to be an array of strings */
      }
      for (j = 0; j < t[i+1].size; j++) {
        jsmntok_t *g = &t[i+j+2];

        char *s = json_get_value_as_string(JSON_STRING, g);
        if (s != NULL) {
          os_printf("\t%s\n", s);
          os_free(s);
        }
      }
      i += t[i+1].size + 1;
    } else {
      os_printf("Unexpected key: %.*s\n", t[i].end-t[i].start,
          JSON_STRING + t[i].start);
    }
  }
  PRINTF("Parsing successful\n");
}