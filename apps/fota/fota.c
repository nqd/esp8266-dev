#include "osapi.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "upgrade.h"

#include "driver/uart.h"
#include "fota.h"

// json parsing
#include "jsmn.h"

#include "fota-util.h"

static struct espconn *version_espconn = NULL;
static struct espconn *firmware_espconn = NULL;

static os_timer_t fota_delay_check;
static struct upgrade_server_info *upServer = NULL;

LOCAL void start_session(struct espconn *pespconn, void *connect_cb, void *disconnect_cb);
LOCAL void upDate_discon_cb(void *arg);
LOCAL void upDate_connect_cb(void *arg);

static fota_info_t fota_info;
static int32_t version_fwr;

LOCAL void ICACHE_FLASH_ATTR
clear_espconn(struct espconn *conn) {
  if (conn != NULL) {
    if (conn->proto.tcp != NULL) {
      os_free(conn->proto.tcp);
      conn->proto.tcp = NULL;
    }
    os_free(conn);
    conn = NULL;
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
  // INFO("Get version receive %s\n", pusrdata);
  /* get body */
  char *body = (char*)os_strstr(pusrdata, "\r\n\r\n");
  if (body == NULL) {
    INFO("Invalide response\n");
    return;
  }
  INFO("Body: %s\n", body+4);
  uint32_t bodylen = os_strlen(body);

  /* parse json, get version */  
  int32_t version;
  if ((version = parse_version(body, bodylen)) < 0) {
    INFO("Invalid response\n");
    return;
  }
  /* then, we have version response */  
  // disable data receiving timeout handing
  // and close connection (server may close connection now, but just to make sure) 
  os_timer_disarm(&fota_delay_check);

  clear_espconn(version_espconn);

  /* if we have newer version, disable timeout, and call get firmware session */
  if (version > version_fwr) {
    INFO("Starting update new firmware\n");
    start_session(firmware_espconn, upDate_connect_cb, upDate_discon_cb);
  }
  else {
    INFO("We have lastest firmware (current %x vs online %x)\n", version_fwr, version);
  }
}

/**
  * @brief  after sending (version) request, wait for reply timeout
  *         we do not received right answer, close connection
  * @param  arg: contain the ip link information
  * @retval None
  */
LOCAL void ICACHE_FLASH_ATTR
get_version_wait(void *arg)
{
  os_timer_disarm(&fota_delay_check);

  INFO("get version timeout, close connection\n");
  clear_espconn(version_espconn);
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
  os_timer_disarm(&fota_delay_check);
  os_timer_setfn(&fota_delay_check, (os_timer_func_t *)get_version_wait, pespconn);
  os_timer_arm(&fota_delay_check, 5000, 0);
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
  clear_espconn(version_espconn);
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

  espconn_regist_recvcb(pespconn, get_version_recv);
  espconn_regist_sentcb(pespconn, get_version_sent_cb);

  char *temp = NULL;
  temp = (uint8 *) os_zalloc(512);

  os_sprintf(temp, "GET /firmware/%s/versions HTTP/1.0\r\nHost: "IPSTR"\r\n"pHeadStatic""pHeadAuthen"\r\n",
    PROJECT,
    IP2STR(pespconn->proto.tcp->remote_ip),
    fota_info.uuid,
    fota_info.token,
    fota_info.host,
    fota_info.version
    );

  espconn_sent(pespconn, temp, os_strlen(temp));
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
  clear_espconn(firmware_espconn);
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
  clear_espconn(firmware_espconn);
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
  uint8_t user_bin[12] = {0};
  uint8_t i = 0;

  system_upgrade_init();

  INFO("+CIPUPDATE:3\n");

  upServer = (struct upgrade_server_info *)os_zalloc(sizeof(struct upgrade_server_info));

  // todo:
  upServer->upgrade_version[5] = '\0';
  upServer->pespconn = pespconn;
  os_memcpy(upServer->ip, pespconn->proto.tcp->remote_ip, 4);
  upServer->port = fota_info.port;        // todo: get from meta data
  upServer->check_cb = upDate_rsp;
  upServer->check_times = 60000;

  if(upServer->url == NULL) {
    upServer->url = (uint8 *) os_zalloc(1024);
  }

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
start_session(struct espconn *pespconn, void *connect_cb, void *disconnect_cb)
{
  pespconn = (struct espconn *)os_zalloc(sizeof(struct espconn));
  pespconn->type = ESPCONN_TCP;
  pespconn->state = ESPCONN_NONE;
  pespconn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
  pespconn->proto.tcp->local_port = espconn_port();
  pespconn->proto.tcp->remote_port = fota_info.port;

  INFO("+CIPUPDATE:1\n");
  os_memcpy(pespconn->proto.tcp->remote_ip, &fota_info.server, 4);

  espconn_regist_connectcb(pespconn, connect_cb);
  espconn_regist_reconcb(pespconn, disconnect_cb);
  espconn_regist_disconcb(pespconn, disconnect_cb);
  espconn_connect(pespconn);
}

void ICACHE_FLASH_ATTR
start_fota(fota_info_t *info)
{
  os_memcpy(&fota_info.server, &info->server, sizeof(fota_info.server));

  fota_info.port = info->port;
  os_memcpy(&fota_info.uuid, &info->uuid, os_strlen(info->uuid));
  os_memcpy(&fota_info.token, &info->token, os_strlen(info->token));
  os_memcpy(&fota_info.host, &info->host, os_strlen(info->host));
  os_memcpy(&fota_info.version, &info->version, os_strlen(info->version));

  // get current firmware version
  version_fwr = convert_version(fota_info.version, os_strlen(fota_info.version));
  if (version_fwr < 0) {
    REPORT("Version configuration is wrong\n");
    return;
  }

  // check for new version
  start_session(version_espconn, get_version_connect_cb, get_version_disconnect_cb);
}
