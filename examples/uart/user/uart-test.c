#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"
#include "mem.h"

#include "driver/uart.h"
#include "cmd.h"

os_event_t *cmd_recvTaskQueue;
extern UartDevice    UartDev;

static void cmd_recvTask(os_event_t *events);
/**
  * @brief  Uart receive task.
  * @param  events: contain the uart receive data
  * @retval None
  */
static void ICACHE_FLASH_ATTR
cmd_recvTask(os_event_t *events) 
{
  if (events->sig == SIG_UART0_RX) {
    os_printf("%s\n", UartDev.rcv_buff.pRcvMsgBuff);
  }
}

//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
  // Initialize the GPIO subsystem.
  // gpio_init();
  uart_init(BIT_RATE_115200, BIT_RATE_115200);
  // reset write pos
  UartDev.rcv_buff.pWritePos = UartDev.rcv_buff.pRcvMsgBuff ;

  os_delay_us(1000000);
  os_printf("\n");
  os_printf("compile time: %s %s\n", __DATE__, __TIME__);
  system_print_meminfo();
  os_printf("boot version: %d\n", system_get_boot_version());
  os_printf("cpu frequency: %d Mhz\n", system_get_cpu_freq());
  os_printf("flash id: %d\n", spi_flash_get_id());

  
  cmd_recvTaskQueue = (os_event_t *)os_malloc(sizeof(os_event_t)*CMD_RECVTASKQUEUELEN);
  system_os_task(cmd_recvTask, CMD_RECVTASKPRIO, cmd_recvTaskQueue, CMD_RECVTASKQUEUELEN);
}
