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
#include "mqtt.h"
#include "wifi.h"
#include "debug.h"
#include "gpio.h"
#include "user_interface.h"
#include "mem.h"

MQTT_Client mqttClient;

// we do not use this app as SSL server
unsigned char *default_certificate;
unsigned int default_certificate_len = 0;
unsigned char *default_private_key;
unsigned int default_private_key_len = 0;

void do_report(void *args)
{
  static ETSTimer report_timer;
  os_timer_disarm(&report_timer);
  os_timer_setfn(&report_timer, (os_timer_func_t *)do_report, NULL);

  char s_sensor[40];
  unsigned int doTemp = rand()%10;
  os_sprintf(s_sensor, "{\"currentValue\":2%d}", doTemp);

  MQTT_Publish(&mqttClient, MQTT_TOPIC, s_sensor, strlen(s_sensor), 0, 0);

  os_timer_arm(&report_timer, 1000*10, 0);
}

void wifiConnectCb(uint8_t status)
{
  if(status == STATION_GOT_IP){
    INFO("AP connected\n");
    MQTT_Connect(&mqttClient);
  } else {
    MQTT_Disconnect(&mqttClient);
  }
}
void mqttConnectedCb(uint32_t *args)
{
  MQTT_Client* client = (MQTT_Client*)args;
  INFO("MQTT: Connected\r\n");
  MQTT_Subscribe(client, MQTT_TOPIC, 0);

  do_report((void*)NULL);
}

void mqttDisconnectedCb(uint32_t *args)
{
  MQTT_Client* client = (MQTT_Client*)args;
  INFO("MQTT: Disconnected\r\n");
}

void mqttPublishedCb(uint32_t *args)
{
  MQTT_Client* client = (MQTT_Client*)args;
  INFO("MQTT: Published\r\n");
}

void mqttDataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
  char *topicBuf = (char*)os_zalloc(topic_len+1),
      *dataBuf = (char*)os_zalloc(data_len+1);

  MQTT_Client* client = (MQTT_Client*)args;

  os_memcpy(topicBuf, topic, topic_len);
  topicBuf[topic_len] = 0;

  os_memcpy(dataBuf, data, data_len);
  dataBuf[data_len] = 0;

  INFO("Receive topic: %s, data: %s \r\n", topicBuf, dataBuf);
  os_free(topicBuf);
  os_free(dataBuf);
}

void user_init(void)
{
  uart_init(BIT_RATE_115200, BIT_RATE_115200);
  os_delay_us(1000000);

  MQTT_InitConnection(&mqttClient, MQTT_HOST, MQTT_PORT, DEFAULT_SECURITY);
  MQTT_InitClient(&mqttClient, MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS, MQTT_KEEPALIVE, 1);

  MQTT_InitLWT(&mqttClient, "/lwt", "offline", 0, 0);
  MQTT_OnConnected(&mqttClient, mqttConnectedCb);
  MQTT_OnDisconnected(&mqttClient, mqttDisconnectedCb);
  MQTT_OnPublished(&mqttClient, mqttPublishedCb);
  MQTT_OnData(&mqttClient, mqttDataCb);

  WIFI_Connect(STA_SSID, STA_PASS, wifiConnectCb);

  INFO("\r\nSystem started ...\r\n");
}
