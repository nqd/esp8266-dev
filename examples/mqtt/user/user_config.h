#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

#define CFG_HOLDER    0x00FF55A0  /* Change this value to load default configurations */
#define CFG_LOCATION  0x3C  /* Please don't change or if you know what you doing */
#define CLIENT_SSL_ENABLE

/*DEFAULT CONFIGURATIONS*/

#define MQTT_HOST       "ubisen.com"
#define MQTT_PORT       1883
#define MQTT_BUF_SIZE   1024
#define MQTT_KEEPALIVE  120  /*second*/

#define MQTT_CLIENT_ID  "11717002041464"
#define MQTT_USER       MQTT_CLIENT_ID
#define MQTT_PASS       "d5c6c92ee49d4faeadf269d806f2f447652536d9439c8ab3ed9ed27d881856d14e322ddd52c6559a8c0f9fed6327c3d498e4580b1594a356f7fc5c14d18c582ed59d20293c1f9bb2b02f7ba224508def"
#define MQTT_TOPIC_DO   "11717002041464/DO/data"
#define MQTT_TOPIC_PH   "11717002041464/PH/data"

#define STA_SSID "FIT_LAB"
#define STA_PASS "fit@dhcn"
#define STA_TYPE AUTH_WPA2_PSK

#define MQTT_RECONNECT_TIMEOUT  5 /*second*/

#define DEFAULT_SECURITY        0
#define QUEUE_BUFFER_SIZE       2048

#define PROTOCOL_NAMEv31  /*MQTT version 3.1 compatible with Mosquitto v0.15*/
//PROTOCOL_NAMEv311     /*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/
#endif
