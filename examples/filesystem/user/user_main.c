#include "driver/uart.h"
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "user_config.h"
#include "user_interface.h"
#include "params_test.h"

#include "spiffs.h"

static spiffs fs;

static u8_t spiffs_work_buf[LOG_PAGE*2];
static u8_t spiffs_fds[FD_BUF_SIZE*2];
static u8_t spiffs_cache_buf[CACHE_BUF_SIZE];


static s32_t ICACHE_FLASH_ATTR
my_spiffs_read(u32_t addr, u32_t size, u8_t *dst) {
  int res = spi_flash_read(addr, (u32_t*)&(*dst), size);
  // os_printf("read res = %d\n", res);
  return SPIFFS_OK;
}

static s32_t ICACHE_FLASH_ATTR
my_spiffs_write(u32_t addr, u32_t size, u8_t *src) {
  os_printf("spiffs write [%s] [%d] bytes\n", src, size);
  int res = spi_flash_write(addr, (u32_t*)&(*src), size);
  os_printf("write res = %d\n", res);
  return SPIFFS_OK;
}

static s32_t ICACHE_FLASH_ATTR
my_spiffs_erase(u32_t addr, u32_t size) {
  int i;
  char ch = 0xff;
  for (i=0; i< size; i++)
    spi_flash_write(addr, (uint32 *)&ch, 1);

  return SPIFFS_OK;
} 

void ICACHE_FLASH_ATTR
test_spiffs() {
  spiffs_config cfg;
  cfg.phys_size = PHYS_FLASH_SIZE; // use all spi flash
  cfg.phys_addr = SPIFFS_FLASH_SIZE; // start spiffs at start of spi flash
  cfg.phys_erase_block = SECTOR_SIZE; // according to datasheet
  cfg.log_block_size = LOG_BLOCK; // let us not complicate things
  cfg.log_page_size = LOG_PAGE; // as we said
  
  cfg.hal_read_f = my_spiffs_read;
  cfg.hal_write_f = my_spiffs_write;
  cfg.hal_erase_f = my_spiffs_erase;
  
  int res = SPIFFS_mount(&fs,
    &cfg,
    spiffs_work_buf,
    spiffs_fds,
    sizeof(spiffs_fds),
    spiffs_cache_buf,
    sizeof(spiffs_cache_buf),
    0);
  os_printf("mount res: %d\n", res);

  char buf[12];
  // Surely, I've mounted spiffs before entering here
  spiffs_file fd = SPIFFS_open(&fs, "my_file", SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_RDWR, 0);
  os_printf("fd = %d\n", fd);
  if (SPIFFS_write(&fs, fd, (u8_t *)"Hello world", 12) < 0) {
    os_printf("errno %d\n", SPIFFS_errno(&fs));
    return;
  }
  SPIFFS_close(&fs, fd); 

  fd = SPIFFS_open(&fs, "my_file", SPIFFS_RDWR, 0);
  if (SPIFFS_read(&fs, fd, (u8_t *)buf, 12) < 0) {
    os_printf("errno %d\n", SPIFFS_errno(&fs));
    return;
  }
  SPIFFS_close(&fs, fd);

  os_printf("--> %s <--\n", buf);
}

//Init function 
void ICACHE_FLASH_ATTR
user_init()
{
    // Initialize the GPIO subsystem.
    uart_init(BIT_RATE_115200, BIT_RATE_115200);

    test_spiffs();
}
