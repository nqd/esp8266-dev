#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_config.h"
#include "user_interface.h"
#include "driver/uart.h"

#include "wifi.h"

#include "espconn.h"
#include "mem.h"


unsigned char *default_certificate;
unsigned int default_certificate_len = 0;
unsigned char *default_private_key;
unsigned int default_private_key_len = 0;

#define NET_DOMAIN "api.ubisen.com"
#define NET_PORT    8883
#define SSL_BUFFER_SIZE   4096
#define DNS_ENABLE

const char* TLSHEAD = "GET / HTTP/1.1\r\nHost: %d.%d.%d.%d\r\nConnection: keep-alive\r\n\r\n";

#define packet_size   (2 * 1024)

LOCAL os_timer_t test_timer;
LOCAL struct espconn user_tcp_conn;
LOCAL struct _esp_tcp user_tcp;
ip_addr_t tcp_server_ip;

/******************************************************************************
 * FunctionName : user_tcp_recv_cb
 * Description  : receive callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_recv_cb(void *arg, char *pusrdata, unsigned short length)
{
  //received some data from tcp connection

  os_printf("tcp recv !!! %s \r\n", pusrdata);
   
}
/******************************************************************************
 * FunctionName : user_tcp_sent_cb
 * Description  : data sent callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_sent_cb(void *arg)
{
  //data sent successfully

  os_printf("tcp sent succeed !!! \r\n");
}
/******************************************************************************
 * FunctionName : user_tcp_discon_cb
 * Description  : disconnect callback.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_discon_cb(void *arg)
{
  //tcp disconnect successfully

  os_printf("tcp disconnect succeed !!! \r\n");
}
/******************************************************************************
 * FunctionName : user_esp_platform_sent
 * Description  : Processing the application data and sending it to the host
 * Parameters   : pespconn -- the espconn used to connetion with the host
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_sent_data(struct espconn *pespconn)
{
  char *pbuf = (char *)os_zalloc(packet_size);

  os_sprintf(pbuf, TLSHEAD,
  pespconn->proto.tcp->remote_ip[0], pespconn->proto.tcp->remote_ip[1],
  pespconn->proto.tcp->remote_ip[2], pespconn->proto.tcp->remote_ip[3]);

  espconn_secure_sent(pespconn, pbuf, os_strlen(pbuf));

  os_free(pbuf);

}

/******************************************************************************
 * FunctionName : user_tcp_connect_cb
 * Description  : A new incoming tcp connection has been connected.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_connect_cb(void *arg)
{
  struct espconn *pespconn = arg;

  os_printf("connect succeed !!! \r\n");

  espconn_regist_recvcb(pespconn, user_tcp_recv_cb);
  espconn_regist_sentcb(pespconn, user_tcp_sent_cb);
  espconn_regist_disconcb(pespconn, user_tcp_discon_cb);
 
  user_sent_data(pespconn);
}

/******************************************************************************
 * FunctionName : user_tcp_recon_cb
 * Description  : reconnect callback, error occured in TCP connection.
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_tcp_recon_cb(void *arg, sint8 err)
{
  //error occured , tcp connection broke. user can try to reconnect here.

  os_printf("reconnect callback, error code %d !!! \r\n",err);
}

#ifdef DNS_ENABLE
/******************************************************************************
 * FunctionName : user_dns_found
 * Description  : dns found callback
 * Parameters   : name -- pointer to the name that was looked up.
 *                ipaddr -- pointer to an ip_addr_t containing the IP address of
 *                the hostname, or NULL if the name could not be found (or on any
 *                other error).
 *                callback_arg -- a user-specified callback argument passed to
 *                dns_gethostbyname
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;

  if (ipaddr == NULL) {
    os_printf("user_dns_found NULL \r\n");
    return;
  }

  //dns got ip
  os_printf("user_dns_found %d.%d.%d.%d \r\n",
        *((uint8 *)&ipaddr->addr), *((uint8 *)&ipaddr->addr + 1),
        *((uint8 *)&ipaddr->addr + 2), *((uint8 *)&ipaddr->addr + 3));

  if (tcp_server_ip.addr == 0 && ipaddr->addr != 0) {
    // dns succeed, create tcp connection
    os_timer_disarm(&test_timer);
    tcp_server_ip.addr = ipaddr->addr;
    os_memcpy(pespconn->proto.tcp->remote_ip, &ipaddr->addr, 4); // remote ip of tcp server which get by dns

    pespconn->proto.tcp->remote_port = NET_PORT; // remote SSL port of tcp server

    pespconn->proto.tcp->local_port = espconn_port(); //local port of ESP8266

    espconn_regist_connectcb(pespconn, user_tcp_connect_cb); // register connect callback
    espconn_regist_reconcb(pespconn, user_tcp_recon_cb); // register reconnect callback as error handler

    espconn_secure_set_size(ESPCONN_CLIENT, SSL_BUFFER_SIZE); // set SSL buffer size, if your SSL packet larger than 2048 bytes
    espconn_secure_connect(pespconn); // tcp SSL connect
  }
}

/******************************************************************************
 * FunctionName : user_esp_platform_dns_check_cb
 * Description  : 1s time callback to check dns found
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_dns_check_cb(void *arg)
{
  struct espconn *pespconn = arg;

  espconn_gethostbyname(pespconn, NET_DOMAIN, &tcp_server_ip, user_dns_found); // recall DNS function

  os_timer_arm(&test_timer, 1000, 0);
}
#endif

/******************************************************************************
 * FunctionName : user_check_ip
 * Description  : check whether get ip addr or not
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
 void ICACHE_FLASH_ATTR
 user_check_ip(void)
 {
  struct ip_info ipconfig;

  //disarm timer first
  os_timer_disarm(&test_timer);

  //get ip info of ESP8266 station
  wifi_get_ip_info(STATION_IF, &ipconfig);

  if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0)
  {
    os_printf("got ip !!! \r\n");

      // Connect to tcp server as NET_DOMAIN
    user_tcp_conn.proto.tcp = &user_tcp;
    user_tcp_conn.type = ESPCONN_TCP;
    user_tcp_conn.state = ESPCONN_NONE;

#ifdef DNS_ENABLE
    tcp_server_ip.addr = 0;
    espconn_gethostbyname(&user_tcp_conn, NET_DOMAIN, &tcp_server_ip, user_dns_found); // DNS function

    os_timer_setfn(&test_timer, (os_timer_func_t *)user_dns_check_cb, user_tcp_conn);
    os_timer_arm(&test_timer, 1000, 0);
#else
    const char esp_server_ip[4] = {192, 168, 1, 163};

    os_memcpy(user_tcp_conn.proto.tcp->remote_ip, esp_server_ip, 4);

    user_tcp_conn.proto.tcp->remote_port = NET_PORT; // remote SSL port of tcp server

    user_tcp_conn.proto.tcp->local_port = espconn_port(); //local port of ESP8266

    espconn_regist_connectcb(&user_tcp_conn, user_tcp_connect_cb); // register connect callback
    espconn_regist_reconcb(&user_tcp_conn, user_tcp_recon_cb); // register reconnect callback as error handler

    espconn_secure_set_size(ESPCONN_CLIENT, SSL_BUFFER_SIZE); // set SSL buffer size, if your SSL packet larger than 2048 bytes
    espconn_secure_connect(&user_tcp_conn); // tcp SSL connect
#endif
  } else {
    if ((wifi_station_get_connect_status() == STATION_WRONG_PASSWORD ||
    wifi_station_get_connect_status() == STATION_NO_AP_FOUND ||
    wifi_station_get_connect_status() == STATION_CONNECT_FAIL)) {
      os_printf("connect fail !!! \r\n");
    }
    else {
      //re-arm timer to check ip
      os_timer_setfn(&test_timer, (os_timer_func_t *)user_check_ip, NULL);
      os_timer_arm(&test_timer, 100, 0);
    }
  }
}


/******************************************************************************
 * FunctionName : user_set_station_config
 * Description  : set the router info which ESP8266 station will connect to
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_set_station_config(void)
{
  // Wifi configuration
  char ssid[32] = SSID;
  char password[64] = SSID_PASSWORD;
  struct station_config stationConf;

  //need not mac address
  stationConf.bssid_set = 0;

  //Set ap settings
  os_memcpy(&stationConf.ssid, ssid, 32);
  os_memcpy(&stationConf.password, password, 64);
  wifi_station_set_config(&stationConf);

  //set a timer to check whether got ip from router succeed or not.
  os_timer_disarm(&test_timer);
  os_timer_setfn(&test_timer, (os_timer_func_t *)user_check_ip, NULL);
  os_timer_arm(&test_timer, 100, 0);

}


/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
  uart_init(BIT_RATE_115200, BIT_RATE_115200);
  os_printf("SDK version:%s\n", system_get_sdk_version());

  //Set softAP + station mode
  wifi_set_opmode(STATIONAP_MODE);

  //ESP8266 connect to router
  user_set_station_config();
}
