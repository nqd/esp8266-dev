#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"

#include "driver/uart.h"
#include "driver/i2c_master.h"

#define DEBUG(...) os_printf(__VA_ARGS__)
#define INFO(...) os_printf(__VA_ARGS__)

bool ICACHE_FLASH_ATTR
mcp3221_read(uint8 addr, uint16 *pData)
{
  uint8 ack;

  i2c_master_start();
  i2c_master_writeByte(addr);

  // reading ack, should be 0
  ack = i2c_master_getAck();
  if (ack != 0) {
    DEBUG("addr not ack when tx write cmd, line %d\n", __LINE__);
    i2c_master_stop();
    return false;
  }

  // reading high byte, first 4 bit should be zero
  uint8 temp = i2c_master_readByte();
  if ((temp | 0x0f) != 0x0f) {
    DEBUG("wrong reading\n");
    return false;
  }
  *pData = temp*256;

  // return ack 0 for high byte
  i2c_master_setAck(0);

  // reading low byte
  temp = i2c_master_readByte();
  *pData += temp;

  // return ack 1 for lower byte
  i2c_master_setAck(1);

  // stop, we dont need continuous reading
  i2c_master_stop();

  return true;
}

void ICACHE_FLASH_ATTR
mcp3221_init(void)
{
  i2c_master_gpio_init();
}