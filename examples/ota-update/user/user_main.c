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

#define MIN(a,b) ((a)>(b)?(b):(a))

LOCAL void ICACHE_FLASH_ATTR
connect_status_check(void *arg)
{
  // os_printf("Check connecting status: %d\n", wifi_station_get_connect_status());
  if (wifi_station_get_connect_status() == STATION_GOT_IP) {
    os_printf("Got IP, start update\n");
    // start fota session
    fota_info_t info;
    info.server.addr = ipaddr_addr(UPDATE_SERVER_IP);
    info.port = UPDATE_SERVER_PORT;

    os_memset(info.uuid, '\0', sizeof(info.uuid));
    os_memset(info.token, '\0', sizeof(info.token));
    os_memset(info.client, '\0', sizeof(info.client));
    os_memset(info.version, '\0', sizeof(info.version));

    os_memcpy(info.uuid, OTA_UUID, MIN(sizeof(info.uuid), strlen(OTA_UUID)));
    os_memcpy(info.token, OTA_TOKEN, MIN(sizeof(info.token), strlen(OTA_TOKEN)));
    os_memcpy(info.client, OTA_CLIENT, MIN(sizeof(info.client), strlen(OTA_CLIENT)));
    os_memcpy(info.version, OTA_VERSION, MIN(sizeof(info.version), strlen(OTA_VERSION)));

    start_fota(&info);
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

  os_printf("SDK version:%s\n", system_get_sdk_version());
  os_printf("Compile time:%s %s\n",__DATE__,__TIME__);
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
