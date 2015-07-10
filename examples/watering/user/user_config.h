#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

#define CFG_HOLDER    0x00FF55A0  /* Change this value to load default configurations */
#define CFG_LOCATION  0x3C  /* Please don't change or if you know what you doing */
#define CLIENT_SSL_ENABLE
// #define SSL_ENABLED_VAR


#define BUTTON1_GPIO 12				// GPIO to use for button 1
#define BUTTON2_GPIO 0				// GPIO to use for button 2
#define BUTTON3_GPIO 5				// GPIO to use for button 3
#define BUTTON4_GPIO 4				// GPIO to use for button 4

#define VALVE1_GPIO 13				// GPIO to use for valve 1
#define VALVE2_GPIO 16				// GPIO to use for valve 2
#define VALVE3_GPIO 3				// GPIO to use for valve 3
#define VALVE4_GPIO 1				// GPIO to use for valve 4
#define VALVE_NUMBER 4

#define ON 1
#define OFF 0

#define MAX_INFO_ELEMENTS 16			// Patcher number of elements
#define INFO_BLOCK_MAGIC 0x3F2A6C17		// Patcher magic
#define INFO_BLOCK_SIG "ESP8266HWSTARSR"// Patcher pattern
#define CONFIG_FLD_REQD 0x01			// Patcher field required flag


// I2C Config
#define I2C_MASTER_PIN
#define I2C_MASTER_SDA_MUX PERIPHS_IO_MUX_MTDI_U
#define I2C_MASTER_SCL_MUX PERIPHS_IO_MUX_MTMS_U
#define I2C_MASTER_SDA_GPIO 2
#define I2C_MASTER_SCL_GPIO 14
#define I2C_MASTER_SDA_FUNC FUNC_GPIO12
#define I2C_MASTER_SCL_FUNC FUNC_GPIO14


/*DEFAULT CONFIGURATIONS*/

#define MQTT_HOST       "88.163.66.219"
#define MQTT_PORT       1883
#define MQTT_BUF_SIZE   1024
#define MQTT_KEEPALIVE  120  /*second*/

#define MQTT_CLIENT_ID  "WateringESP"
#define MQTT_USER       MQTT_CLIENT_ID
#define MQTT_PASS       ""
#define MQTT_DEV_PATH	"/node/watering"

#define MQTT_RECONNECT_TIMEOUT  5 /*second*/

#define DEFAULT_SECURITY        0
#define QUEUE_BUFFER_SIZE       2048

#define PROTOCOL_NAMEv31  /*MQTT version 3.1 compatible with Mosquitto v0.15*/
//PROTOCOL_NAMEv311     /*MQTT version 3.11 compatible with https://eclipse.org/paho/clients/testing/*/

// FOTA params
#define UPDATE_SERVER_IP    "localhost"
#define UPDATE_SERVER_PORT  3000

#define OTA_UUID          "557d2bfc96ce74ff3e78469f"
#define OTA_TOKEN         "cb21211edf824fdb64f7199df5aa104fdd2aa00747256221d40bb82058797592"
#define OTA_CLIENT        "ESP"
#define OTA_VERSION       VERSION

#define FOTA_SECURE       0

#define INTERVAL          (30*1000)      // milisecond

#endif
