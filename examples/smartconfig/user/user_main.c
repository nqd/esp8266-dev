/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2014/1/1, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"
#include "driver/uart.h"
#include "user_interface.h"
#include "smartconfig.h"

#define LOG   1
#define PRINTF  os_printf

void ICACHE_FLASH_ATTR
smartconfig_done(void *data)
{
  struct station_config *sta_conf = data;

  PRINTF("ssid: %s\n", sta_conf->ssid);
  PRINTF("pass: %s\n", sta_conf->password);
  PRINTF("bssid set: %d\n", sta_conf->bssid_set);
  if (sta_conf->bssid_set) {
    PRINTF("bssid: %x:%x:%x:%x:%x:%x\n",
      sta_conf->bssid[0] & 0xff,
      sta_conf->bssid[1] & 0xff,
      sta_conf->bssid[2] & 0xff,
      sta_conf->bssid[3] & 0xff,
      sta_conf->bssid[4] & 0xff,
      sta_conf->bssid[5] & 0xff
      );
  }

  wifi_station_disconnect();
  wifi_station_set_config(sta_conf);
  wifi_station_connect();
}

//Init function 
void ICACHE_FLASH_ATTR
user_init(void)
{
  // Initialize the GPIO subsystem.
  gpio_init();
  uart_init(BIT_RATE_115200, BIT_RATE_115200);
  os_delay_us(1000000);

  os_printf("SDK version:%s\n", system_get_sdk_version());
  smartconfig_start(SC_TYPE_ESPTOUCH, smartconfig_done, LOG);
}
