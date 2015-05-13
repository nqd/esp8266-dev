#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_config.h"
#include "user_interface.h"
#include "driver/uart.h"

#define DOMAIN    "ubisen.com"

LOCAL struct espconn user_conn;
LOCAL ip_addr_t esp_server_ip;
LOCAL os_timer_t client_timer;

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

// LOCAL void ICACHE_FLASH_ATTR                                                    
// user_esp_platform_dns_check_cb(void *arg)                                         
// {                                                                               
//     struct espconn *pespconn = arg;                                             
//     os_printf("user_esp_platform_dns_check_cb\n");                                
//     espconn_gethostbyname(pespconn, DOMAIN, &esp_server_ip, user_esp_platform_dns_found);
//     os_timer_arm(&client_timer, 1000, 0);                                       
// }

//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
    char ssid[32] = SSID;
    char password[64] = SSID_PASSWORD;
    struct station_config stationConf;

    // UART setup
    uart_init(BIT_RATE_115200, BIT_RATE_115200);

    //Set station mode
    wifi_set_opmode( 0x1 );

    //Set ap settings
    os_memcpy(&stationConf.ssid, ssid, 32);
    os_memcpy(&stationConf.password, password, 64);
    wifi_station_set_config(&stationConf);

    esp_server_ip.addr = 0;                                                     
    espconn_gethostbyname(&user_conn, DOMAIN, &esp_server_ip, user_esp_platform_dns_found);

    // os_timer_disarm(&client_timer);
    // os_timer_setfn(&client_timer, (os_timer_func_t *)user_esp_platform_dns_check_cb, pespconn);
    // os_timer_arm(&client_timer, 1000, 0);
}
