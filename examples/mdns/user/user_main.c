#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"
#include "driver/uart.h"
#include "espconn.h"
#include "mem.h"

#include "wifi.h"

#if 1
#define INFO(...) os_printf(__VA_ARGS__)
#else
#define INFO(...)
#endif

LOCAL void ICACHE_FLASH_ATTR
mdns_init()
{
  struct ip_info ipConfig;
  struct mdns_info *info = (struct mdns_info *)os_zalloc(sizeof(struct mdns_info));

  wifi_get_ip_info(STATION_IF, &ipConfig);
  if (!(wifi_station_get_connect_status() == STATION_GOT_IP && ipConfig.ip.addr != 0)) {
    INFO("Cannot read station configuration\n");
    return;
  }

  info->host_name = "ubisen";
  info->ipAddr = ipConfig.ip.addr; //ESP8266 station IP
  info->server_name = "module01";
  info->server_port = 80;
  info->txt_data[0] = "version = now";
  info->txt_data[1] = "user1 = data1";
  info->txt_data[2] = "user2 = data2";
  espconn_mdns_init(info);
  // espconn_mdns_server_register();
  espconn_mdns_enable();
}

LOCAL void ICACHE_FLASH_ATTR
wifiConnectCb(uint8_t status)
{
  if(status == STATION_GOT_IP){
    mdns_init();
  } else {
  }
}

void user_init(void)
{
  uart_init(BIT_RATE_115200, BIT_RATE_115200);
  os_delay_us(1000000);


  WIFI_Connect(SSID, SSID_PASSWORD, wifiConnectCb);

  INFO("\r\nSystem started ...\r\n");
}