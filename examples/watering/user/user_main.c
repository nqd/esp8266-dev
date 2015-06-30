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
#include "user_interface.h"
#include "kvstore.h"
#include "wifi_config.h"
#include "mem.h"
#include "c_types.h"
#include "espconn.h"
#include "upgrade.h"
#include "mqtt.h"
#include "fota.h"


MQTT_Client mqttClient;


  
LOCAL int buttonState[VALVE_NUMBER] = {1, 1, 1, 1};
LOCAL int valveState[VALVE_NUMBER] = {OFF, OFF, OFF, OFF};
LOCAL char *commandTopic, *statusTopic;
LOCAL char *controlTopic = "/node/control";
LOCAL char *infoTopic = "/node/info";
LOCAL flash_handle_s *configHandle;
LOCAL os_timer_t pulseTimer1, pulseTimer2, pulseTimer3, pulseTimer4, buttonTimer, clockTimer;
LOCAL fota_client_t fota_client;

unsigned char *default_certificate;
unsigned int default_certificate_len = 0;
unsigned char *default_private_key;
unsigned int default_private_key_len = 0;



/**
 * Publish connection info
 */
LOCAL void ICACHE_FLASH_ATTR publishConnInfo(MQTT_Client *client)
{
  struct ip_info ipConfig;
  char *buf = util_zalloc(256); 
    
  // Publish who we are and where we live
  wifi_get_ip_info(STATION_IF, &ipConfig);
  os_sprintf(buf, "{\"muster\":{\"connstate\":\"online\",\"device\":\"home/valves\",\"ip4\":\"%d.%d.%d.%d\",\"schema\":\"hwstar_relaynode\",\"ssid\":\"%s\"}}",
      *((uint8_t *) &ipConfig.ip.addr),
      *((uint8_t *) &ipConfig.ip.addr + 1),
      *((uint8_t *) &ipConfig.ip.addr + 2),
      *((uint8_t *) &ipConfig.ip.addr + 3),
      STA_SSID);

  INFO("MQTT Node info: %s\r\n", buf);

  // Publish
  MQTT_Publish(client, infoTopic, buf, os_strlen(buf), 0, 0);
  
  // Free the buffer
  util_free(buf);
  
}


/**
 * Handle qstring command
 */

LOCAL void ICACHE_FLASH_ATTR handleQstringCommand(char *new_value, char *command)
{
  char *buf = util_zalloc(128);
  
  if(!new_value){
    const char *cur_value = kvstore_get_string(configHandle, command);
    os_sprintf(buf, "{\"%s\":\"%s\"}", command, cur_value);
    util_free(cur_value);
    INFO("Query Result: %s\r\n", buf );
    MQTT_Publish(&mqttClient, statusTopic, buf, os_strlen(buf), 0, 0);
  }
  else{
    kvstore_put(configHandle, command, new_value);
  }

  util_free(buf);

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
  MQTT_Publish(&mqttClient, statusTopic, result, os_strlen(result), 0, 0);
  
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
  MQTT_Publish(&mqttClient, statusTopic, result, os_strlen(result), 0, 0);

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
    // start_fota(&fota_client, INTERVAL, UPDATE_SERVER_IP, UPDATE_SERVER_PORT, OTA_UUID, OTA_TOKEN);
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
  
  // Control Message?
  if((!os_strcmp(topicBuf, controlTopic)) || (!os_strcmp(topicBuf, commandTopic))){
    
    jsmn_init(&p);
    r = jsmn_parse(&p, dataBuf, os_strlen(dataBuf), t, sizeof(t)/sizeof(t[0]));
    if (r < 0) {
      os_printf("Failed to parse JSON: %d\n", r);
      return;
    }

    /* Assume the top-level element is an object */
    if (r < 1 || t[0].type != JSMN_OBJECT) {
      os_printf("Object expected\n");
      return;
    }

    i = 1;
    if (jsoneq(dataBuf, &t[i], "control") == 0) {
      char *s = json_get_value_as_string(dataBuf, &t[i+1]);
      if (s != NULL) {
        if (jsoneq(dataBuf, &t[i+1], "muster") == 0) {
          publishConnInfo(&mqttClient);
        } else {
          util_assert(FALSE, "Unsupported command: %s", s);
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
            util_assert(FALSE, "Missing param", NULL);
          } else if ((jsoneq(dataBuf, &t[k], "valve") == 0) && (jsoneq(dataBuf, &t[k+2], "time") == 0)) {
            char *valve = json_get_value_as_primitive(dataBuf, &t[k+1]);
            char *duration = json_get_value_as_primitive(dataBuf, &t[k+3]);
            uint8_t valveIdx = atoi(valve);
            uint32_t millis = atoi(duration);
            if ((valveIdx > 0) && (valveIdx <= VALVE_NUMBER) && (millis > 500)) {
              valveSet(ON, valveIdx);
              os_timer_arm(getPulseTimer(valveIdx), millis, 0);
            } else {
              util_assert(FALSE, "Incorect param", NULL);
            }
            os_free(valve);
            os_free(duration);
          } else {
            util_assert(FALSE, "Missing param", NULL);
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
            util_assert(FALSE, "Missing param", NULL);
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
            util_assert(FALSE, "Missing param", NULL);
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
            util_assert(FALSE, "Missing param", NULL);
          }
        }
      } else {
        char *s = json_get_value_as_string(dataBuf, &t[i+1]);
        if (s != NULL) {
          util_assert(FALSE, "Unsupported command: %s", s);
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
LOCAL void ICACHE_FLASH_ATTR nodeTimerCb(void *arg) {
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


/**
 * System initialization
 * Called once from user_init
 */
LOCAL void ICACHE_FLASH_ATTR sysInit(void) {
  // Read in the config sector from flash
  // configHandle = kvstore_open(KVS_DEFAULT_LOC);

  // const char *ssidKey = commandElements[CMD_SSID].command;
  // const char *WIFIPassKey = commandElements[CMD_WIFIPASS].command;
  // const char *devicePathKey = commandElements[CMD_MQTTDEVPATH].command;

  // // Check for default configuration overrides
  // if(!kvstore_exists(configHandle, ssidKey)){ // if no ssid, assume the rest of the defaults need to be set as well
  //   kvstore_put(configHandle, ssidKey, configInfoBlock.e[WIFISSID].value);
  //   kvstore_put(configHandle, WIFIPassKey, configInfoBlock.e[WIFIPASS].value);
  //   kvstore_put(configHandle, devicePathKey, configInfoBlock.e[MQTTDEVPATH].value);

  //   // Write the KVS back out to flash  
  
  //   kvstore_flush(configHandle);
  // }
  
  // // Get the configurations we need from the KVS and store them in the commandElement data area
    
  // commandElements[CMD_SSID].p.sp = kvstore_get_string(configHandle, ssidKey); // Retrieve SSID
  
  // commandElements[CMD_WIFIPASS].p.sp = kvstore_get_string(configHandle, WIFIPassKey); // Retrieve WIFI Pass

  // commandElements[CMD_MQTTDEVPATH].p.sp = kvstore_get_string(configHandle, devicePathKey); // Retrieve MQTT Device Path
  
  // // Initialize MQTT connection 
  
  // uint8_t *host = configInfoBlock.e[MQTTHOST].value;
  // uint32_t port = (uint32_t) atoi(configInfoBlock.e[MQTTPORT].value);
  
  // MQTT_InitConnection(&mqttClient, host, port,
  // (uint8_t) atoi(configInfoBlock.e[MQTTSECUR].value));

  // MQTT_InitClient(&mqttClient, configInfoBlock.e[MQTTDEVID].value, 
  // configInfoBlock.e[MQTTUSER].value, configInfoBlock.e[MQTTPASS].value,
  // atoi(configInfoBlock.e[MQTTKPALIV].value), 1);

  // MQTT_OnConnected(&mqttClient, mqttConnectedCb);
  // MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
  // MQTT_OnPublished(&mqttClient, mqttPublishedCb);
  // MQTT_OnData(&mqttClient, mqttDataCb);
  
  
}

void user_init(void) {
  // sysInit();

  // Read in the config sector from flash
  configHandle = kvstore_open(KVS_DEFAULT_LOC);
  // Check for default configuration overrides
  // if(!kvstore_exists(configHandle, "ssid")){ // if no ssid, assume the rest of the defaults need to be set as well
  //   kvstore_put(configHandle, "ssid", STA_SSID);
  //   kvstore_put(configHandle, "wifipass", STA_PASS);
  //   kvstore_put(configHandle, "mqttdevpath", MQTT_DEV_PATH);

  //   // Write the KVS back out to flash  
  //   kvstore_flush(configHandle);
  // }

  // // Get the configurations we need from the KVS 
  // const char *ssidKey = kvstore_get_string(configHandle, "ssid");
  // const char *WIFIPassKey = kvstore_get_string(configHandle, "wifipass");
  // const char *devicePathKey = kvstore_get_string(configHandle, "mqttdevpath");

  char *buf = util_zalloc(256);
  
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
  uart_init(BIT_RATE_115200, BIT_RATE_115200);
  os_delay_us(1000000);

  // Last will and testament
  os_sprintf(buf, "{\"muster\":{\"connstate\":\"offline\",\"device\":\"/home/lab/valve\"}}");
  MQTT_InitLWT(&mqttClient, buf, "offline", 0, 0);

  // Subtopics
  commandTopic = util_make_sub_topic(MQTT_DEV_PATH/*devicePathKey*/, "command");
  statusTopic = util_make_sub_topic(MQTT_DEV_PATH/*devicePathKey*/, "status");
  INFO("Command subtopic: %s\r\n", commandTopic);
  INFO("Status subtopic: %s\r\n", statusTopic);

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
  os_timer_setfn(&buttonTimer, (os_timer_func_t *)nodeTimerCb, (void *)0);
  // Timer to execute code every 100 mSec
  os_timer_arm(&buttonTimer, 100, 1);

  // Free working buffer
  util_free(buf);


  INFO("\r\nSystem started ...\r\n");
}
