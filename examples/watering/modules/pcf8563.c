#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "pcf8563.h"

#include "i2c_master.h"

#define DEBUG(...) os_printf(__VA_ARGS__)
#define INFO(...) os_printf(__VA_ARGS__)


// convert normal decimal to binary coded decimal
uint8_t ICACHE_FLASH_ATTR decToBcd(uint8_t dec) {
  return(((dec / 10) * 16) + (dec % 10));
}

// convert binary coded decimal to normal decimal
uint8_t ICACHE_FLASH_ATTR bcdToDec(uint8_t bcd) {
  return(((bcd / 16) * 10) + (bcd % 16));
}


// send a number of bytes to the rtc over i2c
// returns true to indicate success
bool ICACHE_FLASH_ATTR pcf8563_send(uint8_t *data, uint8_t len) {

  int loop;

  // signal i2c start
  i2c_master_start();

  // write address & direction
  i2c_master_writeByte((uint8_t)(PCF8563_ADDR << 1));
  if (!i2c_master_checkAck()) {
    i2c_master_stop();
    return false;
  }

  // send the data
  for (loop = 0; loop < len; loop++) {
    i2c_master_writeByte(data[loop]);
    if (!i2c_master_checkAck()) {
      i2c_master_stop();
      return false;
    }
  }

  // signal i2c stop
  i2c_master_stop();

  return true;

}

// read a number of bytes from the rtc over i2c
// returns true to indicate success
bool ICACHE_FLASH_ATTR pcf8563_recv(uint8_t *data, uint8_t len) {
  
  int loop;

  // signal i2c start
  i2c_master_start();

  // write address & direction
  i2c_master_writeByte((uint8_t)((PCF8563_ADDR << 1) | 1));
  if (!i2c_master_checkAck()) {
    i2c_master_stop();
    return false;
  }

  // read bytes
  for (loop = 0; loop < len; loop++) {
    data[loop] = i2c_master_readByte();
    // send ack (except after last byte, then we send nack)
    if (loop < (len - 1)) i2c_master_send_ack(); else i2c_master_send_nack();
  }

  // signal i2c stop
  i2c_master_stop();

  return true;

}

// set the time on the rtc
// timezone agnostic, pass whatever you like
// I suggest using GMT and applying timezone and DST when read back
// returns true to indicate success
bool ICACHE_FLASH_ATTR pcf8563_setTime(struct tm *time) {
  
  uint8 data[8];

  // start register
  data[0] = PCF8563_ADDR_TIME;
  // time/date data
  data[1] = decToBcd(time->tm_sec);
  data[2] = decToBcd(time->tm_min);
  data[3] = decToBcd(time->tm_hour);
  data[4] = decToBcd(time->tm_mday);
  data[5] = decToBcd(time->tm_wday);
  data[6] = decToBcd(time->tm_mon);
  data[7] = decToBcd(time->tm_year);

  return pcf8563_send(data, 8);

}

// get the time from the rtc, populates a supplied tm struct
// returns true to indicate success
bool ICACHE_FLASH_ATTR pcf8563_getTime(struct tm *time) {
  uint8_t data[7];

  // start register address
  data[0] = PCF8563_ADDR_TIME;
  if (!pcf8563_send(data, 1)) {
    return false;
  }

  // read time
  if (!pcf8563_recv(data, 7)) {
    return false;
  }

  // convert to unix time structure
  time->tm_sec    = bcdToDec(data[0] & 0x7f);
  time->tm_min    = bcdToDec(data[1] & 0x7f);
  time->tm_hour   = bcdToDec(data[2] & 0x3f); 
  time->tm_mday   = bcdToDec(data[3] & 0x3f);
  time->tm_wday   = bcdToDec(data[4] & 0x7);  
  time->tm_mon    = bcdToDec(data[5] & 0x1f);
  time->tm_year   = bcdToDec(data[6]);
  time->tm_isdst  = 0;

  // apply a time zone (if you are not using localtime on the rtc or you want to check/apply DST)
  //applyTZ(time);

  return true;
  
}


void ICACHE_FLASH_ATTR pcf8563_init(void) {
  i2c_master_gpio_init();
}