#include "osapi.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "upgrade.h"

#include "jsmn.h"         // json parsing
#include "utils.h"

#include "fota.h"
#include "fota-info.h"
#include "fota-firmware.h"
#include "fota-util.h"

extern uint32_t version_fwr;
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

  if (parse_fota(body, bodylen, &n_version, &n_host, &n_url, &n_protocol) < 0) {
    INFO("Invalid response\n");
    goto CLEAN_MEM;
  }
  INFO("Version %s\n", n_version);
  INFO("Host %s\n", n_host);
  INFO("URL %s\n", n_url);
  INFO("Protocol %s\n", n_protocol);

  /* then, we have valide JSON response */  
  // disable data receiving timeout handing
  // and close connection 
  os_timer_disarm(&fota_client->request_timeout);
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

  INFO("Prepare to get firmware\n");

  start_cdn(&fota_client->fw_server, n_version, n_host, n_url, n_protocol);

CLEAN_MEM:
  if (n_host!=NULL) os_free(n_host);
  if (n_url!=NULL) os_free(n_url);
  if (n_version!=NULL) os_free(n_version);
  if (n_protocol!=NULL) os_free(n_protocol);
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
  INFO("FOTA Client: sent request\n");
}

/**
  * @brief  Tcp client disconnect success callback function.
  * @param  arg: contain the ip link information
  * @retval None
  */
void ICACHE_FLASH_ATTR
get_version_disconnect_cb(void *arg)
{
  INFO("FOTA Client: disconnect\n");
  clear_tcp_of_espconn((struct espconn *)arg);
}

/**
  * @brief  Get version connection version
  * @param  arg: contain the ip link information
  * @retval None
  */
void ICACHE_FLASH_ATTR
get_version_connect_cb(void *arg)
{
  struct espconn *pespconn = (struct espconn *)arg;
  fota_client_t *fota_client = (fota_client_t *)pespconn->reverse;

  espconn_regist_recvcb(pespconn, get_version_recv);
  espconn_regist_sentcb(pespconn, get_version_sent_cb);

  uint8_t user_bin[12] = {0};
  if(system_upgrade_userbin_check() == UPGRADE_FW_BIN1) {
    os_memcpy(user_bin, "user1.bin", 10);
  }
  else if(system_upgrade_userbin_check() == UPGRADE_FW_BIN2) {
    os_memcpy(user_bin, "user2.bin", 10);
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
  espconn_secure_sent(pespconn, temp, os_strlen(temp));
#else
  espconn_sent(pespconn, temp, os_strlen(temp));
#endif
  os_free(temp);
}
