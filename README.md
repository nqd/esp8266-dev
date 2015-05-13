## Why this?
Short: I do not like the developing environment provided by Espressif. Then I rewrite Makefile to:
- reuse common code, e.g. MQTT, flash file system, and driver for SoC peripherals.
- json parsing of SDK is not good enough, I have no way to check if this/that string is a valid JSON.
- quick setup, e.g. make, make flash, make login, make OTA=1 fash.
- firmware over-the-air update easy, e.g. make RELEASE=MINOR/MINOR/PATCH. This is the on going process.

All examples have been testing with 512KB module, SDK 1.0.0.

## Setup env

Use with SDK v1.0.0, setup with esp-open-sdk. With location of esp-open-sdk, change ESPRESSIF_ROOT in Makefile.common to point to this SDK. We will use lib, include, ld codes/scripts from SDK, ah, together with compiling tools off-course.

I have testing only with 512KB module. >1MB flash will be tested and update later.

To use OTA update firmware with 512KB, change linker file ld/eagle.app.v6.new.512.app1/2.ld, section irom0_0_seg to have more spaces.

```
<  irom0_0_seg :                         org = 0x40241010, len = 0x2B000
---
>  irom0_0_seg :                         org = 0x40201010, len = 0x31000
```

I update tools/gen_appbin.py to avoid compiling error. Off-course I can change $PATH of shell to avoid touching this Python script.
```
<         cmd = 'xtensa-lx106-elf-nm -g ' + elf_file + ' > eagle.app.sym'
---
>         cmd = '~/esp-open-sdk/xtensa-lx106-elf/bin/xtensa-lx106-elf-nm -g ' + elf_file + ' > eagle.app.sym'
```

## To use

make OTA=1/0                #default OTA=1
make OTA=1 IMAGE=1/2        #generate image user1/user2
make OTA=1 IMAGE=1/2 flash // program with OTA enable


## How to write makefile for program:

Declare ROOT, this is the root of the project, from which could access apps and Makefile.common

Example of Makefile for program under ```/examples```:
```
ROOT = ../..
APPS += mqtt
include ${ROOT}/Makefile.common
```

## Using APP
Please (me), check for frequence update

+ mqtt https://github.com/tuanpmt/esp_mqtt
+ json parsing https://bitbucket.org/zserge/jsmn/
+ flash file system https://github.com/pellepl/spiffs.git

## Todo
+ more complete ota update program, api of ota-update is at docs/ota-update.md
+ flash file system testing
