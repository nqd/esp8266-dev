#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_config.h"
#include "user_interface.h"
#include "driver/uart.h"

#include "wifi.h"

#define DOMAIN    "dl.dropboxusercontent.com"

LOCAL struct espconn user_conn;
LOCAL ip_addr_t esp_server_ip;

LOCAL void ICACHE_FLASH_ATTR
user_esp_platform_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
    // struct espconn *pespconn = (struct espconn *)arg;

    if (ipaddr == NULL) {
        os_printf("user_esp_platform_dns_found NULL\n");
        return;
    }

    os_printf("user_esp_platform_dns_found %d.%d.%d.%d\n",
            *((uint8 *)&ipaddr->addr), *((uint8 *)&ipaddr->addr + 1),
            *((uint8 *)&ipaddr->addr + 2), *((uint8 *)&ipaddr->addr + 3));
}

LOCAL void ICACHE_FLASH_ATTR
wifiConnectCb(uint8_t status)
{
  if(status == STATION_GOT_IP){
    espconn_gethostbyname(&user_conn, DOMAIN, &esp_server_ip, user_esp_platform_dns_found);
  }
}

//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
    // UART setup
    uart_init(BIT_RATE_115200, BIT_RATE_115200);

    WIFI_Connect(SSID, SSID_PASSWORD, wifiConnectCb);
}
