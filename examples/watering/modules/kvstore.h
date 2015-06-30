
#define KVS_ENTRY_SIZE 128
#define KVS_MAX_ENTRIES ((SPI_FLASH_SEC_SIZE/KVS_ENTRY_SIZE) - 1)
#define KVS_MAX_KEY 16
#define KVS_MAX_VAL (KVS_ENTRY_SIZE-KVS_MAX_KEY)
#define KVS_DEFAULT_LOC 0x3C



struct flash_config_entry_tag{
	char key[KVS_MAX_KEY];
	char value[KVS_MAX_VAL];
} __attribute__((__packed__));

typedef struct flash_config_entry_tag flash_config_entry_s;

struct flash_sector_tag {
	uint8_t sig[4];
	uint8_t pad[KVS_ENTRY_SIZE - 4];
	flash_config_entry_s entries[KVS_MAX_ENTRIES];
} __attribute__((__packed__));

typedef struct flash_sector_tag flash_sector_s;

typedef struct {
	flash_sector_s *sector;
	bool dirty;
	unsigned location;
} flash_handle_s;

flash_handle_s * kvstore_open(unsigned cfg_sector_loc);
bool kvstore_exists(flash_handle_s *h, const char *key);
char *kvstore_get_string(flash_handle_s *h, const char *key);
int kvstore_get_integer(flash_handle_s *h, const char *key, int *int_var);
bool kvstore_put(flash_handle_s *h, const char *key, const char *value);
bool kvstore_flush(flash_handle_s *h);
bool kvstore_close(flash_handle_s *h);
bool kvstore_update_number(flash_handle_s *h, const char *key, int new_val);
