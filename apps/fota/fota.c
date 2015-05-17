#include "osapi.h"
#include "c_types.h"
#include "user_interface.h"
#include "espconn.h"
#include "mem.h"
#include "upgrade.h"

#include "jsmn.h"         // json parsing
#include "utils.h"

#include "fota.h"
#include "fota-cdn.h"
#include "fota-util.h"

unsigned char *default_certificate;
unsigned int default_certificate_len = 0;
unsigned char *default_private_key;
unsigned int default_private_key_len = 0;

static uint8_t is_running = 0;
static uint32_t version_fwr;

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
LOCAL void ICACHE_FLASH_ATTR
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
LOCAL void ICACHE_FLASH_ATTR
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



LOCAL void ICACHE_FLASH_ATTR
fota_dns_found(const char *name, ip_addr_t *ipaddr, void *arg)
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
  // check for new version
  start_esp_connect(pespconn, FOTA_SECURE);
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
  if (UTILS_StrToIP(fota_client->host, &fota_client->conn->proto.tcp->remote_ip)) {
    INFO("FOTA client: Connect to ip %s:%d\r\n", fota_client->host, fota_client->port);
    // check for new version
    start_esp_connect(fota_client->conn, FOTA_SECURE);
  }
  // else, use dns query to get ip address
  else {
    INFO("FOTA client: Connect to domain %s:%d\r\n", fota_client->host, fota_client->port);
    espconn_gethostbyname(fota_client->conn,
      fota_client->host,
      (ip_addr_t *) &fota_client->conn->proto.tcp->remote_ip,
      fota_dns_found);
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

  espconn_regist_connectcb(fota_client->conn, get_version_connect_cb);
  // espconn_regist_reconcb(fota_client->conn, get_version_disconnect_cb);
  espconn_regist_disconcb(fota_client->conn, get_version_disconnect_cb);

  os_timer_disarm(&fota_client->periodic);
  os_timer_setfn(&fota_client->periodic, (os_timer_func_t *)fota_ticktock, fota_client);
  os_timer_arm(&fota_client->periodic, fota_client->interval, 0);       // do not repeat
}