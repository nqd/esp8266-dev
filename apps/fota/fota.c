#include "osapi.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"

#include "utils.h"

#include "fota.h"
#include "fota-info.h"
#include "fota-util.h"

#ifndef INFO
#define INFO os_printf
#endif

#ifndef REPORT
#define REPORT os_printf
#endif

static uint8_t is_running = 0;
uint32_t version_fwr;

LOCAL void ICACHE_FLASH_ATTR
fota_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;

  if(ipaddr == NULL) {
    INFO("DNS: Found, but got no ip\n");
    ((fota_client_t*)pespconn->reverse)->status = FOTA_IDLE;
    return;
  }

  INFO("DNS: found ip %d.%d.%d.%d\n",
      *((uint8 *) &ipaddr->addr),
      *((uint8 *) &ipaddr->addr + 1),
      *((uint8 *) &ipaddr->addr + 2),
      *((uint8 *) &ipaddr->addr + 3));


  if(ipaddr->addr != 0) {
    os_memcpy(pespconn->proto.tcp->remote_ip, &ipaddr->addr, 4);
  }

  // check for new version
  start_esp_connect(pespconn,
    FOTA_SECURE,
    get_version_connect_cb,
    get_version_disconnect_cb,
    get_version_recon_cb);
}

// is called periodically to check for next version
LOCAL void ICACHE_FLASH_ATTR
fota_ticktock(fota_client_t *fota_client)
{
  // Disable user's timers
  if(fota_client->users_timers_cb) {
    fota_client->users_timers_cb(FOTA_BUSY);
  }


  if (fota_client->status != FOTA_IDLE) {
    // INFO("FOTA is polling, quit\n");
    return;
  }

  fota_client->status = FOTA_POLLING;

  // connection
  fota_client->conn = (struct espconn *)os_zalloc(sizeof(struct espconn));
  fota_client->conn->reverse = (void*)fota_client;
  fota_client->conn->type = ESPCONN_TCP;
  fota_client->conn->state = ESPCONN_NONE;

  // new tcp connection
  fota_client->conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
  fota_client->conn->proto.tcp->local_port = espconn_port();
  fota_client->conn->proto.tcp->remote_port = fota_client->port;

  // if ip address is provided, go ahead
  if (UTILS_StrToIP(fota_client->host, &fota_client->conn->proto.tcp->remote_ip)) {
    INFO("FOTA client: Connect to ip %s:%d\r\n", fota_client->host, fota_client->port);
    // check for new version
    start_esp_connect(fota_client->conn,
      FOTA_SECURE,
      get_version_connect_cb,
      get_version_disconnect_cb,
      get_version_recon_cb);
  }
  // else, use dns query to get ip address
  else {
    INFO("FOTA client: Connect to domain %s:%d\r\n", fota_client->host, fota_client->port);
    espconn_gethostbyname(fota_client->conn,
      fota_client->host,
      (ip_addr_t *)(fota_client->conn->proto.tcp->remote_ip),
      fota_dns_found);
  }

  // Enable user's timers
  if(fota_client->users_timers_cb) {
    fota_client->users_timers_cb(FOTA_IDLE);
  }

}

void ICACHE_FLASH_ATTR
start_fota(fota_client_t *fota_client, uint32_t interval, char *host, uint16_t port, char *id, char* token, users_timers_cb_t users_timers_cb)
{
  if (is_running) {
    REPORT("FOTA is called only one time, exit\n");
    return;
  }
  else
    is_running = 1;

  // get current firmware version
  if (convert_version(VERSION, os_strlen(VERSION), &version_fwr) < 0) {
    REPORT("FOTA: Version configuration [%s] is wrong\n", VERSION);
    return;
  }

  // setup fota_client
  os_memset(fota_client, '\0', sizeof(fota_client_t));
  fota_client->interval = interval;
  fota_client->host = (char*)os_zalloc(os_strlen(host)+1);
  os_memcpy(fota_client->host, host, os_strlen(host));
  // port
  fota_client->port = port;
  // uuid
  fota_client->uuid = (char*)os_zalloc(os_strlen(id)+1);
  os_memcpy(fota_client->uuid, id, os_strlen(id));
  // token
  fota_client->token = (char*)os_zalloc(os_strlen(token)+1);
  os_memcpy(fota_client->token, token, os_strlen(token));
  // user's timers cb
  fota_client->users_timers_cb = users_timers_cb;

  // reverse
  fota_client->fw_server.reverse = (void*)fota_client;

  // status flag
  fota_client->status = FOTA_IDLE;

  os_timer_disarm(&fota_client->periodic);
  os_timer_setfn(&fota_client->periodic, (os_timer_func_t *)fota_ticktock, fota_client);
  os_timer_arm(&fota_client->periodic, fota_client->interval, 1);       // repeat
}