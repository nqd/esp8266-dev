#include "osapi.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "upgrade.h"

#include "fota.h"
#include "fota-cdn.h"
#include "fota-util.h"

LOCAL void upDate_discon_cb(void *arg);
LOCAL void upDate_connect_cb(void *arg);

/************************************************************************************
 * GET FIRMWARE
 ************************************************************************************/

LOCAL void ICACHE_FLASH_ATTR
clear_upgradeconn(struct upgrade_server_info *server)
{
  if (server != NULL) {
    if (server->url != NULL) {
      os_free(server->url);
      server->url = NULL;
    }
    os_free(server);
    server = NULL;
  }
}

LOCAL void ICACHE_FLASH_ATTR
cdn_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
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
  // check for new version
  start_esp_connect(pespconn, fota_cdn->secure);
}

LOCAL void ICACHE_FLASH_ATTR
upDate_rsp(void *arg)
{
  struct upgrade_server_info *server = arg;
  struct espconn *pespconn = server->pespconn;
  fota_cdn_t *fota_cdn = (fota_cdn_t *)pespconn->reverse;

  if(server->upgrade_flag == true) {
    REPORT("device_upgrade_success\n");
    system_upgrade_reboot();
  }
  else {
    REPORT("device_upgrade_failed\n");
  }

  // clear upgrade connection
  clear_upgradeconn(server);
  // we can close it now, start from client
  clear_tcp_of_espconn(pespconn);
  if (pespconn) os_free(pespconn);
  // clear url and host
  if (fota_cdn->host) os_free(fota_cdn->host);
  if (fota_cdn->url) os_free(fota_cdn->url);
}

/**
  * @brief  Tcp client disconnect success callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
LOCAL void ICACHE_FLASH_ATTR
upDate_discon_cb(void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;
  fota_cdn_t *fota_cdn = (fota_cdn_t *)pespconn->reverse;
  // clear upgrade connection
  clear_upgradeconn(fota_cdn->up_server);
  // we can close it now, start from client
  clear_tcp_of_espconn(pespconn);
  if (pespconn) os_free(pespconn);
  // clear url and host
  if (fota_cdn->host) os_free(fota_cdn->host);
  if (fota_cdn->url) os_free(fota_cdn->url);

  INFO("FOTA Client: disconnect\n");
}

/**
  * @brief  Udp server receive data callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
LOCAL void ICACHE_FLASH_ATTR
upDate_connect_cb(void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;
  fota_cdn_t *fota_cdn = (fota_cdn_t *)pespconn->reverse;
  char temp[32] = {0};
  uint8_t i = 0;

  INFO("FOTA Client: connected\n");

  system_upgrade_init();

  fota_cdn->up_server = (struct upgrade_server_info *)os_zalloc(sizeof(struct upgrade_server_info));

  fota_cdn->up_server->upgrade_version[5] = '\0';
  fota_cdn->up_server->pespconn = pespconn;
  os_memcpy(fota_cdn->up_server->ip, pespconn->proto.tcp->remote_ip, 4);
  fota_cdn->up_server->port = fota_cdn->port;
  fota_cdn->up_server->check_cb = upDate_rsp;
  fota_cdn->up_server->check_times = 60000;

  if(fota_cdn->up_server->url == NULL) {
    fota_cdn->up_server->url = (uint8 *) os_zalloc(1024);
  }

  os_sprintf(fota_cdn->up_server->url,
        "GET /%s HTTP/1.1\r\nHost: %s\r\n"pHeadStatic"\r\n",
        fota_cdn->url, fota_cdn->host);

  if(system_upgrade_start(fota_cdn->up_server) != false) {
    INFO("Fail to start system upgrade\n");
  }
}

void ICACHE_FLASH_ATTR
start_cdn(fota_cdn_t *fota_cdn, char *version, char *host, char *url, char *protocol)
{
  if (os_strncmp(protocol, "https", os_strlen("https")))
    fota_cdn->secure = 1;
  else
    fota_cdn->secure = 0;

  fota_cdn->port = 80;         // default of any cdn

  if (fota_cdn->host != NULL)
    os_free(fota_cdn->host);

  fota_cdn->host = (char*)os_zalloc(os_strlen(host)+1);
  os_strncpy(fota_cdn->host, host, os_strlen(host));

  if (fota_cdn->url != NULL)
    os_free(fota_cdn->url);

  fota_cdn->url = (char*)os_zalloc(os_strlen(url)+1);
  os_strncpy(fota_cdn->url, url, os_strlen(url));

  // connection
  if (fota_cdn->conn != NULL)
    os_free(fota_cdn->conn);

  fota_cdn->conn = (struct espconn *)os_zalloc(sizeof(struct espconn));
  fota_cdn->conn->reverse = fota_cdn;
  fota_cdn->conn->type = ESPCONN_TCP;
  fota_cdn->conn->state = ESPCONN_NONE;
  fota_cdn->conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
  fota_cdn->conn->proto.tcp->local_port = espconn_port();
  fota_cdn->conn->proto.tcp->remote_port = fota_cdn->port;

  espconn_regist_connectcb(fota_cdn->conn, upDate_connect_cb);
  // espconn_regist_reconcb(fota_client->conn, upDate_discon_cb);
  espconn_regist_disconcb(fota_cdn->conn, upDate_discon_cb);

  // if ip address is provided, go ahead
  if (UTILS_StrToIP(fota_cdn->host, &fota_cdn->conn->proto.tcp->remote_ip)) {
    INFO("CDN client: Connect to ip %s:%d\r\n", fota_cdn->host, fota_cdn->port);
    // check for new version
    start_esp_connect(fota_cdn->conn, fota_cdn->secure);
  }
  // else, use dns query to get ip address
  else {
    INFO("FOTA client: Connect to domain %s:%d\r\n", fota_cdn->host, fota_cdn->port);
    espconn_gethostbyname(fota_cdn->conn,
      fota_cdn->host,
      (ip_addr_t *) &fota_cdn->conn->proto.tcp->remote_ip,
      cdn_dns_found);
  }
}