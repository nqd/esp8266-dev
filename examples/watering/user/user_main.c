/* main.c -- MQTT client example
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#include "ets_sys.h"
#include "driver/uart.h"
#include "osapi.h"
#include "wifi.h"
#include "easygpio.h"
#include "jsmn.h"
#include "util.h"
#include "debug.h"
#include "gpio.h"
#include "time.h"
#include "user_interface.h"
#include "kvstore.h"
#include "wifi_config.h"
#include "mem.h"
#include "c_types.h"
#include "espconn.h"
#include "upgrade.h"
#include "mqtt.h"
#include "fota.h"
#include "schedule_vector.h"
#include "driver/pcf8563.h"



MQTT_Client mqttClient;


// Definition for valves schedule element
struct schedule_tag {
  int isInit[4];
  ScheduleVector *v1;
  ScheduleVector *v2;
  ScheduleVector *v3;
  ScheduleVector *v4;
};

typedef struct schedule_tag schedule;

  
LOCAL int buttonState[VALVE_NUMBER] = {1, 1, 1, 1};
LOCAL int valveState[VALVE_NUMBER] = {OFF, OFF, OFF, OFF};
LOCAL char *commandTopic, *statusTopic, *v1ScheduleTopic, *v2ScheduleTopic, *v3ScheduleTopic, *v4ScheduleTopic;
LOCAL char *controlTopic = "/node/control";
LOCAL char *infoTopic = "/node/info";
LOCAL flash_handle_s *configHandle;
LOCAL os_timer_t pulseTimer1, pulseTimer2, pulseTimer3, pulseTimer4, buttonTimer, clockTimer;
LOCAL fota_client_t fota_client;
LOCAL schedule valves_schedule;
LOCAL struct tm timeStruct;

unsigned char *default_certificate;
unsigned int default_certificate_len = 0;
unsigned char *default_private_key;
unsigned int default_private_key_len = 0;


/**
 * Init schedule
 */
LOCAL void ICACHE_FLASH_ATTR initSchedule() {
  ScheduleVector* ptr1 = util_zalloc(sizeof(ScheduleVector));
  ScheduleVector* ptr2 = util_zalloc(sizeof(ScheduleVector));
  ScheduleVector* ptr3 = util_zalloc(sizeof(ScheduleVector));
  ScheduleVector* ptr4 = util_zalloc(sizeof(ScheduleVector));
  valves_schedule.v1 = ptr1;
  valves_schedule.v2 = ptr2;
  valves_schedule.v3 = ptr3;
  valves_schedule.v4 = ptr4;
  valves_schedule.isInit[0] = 0;
  valves_schedule.isInit[1] = 0;
  valves_schedule.isInit[2] = 0;
  valves_schedule.isInit[3] = 0;
}

/**
 * Free schedule
 */
LOCAL void ICACHE_FLASH_ATTR freeSchedule() {
  schedule_vector_free(valves_schedule.v1);
  schedule_vector_free(valves_schedule.v2);
  schedule_vector_free(valves_schedule.v3);
  schedule_vector_free(valves_schedule.v4);
  os_free(valves_schedule.v1);
  os_free(valves_schedule.v2);
  os_free(valves_schedule.v3);
  os_free(valves_schedule.v4);
  valves_schedule.v1 = NULL;
  valves_schedule.v2 = NULL;
  valves_schedule.v3 = NULL;
  valves_schedule.v4 = NULL;
}



/**
 * Publish connection info
 */
LOCAL void ICACHE_FLASH_ATTR publishConnInfo(MQTT_Client *client)
{
  struct ip_info ipConfig;
  char *buf = util_zalloc(256); 
  uint32_t freeRam = system_get_free_heap_size();
    
  // Publish who we are and where we live
  wifi_get_ip_info(STATION_IF, &ipConfig);
  os_sprintf(buf, "{\"muster\":{\"connstate\":\"online\",\"device\":\"home/valves\",\"ip4\":\"%d.%d.%d.%d\",\"schema\":\"hwstar_relaynode\",\"ssid\":\"%s\",\"heap\":\"%d\",\"time\":\"%d:%d:%d-%d?%d/%d/%d\"}}",
      *((uint8_t *) &ipConfig.ip.addr),
      *((uint8_t *) &ipConfig.ip.addr + 1),
      *((uint8_t *) &ipConfig.ip.addr + 2),
      *((uint8_t *) &ipConfig.ip.addr + 3),
      STA_SSID,
      freeRam,
      timeStruct.tm_sec,
      timeStruct.tm_min,
      timeStruct.tm_hour,
      timeStruct.tm_wday,
      timeStruct.tm_mday,
      timeStruct.tm_mon,
      timeStruct.tm_year);

  INFO("MQTT Node info: %s\r\n", buf);

  // Publish
  MQTT_Publish(client, infoTopic, buf, os_strlen(buf), 0, 0);
  
  // Free the buffer
  util_free(buf);
  
}


/**
 * Return valve schedule matching valve number
 */
LOCAL ScheduleVector* ICACHE_FLASH_ATTR getValveScheduleFromValveNumber(int valve){
  if (valve == 1) 
    return valves_schedule.v1;
  else if (valve == 2) 
    return valves_schedule.v2;
  else if (valve == 3) 
    return valves_schedule.v3;
  else if (valve == 4) 
    return valves_schedule.v4;
}

/**
 * Return valve topic matching valve number
 */
LOCAL char* ICACHE_FLASH_ATTR getValveTopicFromValveNumber(int valve){
  if (valve == 1) 
    return v1ScheduleTopic;
  else if (valve == 2) 
    return v2ScheduleTopic;
  else if (valve == 3) 
    return v3ScheduleTopic;
  else if (valve == 4) 
    return v4ScheduleTopic;
}

/**
 * Return valve topic matching valve number
 */
LOCAL int ICACHE_FLASH_ATTR getValveNumberFromTopic(char* topic){
  if (!os_strcmp(topic, v1ScheduleTopic)) 
    return 1;
  else if (!os_strcmp(topic, v2ScheduleTopic)) 
    return 2;
  else if (!os_strcmp(topic, v3ScheduleTopic)) 
    return 3;
  else if (!os_strcmp(topic, v4ScheduleTopic)) 
    return 4;
}

/**
 * Valve schedule,
 * publish schedule
 */

LOCAL void ICACHE_FLASH_ATTR publishValveSchedule(MQTT_Client *client, int valveNumber) {
  #define SCHEDULE_ENTRY_LENGTH 50

  ScheduleVector *schedule_vector = getValveScheduleFromValveNumber(valveNumber); 
  char *topic = getValveTopicFromValveNumber(valveNumber);
  char *buf = util_zalloc(SCHEDULE_ENTRY_LENGTH * schedule_vector->size + 2);
  uint8_t i;
  os_sprintf(buf,"[");

  for (i = 0; i < schedule_vector->size; i++) {
    valve_schedule slot = schedule_vector_get(schedule_vector, i);
    os_sprintf(strlen(buf)+ buf, "{\"id\":\"%d\",\"duration\":\"%d\",\"d\":\"%d\",\"h\":\"%d\",\"m\":\"%d\"}", i, slot.duration, slot.when.d, slot.when.h, slot.when.m);
    if (i < schedule_vector->size - 1) {
      os_sprintf(strlen(buf)+ buf, ",");
    }
  }
  os_sprintf(strlen(buf)+ buf, "]");
  
  // Publish
  MQTT_Publish(client, topic, buf, os_strlen(buf), 0, 1);
  
  // Free buffer
  util_free(buf);
}


/**
 * Current time,
 * publish time
 */

LOCAL void ICACHE_FLASH_ATTR publishTime(MQTT_Client *client) {
  char *buf = util_zalloc(256); 
    
  // Publish time
  os_sprintf(buf, "{\"time\":\"%d:%d:%d-%d?%d/%d/%d\"}",
      timeStruct.tm_sec,
      timeStruct.tm_min,
      timeStruct.tm_hour,
      timeStruct.tm_wday,
      timeStruct.tm_mday,
      timeStruct.tm_mon,
      timeStruct.tm_year);

  INFO("MQTT: Time: %s\r\n", buf);
  // Publish
  MQTT_Publish(client, statusTopic, buf, os_strlen(buf), 0, 0);
  
  // Free the buffer
  util_free(buf);
}



/**
 * Handle qstring command
 */

LOCAL void ICACHE_FLASH_ATTR handleQstringCommand(char *new_value, char *command)
{
  // char *buf = util_zalloc(128);
  
  // if(!new_value){
  //   const char *cur_value = kvstore_get_string(configHandle, command);
  //   os_sprintf(buf, "{\"%s\":\"%s\"}", command, cur_value);
  //   util_free(cur_value);
  //   INFO("Query Result: %s\r\n", buf );
  //   MQTT_Publish(&mqttClient, statusTopic, buf, os_strlen(buf), 0, 0);
  // }
  // else{
  //   kvstore_put(configHandle, command, new_value);
  // }

  // util_free(buf);

}

/**
 * Update the button state
 */

LOCAL void ICACHE_FLASH_ATTR publishButtonState(uint8_t button)
{
  char result[25];
  char snum[5];
  os_sprintf(snum,"%d",button);  
  os_strcpy(result,"{\"buttonpressed\":");
  os_strcat(result, snum);
  os_strcat(result, "}");
  INFO("MQTT: New Button Pressed: %d\r\n", button);
  MQTT_Publish(&mqttClient, statusTopic, result, os_strlen(result), 0, 1);
  
}

/**
 * Send MQTT message to update valve state
 * {"valve":"uint8_t","state":"on/off"}
 */
 
LOCAL void ICACHE_FLASH_ATTR updateValveState(uint8_t s, uint8_t v)
{
  char *state = s ? "\"on\"}" : "\"off\"}";
  char result[28];
  char snum[5];
  os_sprintf(snum,"%d",v);
  os_strcpy(result,"{\"valve\":\"");
  os_strcat(result, snum);
  os_strcat(result, ",\"state\":");
  os_strcat(result, state);
  INFO("MQTT: New State: %s for valve %d\r\n", state, v);
  MQTT_Publish(&mqttClient, statusTopic, result, os_strlen(result), 0, 1);

}


/**
 * Return valve schedule matching topic
 */
LOCAL ScheduleVector* ICACHE_FLASH_ATTR getValveScheduleFromTopic(char* topic){
  if (!os_strcmp(topic, v1ScheduleTopic)) 
    return valves_schedule.v1;
  else if (!os_strcmp(topic, v2ScheduleTopic)) 
    return valves_schedule.v2;
  else if (!os_strcmp(topic, v3ScheduleTopic)) 
    return valves_schedule.v3;
  else if (!os_strcmp(topic, v4ScheduleTopic)) 
    return valves_schedule.v4;
}

/**
 * Return corresponding valve GPIO pin
 */
LOCAL uint8_t ICACHE_FLASH_ATTR getValveGpio(uint8_t valve){
  if (valve == 1) 
    return VALVE1_GPIO;
  else if (valve == 2) 
    return VALVE2_GPIO;
  else if (valve == 3) 
    return VALVE3_GPIO;
  else if (valve == 4) 
    return VALVE4_GPIO;
}

/**
 * Return corresponding button GPIO pin
 */
LOCAL uint8_t ICACHE_FLASH_ATTR getButtonGpio(uint8_t valve){
  if (valve == 1) 
    return BUTTON1_GPIO;
  else if (valve == 2) 
    return BUTTON2_GPIO;
  else if (valve == 3) 
    return BUTTON3_GPIO;
  else if (valve == 4) 
    return BUTTON4_GPIO;
}

/**
 * Return valve corresponding timer
 */
LOCAL os_timer_t* ICACHE_FLASH_ATTR getPulseTimer(uint8_t valve) {
  if (valve == 1) 
    return &pulseTimer1;
  else if (valve == 2) 
    return &pulseTimer2;
  else if (valve == 3) 
    return &pulseTimer3;
  else if (valve == 4) 
    return &pulseTimer4;
}

/**
 * Set new valve state
 */
LOCAL void ICACHE_FLASH_ATTR valveSet(bool new_state, uint8_t valve)
{
  if ((valve <= 0) || (valve > 4)) {
    return;
  }
  valveState[valve-1] = new_state; 
  easygpio_outputSet(getValveGpio(valve), ((new_state) ? ON : OFF));
  updateValveState(valveState[valve-1], valve);
}

/**
 * Toggle the valve output
 */
LOCAL void ICACHE_FLASH_ATTR valveToggle(uint8_t valve)
{
  bool new_relayState = (valveState[valve-1]) ? OFF : ON;
  valveSet(new_relayState, valve);
}


/**
 * WIFI connect call back
 */

LOCAL void ICACHE_FLASH_ATTR wifiConnectCb(uint8_t status) {
  if(status == STATION_GOT_IP){
    MQTT_Connect(&mqttClient);
    start_fota(&fota_client, INTERVAL, UPDATE_SERVER_IP, UPDATE_SERVER_PORT, OTA_UUID, OTA_TOKEN);
  } else {
    MQTT_Disconnect(&mqttClient);
  }
}




/**
 * Survey complete,
 * publish results
 */

LOCAL void ICACHE_FLASH_ATTR surveyCompleteCb(void *arg, STATUS status) {
  struct bss_info *bss = arg;
  
  #define SURVEY_CHUNK_SIZE 256
  
  if(status == OK){
    uint8_t i;
    char *buf = util_zalloc(SURVEY_CHUNK_SIZE);
    bss = bss->next.stqe_next; //ignore first
    for(i = 2; (bss); i++){
      if(2 == i)
        os_sprintf(strlen(buf) + buf,"{\"access_points\":[");
      else
        os_strcat(buf,",");
      os_sprintf(strlen(buf)+ buf, "\"%s\":{\"chan\":\"%d\",\"rssi\":\"%d\"}", bss->ssid, bss->channel, bss->rssi);
      bss = bss->next.stqe_next;
      buf = util_str_realloc(buf, i * SURVEY_CHUNK_SIZE); // Grow buffer
    }
    if(buf[0])
      os_strcat(buf,"]}");
    
    INFO("Survey Results:\r\n", buf);
    INFO(buf);
    MQTT_Publish(&mqttClient, statusTopic, buf, os_strlen(buf), 0, 0);
    util_free(buf);
  }

}




/**
 * MQTT Connect call back
 */
 
LOCAL void ICACHE_FLASH_ATTR mqttConnectedCb(uint32_t *args) { 
  MQTT_Client* client = (MQTT_Client*)args;

  INFO("MQTT: Connected\r\n");

  publishConnInfo(client);
  
  // Subscribe to the control topic
  MQTT_Subscribe(client, controlTopic, 0);
  // Subscribe to command topic
  MQTT_Subscribe(client, commandTopic, 0);
  // Subscribe to valves schedule topic
  MQTT_Subscribe(client, v1ScheduleTopic, 0);
  MQTT_Subscribe(client, v2ScheduleTopic, 0);
  MQTT_Subscribe(client, v3ScheduleTopic, 0);
  MQTT_Subscribe(client, v4ScheduleTopic, 0);

  // Publish valves state
  // uint8_t i;
  // for (i = 0; i < VALVE_NUMBER; ++i) {
  //   updateValveState(valveState[i], i);
  //   os_delay_us(1000000);
  // }
}


/**
 * MQTT Disconnect call back
 */
 
LOCAL void ICACHE_FLASH_ATTR mqttDisconnectedCb(uint32_t *args)
{
  MQTT_Client* client = (MQTT_Client*)args;
  INFO("MQTT: Disconnected\r\n");
}

/**
 * MQTT published call back
 */

LOCAL void ICACHE_FLASH_ATTR mqttPublishedCb(uint32_t *args)
{
  MQTT_Client* client = (MQTT_Client*)args;
  INFO("MQTT: Published\r\n");
}


/**
 * MQTT Data call back
 * Commands are decoded and acted upon here
 */

LOCAL void ICACHE_FLASH_ATTR mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len) {
  char *topicBuf, *dataBuf;
  uint8_t i;
  uint8_t r;
  jsmn_parser p;
  jsmntok_t t[128];

  MQTT_Client* client = (MQTT_Client*)args; // Pointer to MQTT control block passed in as args
  
  // Save local copies of the topic and data
  topicBuf = util_strndup(topic, topic_len);
  dataBuf = util_strndup(data, data_len);
  
  INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);
  
  
  if ((!os_strcmp(topicBuf, v1ScheduleTopic)) || (!os_strcmp(topicBuf, v2ScheduleTopic)) || (!os_strcmp(topicBuf, v3ScheduleTopic)) || (!os_strcmp(topicBuf, v4ScheduleTopic))) {   
    ScheduleVector *schedule_vector = getValveScheduleFromTopic(topicBuf);
    int valve_number = getValveNumberFromTopic(topicBuf);
    jsmn_init(&p);
    r = jsmn_parse(&p, dataBuf, os_strlen(dataBuf), t, sizeof(t)/sizeof(t[0]));
    if (r < 0) {
      os_printf("Failed to parse JSON: %d\n", r);
      util_free(topicBuf);
      util_free(dataBuf);
      return;
    }

    /* Assume the top-level element is an object */
    if (r < 1 || t[0].type != JSMN_ARRAY) {
      os_printf("Array expected\n");
      util_free(topicBuf);
      util_free(dataBuf);
      return;
    }

    // Check if schedule has already been init 
    if (valves_schedule.isInit[valve_number-1]){
      os_printf("Schedule already init\n");
      util_free(topicBuf);
      util_free(dataBuf);
      return;
    }
    valves_schedule.isInit[valve_number-1] = 1;

    if (t[0].size <= 0) {
      os_printf("Empty array\n");
      util_free(topicBuf);
      util_free(dataBuf);
      return;
    } else {
      schedule_vector_init(schedule_vector, t[0].size);
    }

    for (i = 1; i < r; i++) {
      if ((t[i].type != JSMN_OBJECT) || (t[i].size < 5)) {
        os_printf("Wrong json syntax, expecting an object with 5 attributes\n");
        continue;
      } else {
        uint8_t j;
        int8_t id = 0;
        int8_t day = -1;
        int8_t hour = -1;
        int8_t min = -1;
        int32_t duration = -1;
        for (j = 0; j < t[i].size * 2; j++) {
          if (jsoneq(dataBuf, &t[i+1+j], "id") == 0) {
            char *idC = json_get_value_as_primitive(dataBuf, &t[i+1+j+1]);
            if (idC != NULL) {
              id = atoi(idC);
              os_free(idC);
            }
            j++;
          } else if (jsoneq(dataBuf, &t[i+1+j], "duration") == 0) {
            char *durC = json_get_value_as_primitive(dataBuf, &t[i+1+j+1]);
            if (durC != NULL) {
              duration = atoi(durC);
              os_free(durC);
            }
            j++;
          } else if (jsoneq(dataBuf, &t[i+1+j], "d") == 0) {
            char *dayC = json_get_value_as_primitive(dataBuf, &t[i+1+j+1]);
            if (dayC != NULL) {
              day = atoi(dayC);
              os_free(dayC);
            }
            j++;
          } else if (jsoneq(dataBuf, &t[i+1+j], "h") == 0) {
            char *hourC = json_get_value_as_primitive(dataBuf, &t[i+1+j+1]);
            if (hourC != NULL) {
              hour = atoi(hourC);
              os_free(hourC);
            }
            j++;
          } else if (jsoneq(dataBuf, &t[i+1+j], "m") == 0) {
            char *minC = json_get_value_as_primitive(dataBuf, &t[i+1+j+1]);
            if (minC != NULL) {
              min = atoi(minC);
              os_free(minC);
            }
            j++;
          }
        }
        if ((id >= 0) && (day >= 0) && (hour >= 0) && (min >= 0) && (duration >= 0)) {
          schedule_vector_set(schedule_vector, id, duration, day, hour, min);
        }
        i += t[i].size * 2;
      }
    }

  }
  // Control|Command Message?
  else if ((!os_strcmp(topicBuf, controlTopic)) || (!os_strcmp(topicBuf, commandTopic))){
    
    jsmn_init(&p);
    r = jsmn_parse(&p, dataBuf, os_strlen(dataBuf), t, sizeof(t)/sizeof(t[0]));
    if (r < 0) {
      os_printf("Failed to parse JSON: %d\n", r);
      util_free(topicBuf);
      util_free(dataBuf);
      return;
    }

    /* Assume the top-level element is an object */
    if (r < 1 || t[0].type != JSMN_OBJECT) {
      os_printf("Object expected\n");
      util_free(topicBuf);
      util_free(dataBuf);
      return;
    }

    i = 1;
    if (jsoneq(dataBuf, &t[i], "control") == 0) {
      char *s = json_get_value_as_string(dataBuf, &t[i+1]);
      if (s != NULL) {
        if (jsoneq(dataBuf, &t[i+1], "muster") == 0) {
          publishConnInfo(&mqttClient);
        } else if (jsoneq(dataBuf, &t[i+1], "time") == 0) {
          publishTime(&mqttClient);
        } else {
          os_printf("Unsupported command: %s\n", s);
        }
        os_free(s);
      }
    } else if (jsoneq(dataBuf, &t[i], "command") == 0) {
      if (jsoneq(dataBuf, &t[i+1], "on") == 0) {
        i += 2;
        int8_t valveIdx;
        if ((valveIdx = util_check_command_param(i, r, t, dataBuf)) > 0) {
          valveSet(ON, valveIdx);
        }
      } else if (jsoneq(dataBuf, &t[i+1], "off") == 0) {
        i += 2;
        int8_t valveIdx;
        if ((valveIdx = util_check_command_param(i, r, t, dataBuf)) > 0) {
          valveSet(OFF, valveIdx);
        }
      } else if (jsoneq(dataBuf, &t[i+1], "pulse") == 0) {
        i += 2;
        if ((i < r) && (jsoneq(dataBuf, &t[i], "param") == 0)) {
          uint8_t j, k; 
          k = i+j+2;
          if ((t[i+1].type != JSMN_OBJECT) && (t[i+1].size < 4)) {
            os_printf("Missing param\n");
          } else if ((jsoneq(dataBuf, &t[k], "valve") == 0) && (jsoneq(dataBuf, &t[k+2], "time") == 0)) {
            char *valve = json_get_value_as_primitive(dataBuf, &t[k+1]);
            char *duration = json_get_value_as_primitive(dataBuf, &t[k+3]);
            uint8_t valveIdx = atoi(valve);
            uint32_t millis = atoi(duration);
            if ((valveIdx > 0) && (valveIdx <= VALVE_NUMBER) && (millis > 500)) {
              valveSet(ON, valveIdx);
              os_timer_arm(getPulseTimer(valveIdx), millis, 0);
            } else {
              os_printf("Incorect param\n");
            }
            os_free(valve);
            os_free(duration);
          } else {
            os_printf("Missing param\n");
          }
        }
      } else if (jsoneq(dataBuf, &t[i+1], "addSlot") == 0) {
        i += 2;
        if ((i < r) && (jsoneq(dataBuf, &t[i], "param") == 0)) {
          uint8_t j, k; 
          k = i+j+2;
          if ((t[i+1].type != JSMN_OBJECT) && (t[i+1].size < 5)) {
            os_printf("Missing param\n");
          } else if ((jsoneq(dataBuf, &t[k], "valve") == 0) && (jsoneq(dataBuf, &t[k+2], "duration") == 0) && (jsoneq(dataBuf, &t[k+4], "d") == 0) && (jsoneq(dataBuf, &t[k+6], "h") == 0) && (jsoneq(dataBuf, &t[k+8], "m") == 0)) {
            char *valve = json_get_value_as_primitive(dataBuf, &t[k+1]);
            char *durationC = json_get_value_as_primitive(dataBuf, &t[k+3]);
            char *dayC = json_get_value_as_primitive(dataBuf, &t[k+5]);
            char *hourC = json_get_value_as_primitive(dataBuf, &t[k+7]);
            char *minC = json_get_value_as_primitive(dataBuf, &t[k+9]);
            uint32_t duration = atoi(durationC);
            uint8_t valveIdx = atoi(valve);
            uint8_t day = atoi(dayC);
            uint8_t hour = atoi(hourC);
            uint8_t min = atoi(minC);
            if ((valveIdx > 0) && (valveIdx <= VALVE_NUMBER) && (duration > 10) && (day <= 7) && (hour <= 23) && (min <= 59)) {
              ScheduleVector *schedule_vector = getValveScheduleFromValveNumber(valveIdx);
              // Check if schedule has been init 
              if (!valves_schedule.isInit[valveIdx-1]){
                schedule_vector_init(schedule_vector, 4);
                valves_schedule.isInit[valveIdx-1] = 1;
              }

              // Add entry to schedule
              schedule_vector_append(schedule_vector, duration, day, hour, min);
              // Publish updated schedule
              publishValveSchedule(client, valveIdx);
            } else {
              os_printf("Incorect param\n");
            }
            os_free(durationC);
            os_free(valve);
            os_free(dayC);
            os_free(hourC);
            os_free(minC);
          } else {
            os_printf("Missing param\n");
          }
        }
      } else if (jsoneq(dataBuf, &t[i+1], "editSlot") == 0) {
        i += 2;
        if ((i < r) && (jsoneq(dataBuf, &t[i], "param") == 0)) {
          uint8_t j, k; 
          k = i+j+2;
          if ((t[i+1].type != JSMN_OBJECT) && (t[i+1].size < 6)) {
            os_printf("Missing param\n");
          } else if ((jsoneq(dataBuf, &t[k], "valve") == 0) && (jsoneq(dataBuf, &t[k+2], "id") == 0) && (jsoneq(dataBuf, &t[k+4], "duration") == 0) && (jsoneq(dataBuf, &t[k+6], "d") == 0) && (jsoneq(dataBuf, &t[k+8], "h") == 0) && (jsoneq(dataBuf, &t[k+10], "m") == 0)) {
            char *valve = json_get_value_as_primitive(dataBuf, &t[k+1]);
            char *idC = json_get_value_as_primitive(dataBuf, &t[k+3]);
            char *durationC = json_get_value_as_primitive(dataBuf, &t[k+5]);
            char *dayC = json_get_value_as_primitive(dataBuf, &t[k+7]);
            char *hourC = json_get_value_as_primitive(dataBuf, &t[k+9]);
            char *minC = json_get_value_as_primitive(dataBuf, &t[k+11]);
            uint32_t duration = atoi(durationC);
            uint8_t id = atoi(idC);
            uint8_t valveIdx = atoi(valve);
            uint8_t day = atoi(dayC);
            uint8_t hour = atoi(hourC);
            uint8_t min = atoi(minC);
            if ((valveIdx > 0) && (valveIdx <= VALVE_NUMBER) && (duration > 10) && (day <= 7) && (hour <= 23) && (min <= 59)) {
              ScheduleVector *schedule_vector = getValveScheduleFromValveNumber(valveIdx);
              // Check if schedule has been init 
              if (valves_schedule.isInit[valveIdx-1]){
                // Add entry to schedule
                schedule_vector_set(schedule_vector, id, duration, day, hour, min);
                // Publish updated schedule
                publishValveSchedule(client, valveIdx);
              }
            } else {
              os_printf("Incorect param\n");
            }
            os_free(durationC);
            os_free(idC);
            os_free(valve);
            os_free(dayC);
            os_free(hourC);
            os_free(minC);
          } else {
            os_printf("Missing param\n");
          }
        }
      } else if (jsoneq(dataBuf, &t[i+1], "rmSlot") == 0) {
        i += 2;
        if ((i < r) && (jsoneq(dataBuf, &t[i], "param") == 0)) {
          uint8_t j, k; 
          k = i+j+2;
          if ((t[i+1].type != JSMN_OBJECT) && (t[i+1].size < 6)) {
            os_printf("Missing param\n");
          } else if ((jsoneq(dataBuf, &t[k], "valve") == 0) && (jsoneq(dataBuf, &t[k+2], "id") == 0)) {
            char *valve = json_get_value_as_primitive(dataBuf, &t[k+1]);
            char *idC = json_get_value_as_primitive(dataBuf, &t[k+3]);
            uint8_t valveIdx = atoi(valve);
            uint8_t id = atoi(idC);
            if ((valveIdx > 0) && (valveIdx <= VALVE_NUMBER)) {
              ScheduleVector *schedule_vector = getValveScheduleFromValveNumber(valveIdx);
              if (valves_schedule.isInit[valveIdx-1]){
                // Add entry to schedule
                schedule_vector_remove(schedule_vector, id);
                // Publish updated schedule
                publishValveSchedule(client, valveIdx);
              }
            } else {
              os_printf("Incorect param\n");
            }
            os_free(valve);
            os_free(idC);
          } else {
            os_printf("Missing param\n");
          }
        }
      } else if (jsoneq(dataBuf, &t[i+1], "time") == 0) {
        i += 2;
        if ((i < r) && (jsoneq(dataBuf, &t[i], "param") == 0)) {
          uint8_t j, k; 
          k = i+j+2;
          if ((t[i+1].type != JSMN_OBJECT) && (t[i+1].size < 6)) {
            os_printf("Missing param\n");
          } else if ((jsoneq(dataBuf, &t[k], "sec") == 0) && (jsoneq(dataBuf, &t[k+2], "min") == 0) && (jsoneq(dataBuf, &t[k+4], "hour") == 0) && (jsoneq(dataBuf, &t[k+6], "dayW") == 0) && (jsoneq(dataBuf, &t[k+8], "dayM") == 0) && (jsoneq(dataBuf, &t[k+10], "month") == 0) && (jsoneq(dataBuf, &t[k+12], "year") == 0)) {
            char *secC = json_get_value_as_primitive(dataBuf, &t[k+1]);
            char *minC = json_get_value_as_primitive(dataBuf, &t[k+3]);
            char *hourC = json_get_value_as_primitive(dataBuf, &t[k+5]);
            char *dayWC = json_get_value_as_primitive(dataBuf, &t[k+7]);
            char *dayMC = json_get_value_as_primitive(dataBuf, &t[k+9]);
            char *monthC = json_get_value_as_primitive(dataBuf, &t[k+11]);
            char *yearC = json_get_value_as_primitive(dataBuf, &t[k+13]);
            uint16_t year = atoi(yearC);
            uint8_t month = atoi(monthC);
            uint8_t dayM = atoi(dayMC);
            uint8_t dayW = atoi(dayWC);
            uint8_t hour = atoi(hourC);
            uint8_t min = atoi(minC);
            uint8_t sec = atoi(secC);
            if ((month <= 12) && (dayM <= 31) && (dayW <= 7) && (hour <= 24) && (min <= 60) && (sec <= 60)) {
              timeStruct.tm_sec    = sec;
              timeStruct.tm_min    = min;
              timeStruct.tm_hour   = hour;
              timeStruct.tm_mday   = dayM;
              timeStruct.tm_wday   = dayW;
              timeStruct.tm_mon    = month;
              timeStruct.tm_year   = year;
              pcf8563_setTime(&timeStruct);
              // Publish updated time
              publishTime(client);
            } else {
              os_printf("Incorect param\n");
            }
            os_free(secC);
            os_free(minC);
            os_free(hourC);
            os_free(dayWC);
            os_free(dayMC);
            os_free(monthC);
            os_free(yearC);
          } else {
            os_printf("Missing param\n");
          }
        }
      } else if (jsoneq(dataBuf, &t[i+1], "toggle") == 0) {
        i += 2;
        int8_t valveIdx;
        if ((valveIdx = util_check_command_param(i, r, t, dataBuf)) > 0) {
          valveToggle(valveIdx);
        }
      } else if (jsoneq(dataBuf, &t[i+1], "query") == 0) {
        i += 2;
        int8_t valveIdx;
        if ((valveIdx = util_check_command_param(i, r, t, dataBuf)) > 0) {
          INFO("Valeur param dans query :%d\r\n", valveIdx);
          updateValveState(valveState[valveIdx-1], valveIdx);
        }          
      } else if (jsoneq(dataBuf, &t[i+1], "survey") == 0) {
        wifi_station_scan(NULL, surveyCompleteCb);
      } else if (jsoneq(dataBuf, &t[i+1], "ssid") == 0) {
        i++;
        if ((i < r) && (jsoneq(dataBuf, &t[i], "param") == 0)) {
          if (t[i+1].type == JSMN_STRING) {
            char *val = json_get_value_as_string(dataBuf, &t[i+1]);
            if (val != NULL) {
              handleQstringCommand(val, "ssid");
              os_free(val);
            }
          } else {
            os_printf("Missing param\n");
          }
        }
      } else if (jsoneq(dataBuf, &t[i+1], "restart") == 0) {
        util_restart();
      } else if (jsoneq(dataBuf, &t[i+1], "wifipass") == 0) {
        i++;
        if ((i < r) && (jsoneq(dataBuf, &t[i], "param") == 0)) {
          if (t[i+1].type == JSMN_STRING) {
            char *val = json_get_value_as_string(dataBuf, &t[i+1]);
            if (val != NULL) {
              handleQstringCommand(val, "wifipass");
              os_free(val);
            }
          } else {
            os_printf("Missing param\n");
          }
        }
      } else if (jsoneq(dataBuf, &t[i+1], "mqttdevpath") == 0) {
        i++;
        if ((i < r) && (jsoneq(dataBuf, &t[i], "param") == 0)) {
          if (t[i+1].type == JSMN_STRING) {
            char *val = json_get_value_as_string(dataBuf, &t[i+1]);
            if (val != NULL) {
              handleQstringCommand(val, "mqttdevpath");
              os_free(val);
            }
          } else {
            os_printf("Missing param\n");
          }
        }
      } else {
        char *s = json_get_value_as_string(dataBuf, &t[i+1]);
        if (s != NULL) {
          os_printf("Unsupported command: %s\n", s);
          os_free(s);
        }
      }
    }
  }

  // Free local copies of the topic and data strings
  util_free(topicBuf);
  util_free(dataBuf);
}

/**
 * Pulse timer callback function. This is used to time
 * the length of the relay on state after a pulse:n command
 * is received.
 */
 
LOCAL void ICACHE_FLASH_ATTR pulseTmerExpireCb(uint8_t arg) {
  uint8_t valve = arg;
  valveSet(OFF, valve);  
}


/**
 * 100 millisecond timer callback
 * This function handles button sampling, 
 * latching relay control, and LED control
 */
LOCAL void ICACHE_FLASH_ATTR buttonTimerCb(void *arg) {
  uint8_t i, newstate;
  for (i = 0; i < VALVE_NUMBER; ++i) {
    newstate =  easygpio_inputGet(getButtonGpio(i+1));
  
    if(newstate != buttonState[i]){
      if(newstate){
        valveToggle(i+1);
        publishButtonState(i+1);
        INFO("Button released\r\n");
      }
      else{
        INFO("Button pressed\r\n");
      } 
      buttonState[i] = newstate;
    }
  }
}


LOCAL void ICACHE_FLASH_ATTR minuteCb() {
  uint8_t i, j;
  for (i = 1; i <= VALVE_NUMBER; i++) {
    ScheduleVector *schedule_vector = getValveScheduleFromValveNumber(i);
    for (j = 0; j < schedule_vector->size; j++) {
      valve_schedule slot = schedule_vector_get(schedule_vector, j);
      if ((timeStruct.tm_wday == slot.when.d) && 
          (timeStruct.tm_hour == slot.when.h) && 
          (timeStruct.tm_min == slot.when.m)) {
        valveSet(ON, i);
        os_timer_arm(getPulseTimer(i), slot.duration * 1000, 0);
        break;
      }
    }
  }
}

/**
 * 250 millisecond timer callback
 * This function handles clock sampling, 
 */
LOCAL void ICACHE_FLASH_ATTR clockTimerCb(void *arg) {
  struct tm latestTime;
  if (pcf8563_getTime(&latestTime)) {
    if (latestTime.tm_min != timeStruct.tm_min) {
      minuteCb();
    }
    timeStruct.tm_sec    = latestTime.tm_sec;
    timeStruct.tm_min    = latestTime.tm_min;
    timeStruct.tm_hour   = latestTime.tm_hour;
    timeStruct.tm_mday   = latestTime.tm_mday;
    timeStruct.tm_wday   = latestTime.tm_wday;
    timeStruct.tm_mon    = latestTime.tm_mon;
    timeStruct.tm_year   = latestTime.tm_year;
  } else {
    INFO("Time update failed");
  }
}

void user_init(void) {
  char *buf = util_zalloc(256);

  // Init valve schedule data
  initSchedule();
  schedule_vector_init(valves_schedule.v1, 7);
  
  // I/O initialization
  gpio_init();
  
  // Initialize valves GPIO as an output
  easygpio_pinMode(VALVE1_GPIO, EASYGPIO_NOPULL, EASYGPIO_OUTPUT);
  easygpio_outputSet(VALVE1_GPIO, OFF);
  
  easygpio_pinMode(VALVE2_GPIO, EASYGPIO_NOPULL, EASYGPIO_OUTPUT);
  easygpio_outputSet(VALVE2_GPIO, OFF);
  
  easygpio_pinMode(VALVE3_GPIO, EASYGPIO_NOPULL, EASYGPIO_OUTPUT);
  easygpio_outputSet(VALVE3_GPIO, OFF);
  
  easygpio_pinMode(VALVE4_GPIO, EASYGPIO_NOPULL, EASYGPIO_OUTPUT);
  easygpio_outputSet(VALVE4_GPIO, OFF);
  
  // Initialize button GPIO input
  easygpio_pinMode(BUTTON1_GPIO, EASYGPIO_PULLUP, EASYGPIO_INPUT);
  easygpio_pinMode(BUTTON2_GPIO, EASYGPIO_PULLUP, EASYGPIO_INPUT);
  easygpio_pinMode(BUTTON3_GPIO, EASYGPIO_PULLUP, EASYGPIO_INPUT);
  easygpio_pinMode(BUTTON4_GPIO, EASYGPIO_PULLUP, EASYGPIO_INPUT);

  // Uart init
  // uart_init(BIT_RATE_115200, BIT_RATE_115200);
  os_delay_us(1000000);

  // Last will and testament
  os_sprintf(buf, "{\"muster\":{\"connstate\":\"offline\",\"device\":\"/home/lab/valve\"}}");
  MQTT_InitLWT(&mqttClient, buf, "offline", 0, 0);

  // Subtopics
  commandTopic = util_make_sub_topic(MQTT_DEV_PATH/*devicePathKey*/, "command");
  statusTopic = util_make_sub_topic(MQTT_DEV_PATH/*devicePathKey*/, "status");
  v1ScheduleTopic = util_make_sub_topic(MQTT_DEV_PATH/*devicePathKey*/, "schedule/1");
  v2ScheduleTopic = util_make_sub_topic(MQTT_DEV_PATH/*devicePathKey*/, "schedule/2");
  v3ScheduleTopic = util_make_sub_topic(MQTT_DEV_PATH/*devicePathKey*/, "schedule/3");
  v4ScheduleTopic = util_make_sub_topic(MQTT_DEV_PATH/*devicePathKey*/, "schedule/4");
  INFO("Command subtopic: %s\r\n", commandTopic);
  INFO("Status subtopic: %s\r\n", statusTopic);
  INFO("Valve 1 schedule subtopic: %s\r\n", v1ScheduleTopic);
  INFO("Valve 2 schedule subtopic: %s\r\n", v2ScheduleTopic);
  INFO("Valve 3 schedule subtopic: %s\r\n", v3ScheduleTopic);
  INFO("Valve 4 schedule subtopic: %s\r\n", v4ScheduleTopic);

  // Initialize MQTT connection  
  MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, DEFAULT_SECURITY);
  MQTT_InitClient(&mqttClient, MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, MQTT_KEEPALIVE, 1);
 
  MQTT_OnConnected(&mqttClient, mqttConnectedCb);
  MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
  MQTT_OnPublished(&mqttClient, mqttPublishedCb);
  MQTT_OnData(&mqttClient, mqttDataCb);

  WIFI_Connect(STA_SSID/*(uint8_t*) ssidKey*/, STA_PASS/*(uint8_t*)WIFIPassKey*/, wifiConnectCb);

  // Timers
  os_timer_disarm(&pulseTimer1);
  os_timer_setfn(&pulseTimer1, (os_timer_func_t *)pulseTmerExpireCb, (uint8_t)1);
  os_timer_disarm(&pulseTimer2);
  os_timer_setfn(&pulseTimer2, (os_timer_func_t *)pulseTmerExpireCb, (uint8_t)2);
  os_timer_disarm(&pulseTimer3);
  os_timer_setfn(&pulseTimer3, (os_timer_func_t *)pulseTmerExpireCb, (uint8_t)3);
  os_timer_disarm(&pulseTimer4);
  os_timer_setfn(&pulseTimer4, (os_timer_func_t *)pulseTmerExpireCb, (uint8_t)4);
  os_timer_disarm(&buttonTimer);
  os_timer_setfn(&buttonTimer, (os_timer_func_t *)buttonTimerCb, (void *)0);
  // Timer to execute code every 100 mSec
  os_timer_arm(&buttonTimer, 100, 1);

  // RTC clock
  // Set timer to check time every 100ms 
  os_timer_disarm(&clockTimer);
  os_timer_setfn(&clockTimer, (os_timer_func_t *)clockTimerCb, (void *)0);
  os_timer_arm(&clockTimer, 100, 1);
  // Init PCF8563
  pcf8563_init(); 

  // Free working buffer
  util_free(buf);


  INFO("\r\nSystem started ...\r\n");
}
