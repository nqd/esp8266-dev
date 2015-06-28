#ifndef _MQTT_CONFIG_H_
#define _MQTT_CONFIG_H_

#define MQTT_BUF_SIZE               1024
#define MQTT_RECONNECT_TIMEOUT      5 /*second*/

#ifndef MQTT_SSL_SIZE
#define MQTT_SSL_SIZE					4096
#endif

#define PROTOCOL_NAMEv31  /*MQTT version 3.1 compatible with Mosquitto v0.15*/
//PROTOCOL_NAMEv311     /*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/

#endif