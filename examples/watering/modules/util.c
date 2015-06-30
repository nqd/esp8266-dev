#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "user_interface.h"
#include "json/jsonparse.h"
#include "jsmn.h"
#include "util.h"

/*
 * Restart system
 */
 
void ICACHE_FLASH_ATTR util_restart(void)
{
    os_printf("\r\nRestarting...\r\n");
    ets_delay_us(250000);
    system_restart();
}


/*
 * Assert error handler
 */
 

void ICACHE_FLASH_ATTR util_assert_handler(void)
{   
    util_restart();
}



/*
 * util_str_realloc - return a new string pointer in a larger buffer
 */
 
char * ICACHE_FLASH_ATTR util_str_realloc(const char *p, size_t new_len)
{
    size_t oldlen = strlen(p) + 1;
    util_assert(new_len > oldlen + 1, "New string smaller than original string. new length: %d, old length: %d", new_len, oldlen); 
    char *n =  util_zalloc(new_len);
    os_strcpy(n, p);
    util_free(p);
    return n;
}



/*
 * strdup function missing from os_* calls.
 */
 
char * ICACHE_FLASH_ATTR util_strdup(const char *s)
{
    char *p =  util_zalloc(os_strlen(s) + 1);
    os_strcpy(p, s);
    return p;
}



/*
 * strndup function missing from os_* calls.
 */
 
char * ICACHE_FLASH_ATTR util_strndup(const char *s, int len)
{
    char *p =  util_zalloc(len + 1);
    os_strncpy(p, s, len);
    p[len] = 0;
    return p;
}
 




/*
* Split a string into substrings and return the result as a list of pointers
* List of pointers should be 1 more than what is required. 
* max_list_length includes the NULL marker.
* 
* Returned char pointer must be freed when no longer required.
* 
* Does not remove white space
*/
char * ICACHE_FLASH_ATTR util_string_split(const char *in_str, char **list, char sep, int max_list_length)
{
    int i, j;
    char *str = util_strdup(in_str);
    
    for(i = 0, j = 0; i < max_list_length - 1; i++){

        // Skip leading seps
        while(sep == str[j]){
            if(!str[j])
                break;
            j++;
        }
        
        // Test for empty entry
        if(!str[j])
            break;
            
        list[i] = str + j; // Save the beginning of the string
        while(str[j] && (str[j] != sep))
            j++;
        // Test for end of string
        if(!str[j]){
            i++;
            break;
        }
        str[j] = 0; // Terminate substring
        j++;
    }
    list[i] = NULL; // Terminate end of list
    return str;
}

/*
 * Allocate and make a subtopic string 
 */

char * ICACHE_FLASH_ATTR util_make_sub_topic(const char *rootTopic, char *subTopic)
{
    char *r = (char *) os_zalloc(strlen(rootTopic) + 
    2 + strlen(subTopic));
    os_strcpy(r, rootTopic);
    os_strcat(r, "/");
    os_strcat(r, subTopic);
    return r;
}


char* ICACHE_FLASH_ATTR json_get_value_as_string(const char *json, jsmntok_t *tok) {
  if (tok->type != JSMN_STRING)
    return NULL;

  uint32_t len = tok->end-tok->start;
  char *s = (char*)os_zalloc(len+1);
  os_strncpy(s, (char*)(json + tok->start), len);
  return s;
}

char* ICACHE_FLASH_ATTR json_get_value_as_primitive(const char *json, jsmntok_t *tok) {
  if (tok->type != JSMN_PRIMITIVE)
    return NULL;
  
  uint32_t len = tok->end-tok->start;
  char *s = (char*)os_zalloc(len+1);
  os_strncpy(s, (char*)(json + tok->start), len);
  return s;
}

int8_t ICACHE_FLASH_ATTR jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int) os_strlen(s) == tok->end - tok->start &&
      strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
    return 0;
  }
  return -1;
}

int8_t ICACHE_FLASH_ATTR util_check_command_param(uint8_t i, uint8_t r, jsmntok_t *t, char *dataBuf) {
  int8_t retVal = -1;
  if ((i < r) && (jsoneq(dataBuf, &t[i], "param") == 0)) {
    if (t[i+1].type == JSMN_PRIMITIVE) {
      char *valve = json_get_value_as_primitive(dataBuf, &t[i+1]);
      uint8_t valveIdx = atoi(valve);
      if ((valveIdx > 0) && (valveIdx <= VALVE_NUMBER)) {
        retVal = valveIdx;
      }
      os_free(valve);
    }
  }
  return retVal;
}

