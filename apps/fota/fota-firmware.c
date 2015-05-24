#include "osapi.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "upgrade.h"

#include "fota.h"
#include "fota-firmware.h"
#include "fota-util.h"

LOCAL void upgrade_discon_cb(void *arg);
LOCAL void upgrade_connect_cb(void *arg);

/************************************************************************************
 * GET FIRMWARE
 ************************************************************************************/

LOCAL void ICACHE_FLASH_ATTR
clear_upgradeconn(struct upgrade_server_info *server)
{
  if (server != NULL) {
    if (server->url != NULL) {
      FREE(server->url);
    }
    FREE(server);
  }
}

LOCAL void ICACHE_FLASH_ATTR
upgrade_response(void *arg)
{
  struct upgrade_server_info *server = arg;
  struct espconn *pespconn = server->pespconn;
  fota_cdn_t *fota_cdn = (fota_cdn_t *)pespconn->reverse;

  if(server->upgrade_flag == true) {
    REPORT("Firmware upgrade success\n");
    system_upgrade_reboot();
  }
  else {
    REPORT("Firmware upgrade fail\n");
  }

if (fota_cdn->secure)
  espconn_secure_disconnect(pespconn);
else
  espconn_disconnect(pespconn);
}

LOCAL void ICACHE_FLASH_ATTR
upgrade_recon_cb(void *arg, sint8 err)
{
  //error occured , tcp connection broke. user can try to reconnect here.
  INFO("reconnect callback, error code %d !!!\n",err);
//   struct espconn *pespconn = (struct espconn *)arg;
//   fota_cdn_t *fota_cdn = (fota_cdn_t *)pespconn->reverse;
// if (fota_cdn->secure)
//   espconn_secure_disconnect(pespconn);
// else
//   espconn_disconnect(pespconn);
}

/**
  * @brief  Tcp client disconnect success callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
LOCAL void ICACHE_FLASH_ATTR
upgrade_discon_cb(void *arg)
{
  INFO("Firmware client: Disconnect\n");

  struct espconn *pespconn = (struct espconn *)arg;
  fota_cdn_t *fota_cdn = (fota_cdn_t *)pespconn->reverse;
  // clear upgrade connection
  clear_upgradeconn(fota_cdn->up_server);
  // we can close it now, start from client
  clear_espconn(pespconn);
  // clear url and host
  FREE(fota_cdn->host);
  FREE(fota_cdn->url);
}

/**
  * @brief  Udp server receive data callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
LOCAL void ICACHE_FLASH_ATTR
upgrade_connect_cb(void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;
  fota_cdn_t *fota_cdn = (fota_cdn_t *)pespconn->reverse;
  char temp[32] = {0};
  uint8_t i = 0;

  INFO("Firmware client: Connected\n");

  system_upgrade_init();

  fota_cdn->up_server = (struct upgrade_server_info *)os_zalloc(sizeof(struct upgrade_server_info));

  fota_cdn->up_server->upgrade_version[5] = '\0';       // no, we dont use this
  fota_cdn->up_server->pespconn = pespconn;
  os_memcpy(fota_cdn->up_server->ip, pespconn->proto.tcp->remote_ip, 4);
  fota_cdn->up_server->port = fota_cdn->port;
  fota_cdn->up_server->check_cb = upgrade_response;
  fota_cdn->up_server->check_times = 60000;

  if(fota_cdn->up_server->url == NULL) {
    fota_cdn->up_server->url = (uint8 *) os_zalloc(1024);
  }

  os_sprintf(fota_cdn->up_server->url,
        "GET %s HTTP/1.1\r\nHost: %s\r\n"pHeadStatic"\r\n",
        fota_cdn->url, fota_cdn->host);

  if(system_upgrade_start(fota_cdn->up_server) == false) {
    INFO("Fail to start system upgrade\n");
  }
}

LOCAL void ICACHE_FLASH_ATTR
upgrade_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
  if(ipaddr == NULL) {
    INFO("DNS: Found, but got no ip\n");
    return;
  }

  INFO("DNS: found ip %d.%d.%d.%d\n",
      *((uint8 *) &ipaddr->addr),
      *((uint8 *) &ipaddr->addr + 1),
      *((uint8 *) &ipaddr->addr + 2),
      *((uint8 *) &ipaddr->addr + 3));

  struct espconn *pespconn = (struct espconn *)arg;
  fota_cdn_t *fota_cdn = (fota_cdn_t *)pespconn->reverse;

  // INFO("DNS dest ip: %d.%d.%d.%d:%d\n",
  //   pespconn->proto.tcp->remote_ip[0],
  //   pespconn->proto.tcp->remote_ip[1],
  //   pespconn->proto.tcp->remote_ip[2],
  //   pespconn->proto.tcp->remote_ip[3],
  //   pespconn->proto.tcp->remote_port);

  if(ipaddr->addr != 0) {
    os_memcpy(pespconn->proto.tcp->remote_ip, &ipaddr->addr, 4);
  }
  // check for new version
  start_esp_connect(fota_cdn->conn,
    fota_cdn->secure,
    upgrade_connect_cb,
    upgrade_discon_cb,
    upgrade_recon_cb);
}

void ICACHE_FLASH_ATTR
start_cdn(fota_cdn_t *fota_cdn, char *version, char *host, char *url, char *protocol)
{
  os_memset(fota_cdn, '\0', sizeof(fota_cdn_t));

  if (os_strncmp(protocol, "https", os_strlen("https")) == 0) {
    fota_cdn->secure = 1;
    fota_cdn->port = 443;
  }
  else {
    fota_cdn->secure = 0;
    fota_cdn->port = 80;
  }

  fota_cdn->host = (char*)os_zalloc(os_strlen(host)+1);
  os_strncpy(fota_cdn->host, host, os_strlen(host));

  fota_cdn->url = (char*)os_zalloc(os_strlen(url)+1);
  os_strncpy(fota_cdn->url, url, os_strlen(url));

  // connection
  fota_cdn->conn = (struct espconn *)os_zalloc(sizeof(struct espconn));
  fota_cdn->conn->reverse = (void*)fota_cdn;
  fota_cdn->conn->type = ESPCONN_TCP;
  fota_cdn->conn->state = ESPCONN_NONE;
  // new tcp connection
  fota_cdn->conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
  fota_cdn->conn->proto.tcp->local_port = espconn_port();
  fota_cdn->conn->proto.tcp->remote_port = fota_cdn->port;

  // if ip address is provided, go ahead
  if (UTILS_StrToIP(fota_cdn->host, &fota_cdn->conn->proto.tcp->remote_ip)) {
    INFO("Firmware client: Connect to %s:%d\r\n", fota_cdn->host, fota_cdn->port);
    // check for new version
    start_esp_connect(fota_cdn->conn,
      fota_cdn->secure,
      upgrade_connect_cb,
      upgrade_discon_cb,
      upgrade_recon_cb);
  }
  // else, use dns query to get ip address
  else {
    INFO("Firmware client: Connect to domain %s:%d\r\n", fota_cdn->host, fota_cdn->port);
    espconn_gethostbyname(fota_cdn->conn,
      fota_cdn->host,
      (ip_addr_t *)(fota_cdn->conn->proto.tcp->remote_ip),
      upgrade_dns_found);
  }
}