#include "osapi.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "upgrade.h"

#include "driver/uart.h"

#include "jsmn.h"         // json parsing
#include "utils.h"

#include "fota.h"
#include "fota-util.h"

static is_running = 0;

static struct espconn *firmware_espconn = NULL;
static struct upgrade_server_info *upServer = NULL;

LOCAL void start_session(fota_client_t *fota_client, void *connect_cb, void *disconnect_cb);
LOCAL void upDate_discon_cb(void *arg);
LOCAL void upDate_connect_cb(void *arg);

static fota_client_t fota_client;
static uint32_t version_fwr;

LOCAL void ICACHE_FLASH_ATTR
clear_tcp_of_espconn(struct espconn *conn) {
  if (conn != NULL) {
    if (conn->proto.tcp != NULL) {
      os_free(conn->proto.tcp);
      conn->proto.tcp = NULL;
    }
    // os_free(conn);
    // conn = NULL;
  }
}

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

/************************************************************************************
 * GET VERSION
 ************************************************************************************/

/**
  * @brief  response for get version request.
  *         parse answer, save online version to flash.
  * @param  arg: contain the ip link information
  *         pusrdata: data
  *         len: len of data (strlen)
  * @retval None
  */
LOCAL void ICACHE_FLASH_ATTR
get_version_recv(void *arg, char *pusrdata, unsigned short len)
{
  struct espconn *pespconn = arg;
  fota_client_t *fota_client = (fota_client_t *)pespconn->reverse;

  /* get body */
  char *body = (char*)os_strstr(pusrdata, "\r\n\r\n");
  if (body == NULL) {
    INFO("Invalide response\n");
    return;
  }
  INFO("Body: %s\n", body+4);
  uint32_t bodylen = os_strlen(body);

  /* parse json, get version */  
  char *n_host,
       *n_url,
       *n_version,
       *n_protocol;

  if (parse_fota(body, bodylen, n_version, n_host, n_url, n_protocol) < 0) {
    INFO("Invalid response\n");
    goto CLEAN_MEM;
  }

  /* then, we have valide JSON response */  
  // disable data receiving timeout handing
  // and close connection 
  os_timer_disarm(&(fota_client->request_timeout));
  clear_tcp_of_espconn(fota_client->conn);

  uint32_t version;
  if (convert_version(n_version, os_strlen(n_version), &version) < 0) {
    REPORT("Invalide version return %s\n", n_version);
    goto CLEAN_MEM;
  }

  /* if we still have lastest version */
  if (version <= version_fwr) {
    INFO("We have lastest firmware (current %u.%u.%u vs online %u.%u.%u)\n", 
      (version_fwr/256/256)%256, (version_fwr/256)%256, version_fwr%256,
      (version/256/256)%256, (version/256)%256, version%256);
    goto CLEAN_MEM;
  }

  if (os_strncmp(n_protocol, "https", os_strlen("https")))
    fota_client->fw_server.secure = 1;
  else
    fota_client->fw_server.secure = 0;

  // copy 
  fota_client->fw_server.host = n_host;
  fota_client->fw_server.url = n_url;

CLEAN_MEM:
  if (n_host) os_free(n_host);
  if (n_url) os_free(n_url);
  if (n_version) os_free(n_version);
  if (n_protocol) os_free(n_protocol);
}

/**
  * @brief  after sending (version) request, wait for reply timeout
  *         we do not received right answer, close connection
  * @param  arg: contain the ip link information
  * @retval None
  */
LOCAL void ICACHE_FLASH_ATTR
get_version_timeout(void *arg)
{
  struct espconn *pespconn = arg;
  fota_client_t *fota_client = (fota_client_t *)pespconn->reverse;
  os_timer_disarm(&fota_client->request_timeout);

  INFO("get version timeout, close connection\n");
  clear_tcp_of_espconn(pespconn);
}

/**
  * @brief  sent callback, data has been set successfully, and ack by the
  *         remote host. Set timmer to wait for reply
  * @param  arg: contain the ip link information
  * @retval None
  */
LOCAL void ICACHE_FLASH_ATTR
get_version_sent_cb(void *arg)
{
  struct espconn *pespconn = arg;
  fota_client_t *fota_client = (fota_client_t *)pespconn->reverse;

  os_timer_disarm(&fota_client->request_timeout);
  os_timer_setfn(&fota_client->request_timeout, (os_timer_func_t *)get_version_timeout, pespconn);
  os_timer_arm(&fota_client->request_timeout, 5000, 0);
  INFO("get version sent cb\n");
}

/**
  * @brief  Tcp client disconnect success callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
LOCAL void ICACHE_FLASH_ATTR
get_version_disconnect_cb(void *arg)
{
  INFO("get version disconnect tcp\n");
  clear_tcp_of_espconn((struct espconn *)arg);
}

/**
  * @brief  Get version connection version
  * @param  arg: contain the ip link information
  * @retval None
  */
LOCAL void ICACHE_FLASH_ATTR
get_version_connect_cb(void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;
  fota_client_t *fota_client = (fota_client_t *)pespconn->reverse;

  espconn_regist_recvcb(pespconn, get_version_recv);
  espconn_regist_sentcb(pespconn, get_version_sent_cb);

  uint8_t user_bin[12] = {0};
  if(system_upgrade_userbin_check() == UPGRADE_FW_BIN1) {
    os_memcpy(user_bin, "user2.bin", 10);
  }
  else if(system_upgrade_userbin_check() == UPGRADE_FW_BIN2) {
    os_memcpy(user_bin, "user1.bin", 10);
  }

  char *temp = NULL;
  temp = (uint8 *) os_zalloc(512);

  os_sprintf(temp, "GET /firmware/%s/versions HTTP/1.0\r\nHost: %s\r\n"pHeadStatic""pHeadAuthen"\r\n",
    PROJECT,
    fota_client->host,
    fota_client->uuid,   //pHeaderAuthen
    fota_client->token,
    FOTA_CLIENT,
    user_bin,
    VERSION
    );

#if (FOTA_SECURE)
  espconn_sent(pespconn, temp, os_strlen(temp));
#else
  espconn_secure_sent(pespconn, temp, os_strlen(temp));
#endif
  os_free(temp);
}

/************************************************************************************
 * GET FIRMWARE
 ************************************************************************************/

LOCAL void ICACHE_FLASH_ATTR
upDate_rsp(void *arg)
{
  struct upgrade_server_info *server = arg;

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
  clear_tcp_of_espconn(firmware_espconn);
}

/**
  * @brief  Tcp client disconnect success callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
LOCAL void ICACHE_FLASH_ATTR
upDate_discon_cb(void *arg)
{
  // clear upgrade connection
  clear_upgradeconn(upServer);
  // we can close it now, start from client
  clear_tcp_of_espconn(firmware_espconn);
  INFO("update disconnect\n");
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
  char temp[32] = {0};
  uint8_t i = 0;

  system_upgrade_init();

  INFO("+CIPUPDATE:3\n");

  upServer = (struct upgrade_server_info *)os_zalloc(sizeof(struct upgrade_server_info));

  // todo:
  upServer->upgrade_version[5] = '\0';
  upServer->pespconn = pespconn;
  os_memcpy(upServer->ip, pespconn->proto.tcp->remote_ip, 4);
  upServer->port = fota_client.port;        // todo: get from meta data
  upServer->check_cb = upDate_rsp;
  upServer->check_times = 60000;

  if(upServer->url == NULL) {
    upServer->url = (uint8 *) os_zalloc(1024);
  }

  uint8_t user_bin[12] = {0};
  if(system_upgrade_userbin_check() == UPGRADE_FW_BIN1) {
    os_memcpy(user_bin, "user2.bin", 10);
  }
  else if(system_upgrade_userbin_check() == UPGRADE_FW_BIN2) {
    os_memcpy(user_bin, "user1.bin", 10);
  }

  os_sprintf(upServer->url,
        "GET /%s HTTP/1.1\r\nHost: "IPSTR"\r\n"pHeadStatic"\r\n",
        user_bin, IP2STR(upServer->ip));

  if(system_upgrade_start(upServer) != false) {
    INFO("+CIPUPDATE:4\n");
  }
}


/************************************************************************************
 *                      FIRMWARE OVER THE AIR UPDATE
 ************************************************************************************/

LOCAL void ICACHE_FLASH_ATTR
start_session(fota_client_t *fota_client, void *connect_cb, void *disconnect_cb)
{
  os_memcpy(fota_client->conn->proto.tcp->remote_ip, &fota_client->ip, 4);

  espconn_regist_connectcb(fota_client->conn, connect_cb);
  espconn_regist_reconcb(fota_client->conn, disconnect_cb);
  espconn_regist_disconcb(fota_client->conn, disconnect_cb);
#if (FOTA_SECURE)
  espconn_secure_connect(fota_client->conn);
#else
  espconn_connect(fota_client->conn);
#endif
}

LOCAL void ICACHE_FLASH_ATTR
fota_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
{
  if(ipaddr == NULL) {
    INFO("DNS: Found, but got no ip, try to reconnect\n");
    return;
  }

  INFO("DNS: found ip %d.%d.%d.%d\n",
      *((uint8 *) &ipaddr->addr),
      *((uint8 *) &ipaddr->addr + 1),
      *((uint8 *) &ipaddr->addr + 2),
      *((uint8 *) &ipaddr->addr + 3));

  struct espconn *pespconn = (struct espconn *)arg;
  fota_client_t *fota_client = (fota_client_t *)pespconn->reverse;
  // check for new version
  start_session(fota_client, get_version_connect_cb, get_version_disconnect_cb);
}

// is called periodically to check for next version
LOCAL void ICACHE_FLASH_ATTR
fota_ticktock(fota_client_t *fota_client)
{
  // new tcp connection
  // remember to clean tcp connection after each using
  fota_client->conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
  fota_client->conn->proto.tcp->local_port = espconn_port();
  fota_client->conn->proto.tcp->remote_port = fota_client->port;

  // if ip address is provided, go ahead
  if (UTILS_StrToIP(fota_client->host, &fota_client->ip)) {
    INFO("FOTA client: Connect to ip  %s:%d\r\n", fota_client->host, fota_client->port);
    // check for new version
    start_session(fota_client, get_version_connect_cb, get_version_disconnect_cb);
  }
  // else, use dns query to get ip address
  else {
    INFO("FOTA client: Connect to domain %s:%d\r\n", fota_client->host, fota_client->port);
    espconn_gethostbyname(fota_client->conn, fota_client->host, &fota_client->ip, fota_dns_found);
  }
}

void ICACHE_FLASH_ATTR
start_fota(fota_client_t *fota_client, uint16_t interval, char *host, uint16_t port, char *id, char* token)
{
  if (is_running) {
    REPORT("FOTA is called only one time, exit\n");
    return;
  }
  else
    is_running = 1;

  // get current firmware version
  if (convert_version(VERSION, os_strlen(VERSION), &version_fwr) < 0) {
    REPORT("Version configuration [%s] is wrong\n", VERSION);
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

  // connection
  fota_client->conn = (struct espconn *)os_zalloc(sizeof(struct espconn));
  fota_client->conn->reverse = fota_client;
  fota_client->conn->type = ESPCONN_TCP;
  fota_client->conn->state = ESPCONN_NONE;

  os_timer_disarm(&fota_client->periodic);
  os_timer_setfn(&fota_client->periodic, (os_timer_func_t *)fota_ticktock, fota_client);
  os_timer_arm(&fota_client->periodic, fota_client->interval, 0);       // do not repeat
}