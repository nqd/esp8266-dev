#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "user_config.h"
#include "user_interface.h"

#include "driver/uart.h"

#define INFO(...) os_printf(__VA_ARGS__)
#define __SET__DEEP_SLEEP__WAKEUP_NO_RF__  system_deep_sleep_set_option(4)
#define __SET__DEEP_SLEEP__WAKEUP_NORMAL__ system_deep_sleep_set_option(1)

static ETSTimer report_timer;

void ICACHE_FLASH_ATTR
read_vdcc()
{
  uint16_t vdd33 = system_get_vdd33();
  uint16_t vdd = vdd33*100/1024;
  INFO("VDD = %d.%d\n", vdd/100, vdd%100);
}

//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
  // UART setup
  uart_init(BIT_RATE_115200, BIT_RATE_115200);
  os_delay_us(2000000);     // 1 sec

  read_vdcc();
  __SET__DEEP_SLEEP__WAKEUP_NORMAL__; 
  system_deep_sleep(5000000);
}
  
