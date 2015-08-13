/******************************************************************************
 * Description: entry file of user application
 *
 * Modification history:
 *     2015/1/23, v1.0 create this file.
*******************************************************************************/

#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "osapi.h"
#include "upgrade.h"
#include "driver/uart.h"

#include "user_config.h"
#include "fota.h"

static os_timer_t connect_check_timer;
static fota_client_t fota_client;

// we do not use this app as SSL server
unsigned char *default_certificate;
unsigned int default_certificate_len = 0;
unsigned char *default_private_key;
unsigned int default_private_key_len = 0;

LOCAL void ICACHE_FLASH_ATTR
connect_status_check(void *arg)
{
  // os_printf("Check connecting status: %d\n", wifi_station_get_connect_status());
  if (wifi_station_get_connect_status() == STATION_GOT_IP) {
    os_printf("Got IP, start update\n");
    // start fota session
    start_fota(&fota_client, INTERVAL, UPDATE_SERVER_IP, UPDATE_SERVER_PORT, OTA_UUID, OTA_TOKEN);
  }
  else {
    os_timer_disarm(&connect_check_timer);
    os_timer_setfn(&connect_check_timer, (os_timer_func_t *)connect_status_check, NULL);
    os_timer_arm(&connect_check_timer, 5000, 0);
    // os_printf("Waiting to update ...\n");
  }
}

void user_init(void)
{
  gpio_init();
  uart_init(BIT_RATE_115200, BIT_RATE_115200);
  os_delay_us(1000000);

  // Information
  os_printf("\n");
  uart0_tx_buffer("\n", os_strlen("\n"));

  os_printf("SDK version: %s\n", system_get_sdk_version());
  os_printf("Compile time: %s %s\n",__DATE__,__TIME__);
  os_printf("Project %s with version %s\n", PROJECT, VERSION);

  // Set station mode
  char ssid[32] = SSID;
  char password[64] = SSID_PASSWORD;
  struct station_config stationConf;
  wifi_set_opmode(0x1);
  // Set ap settings
  os_memcpy(&stationConf.ssid, ssid, 32);
  os_memcpy(&stationConf.password, password, 64);
  wifi_station_set_config(&stationConf);

  connect_status_check(NULL);
}
