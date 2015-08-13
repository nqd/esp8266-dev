## Why this?
Short: I do not like the developing environment provided by Espressif. Then I rewrite Makefile (mostly) to:
- avoid duplicate codes by reuse common modules, e.g. MQTT, flash file system, and drivers for SoC peripherals.
- make firmware over-the-air update easy
- quick setup with Makefile in each program

This dev env will use as much (closed blobs) libs from Espressif as possible. However, current json parsing of SDK (taken from Contiki OS) is not good enough, I have no way to check if this/that string is a valid one, then I decided to use [jsmn](https://bitbucket.org/zserge/jsmn)

All examples have been testing with SDK 1.2.0, 4MB Module-12. 512KB Module-01 is considered out of date.

## Begin
1. Install compiler for ESP with [esp-open-sdk](https://github.com/pfalcon/esp-open-sdk). It would be good if you install at HOME.
2. Clone this project
```
git clone --recursive https://github.com/nqd/esp8266-dev.git
```

3. Try OTA example under ```examples/ota-update```

I setup an OTA server at http://103.253.146.183/, you may need to
- register new account
- then copy ```apiKey, _id```,  at ```Profile``` for ```OTA_UUID, OTA_TOKEN``` at ```user_config.h```
- update ```APIKEY``` at ```tools/fotaclient/fota-client.js```. You will need node.js to register new firmware
- from server, create new application, named ```otaupdate``` (check Makefile for other names)
- ```make``` to compile the example. Then ```make flash``` to flash the fist firmware. By default, this new firmware labeled with version 0.0.1.
- do something with the code, or just leave it alone for few seconds.
- ready for new version? edit ```.version```, e.g. 0.0.2 for this new cool update. ``` make clean && make && make IMAGE=2 && make register``` to label your image with new version, and then upload to OTA server

Via UART logging, you should see new version 0.0.2 downloaded, then ESP boots new firmware, print out your last compiling time.

## To use
By default, flash size is 4096 KB (1024+1024 with OTA=1). Pull request for Makefile to work with all flash options is more than welcome.

Command to use:
```
make OTA=1/0                # default OTA=1
make OTA=1 IMAGE=1/2        # generate image user1/user2
make OTA=1 IMAGE=1/2 flash  # program with OTA enable
make register               # upload, register new version. Must setup OTA server first
```

## How to write makefile for your own program:

Look at Makefile for program under ```/examples```:
```
PROJECT = yourprojectname
ROOT = ../..

APPS += fota
APPS += mqtt

include ${ROOT}/Makefile.common
```

## Using APPs
All credit of apps/ belong to their authors. Mis config and/or using belong to me.

+ mqtt https://github.com/tuanpmt/esp_mqtt
+ json parsing https://bitbucket.org/zserge/jsmn
+ flash file system https://github.com/pellepl/spiffs

## Todo
+ makefile with all options of flash
+ flash file system testing

## License
MIT
