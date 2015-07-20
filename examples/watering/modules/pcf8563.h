#ifndef _PCF8563_H_
#define _PCF8563_H_

#include <time.h>

#define PCF8563_ADDR 0x51
#define PCF8563_ADDR_TIME  0x02


#define PCF8563_SET     0
#define PCF8563_CLEAR   1
#define PCF8563_REPLACE 2


bool pcf8563_setTime(struct tm *time);
bool pcf8563_getTime(struct tm *time);
void pcf8563_init(void);

#endif