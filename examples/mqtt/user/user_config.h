#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

/*DEFAULT CONFIGURATIONS*/

#define MQTT_HOST       "192.168.1.212" 			// "test.mosquitto.org"
#define MQTT_PORT       1883 	//
#define MQTT_KEEPALIVE  120  /*second*/

#define MQTT_CLIENT_ID  "id"
#define MQTT_USER       MQTT_CLIENT_ID
#define MQTT_PASS       "pass"
#define MQTT_TOPIC   		"esp8266dev"

#define STA_SSID "FIT_LAB"
#define STA_PASS "fit@dhcn"
#define STA_TYPE AUTH_WPA2_PSK

#define DEFAULT_SECURITY        1
#define QUEUE_BUFFER_SIZE       2048

//#define PROTOCOL_NAMEv31  /*MQTT version 3.1 compatible with Mosquitto v0.15*/
#define PROTOCOL_NAMEv311     /*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/
#endif
