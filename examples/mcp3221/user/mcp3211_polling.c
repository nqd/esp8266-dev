#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "user_config.h"
#include "user_interface.h"

#include "driver/uart.h"
#include "driver/i2c_master.h"

static ETSTimer report_timer;

/******************************************************************************
*******************************************************************************/
LOCAL bool ICACHE_FLASH_ATTR
i2c_request(uint8 addr)
{
  uint8 ack;
  uint16 i;

  i2c_master_start();
  i2c_master_writeByte(addr);
  ack = i2c_master_getAck();

  if (ack) {
    // os_printf("addr not ack when tx write cmd, line %d\n", __LINE__);
    i2c_master_stop();
    return false;
  }

  i2c_master_stop();
  i2c_master_wait(40000);

  i2c_master_start();
  i2c_master_writeByte(addr + 1);
  ack = i2c_master_getAck();

  if (ack) {
    // os_printf("addr not ack when tx write cmd, line %d\n", __LINE__);
    i2c_master_stop();
    return false;
  }

  uint8 len = 2;        // for mcp3211, we read 2 bytes
  for (i = 0; i < len; i++) {
      uint8 temp = i2c_master_readByte();
      i2c_master_setAck((i == (len - 1)) ? 1 : 0);
  }
  i2c_master_stop();

  return true;
}

void ICACHE_FLASH_ATTR
i2c_polling()
{
  static uint8 addr = 0x00;
  if (i2c_request(addr) == true) {
    os_printf("%x success\n", addr);
  }
  else {
    // os_printf("%x failed\n", addr);
  }
  addr++;
  if (addr == 0) {
    os_printf("... done\n");
    os_timer_disarm(&report_timer);
  }
}

//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
  // UART setup
  uart_init(BIT_RATE_115200, BIT_RATE_115200);
  os_delay_us(1000000);

  i2c_master_gpio_init();

  os_printf("begin polling ... \n");

  os_timer_disarm(&report_timer);
  os_timer_setfn(&report_timer, (os_timer_func_t *)i2c_polling, NULL);
  os_timer_arm(&report_timer, 100*3, 1);
}
  