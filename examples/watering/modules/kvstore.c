#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "debug.h"
#include "user_interface.h"
#include "util.h"
#include "kvstore.h"


static uint8_t sig[4] = {0x55,0x00,0xFF,0xAA};

/*
 * Return the index of an entry in the sector
 * Returns MAX_ENTRIES if the key is not found
 */ 


LOCAL unsigned ICACHE_FLASH_ATTR 
kvstoreIndex(flash_handle_s *h, const char *key)
{
	int i;
	if(!h){
		INFO("Bad kvs handle for key %s!\r\n", key);
		return 0;
	}
	for(i = 0; i < KVS_MAX_ENTRIES; i++){
		if(!strcmp(h->sector->entries[i].key, key))
			break;
	}
	return i;
}

/*
 * Get a key's value from the store
 */

LOCAL char * ICACHE_FLASH_ATTR 
kvstoreGet(flash_handle_s *h, const char *key)
{
	unsigned idx = kvstoreIndex(h, key);	
	
	// If not in store, return 0
	if(KVS_MAX_ENTRIES == idx){
		//INFO("KVS get no such key: %s\r\n", key); 
		return NULL;
	}
	return h->sector->entries[idx].value;
}


/*
 * Read the contents of the flash sector into a buffer
 */
 
flash_handle_s * 
ICACHE_FLASH_ATTR kvstore_open(unsigned cfg_sector_loc)
{
	
	
	// Allocate the memory for the handle struct
	flash_handle_s *h = (flash_handle_s *) os_zalloc(SPI_FLASH_SEC_SIZE);
	
	// Allocate the memory to hold the sector
	h->sector = (flash_sector_s *) os_zalloc(SPI_FLASH_SEC_SIZE);
	
	// Note location for future reference
	h->location = cfg_sector_loc;

	// Read it in
	spi_flash_read(cfg_sector_loc * SPI_FLASH_SEC_SIZE,
	(uint32 *) h->sector, SPI_FLASH_SEC_SIZE);
	// If the signature doesn't match we will need to init the sector
	if(os_memcmp(sig, h->sector->sig, sizeof(sig))){
		INFO("Flash configuration area not initialized\r\n");
		os_memset(h->sector, 0, SPI_FLASH_SEC_SIZE);
		os_memcpy(h->sector->sig, sig, sizeof(sig));
		h->dirty = TRUE;
	
	}	
	return h;
		
}

/*
* Search the sector for an existing key
*/

bool ICACHE_FLASH_ATTR 
kvstore_exists(flash_handle_s *h, const char *key)
{
	unsigned i = kvstoreIndex(h, key);
	if(KVS_MAX_ENTRIES == i)
		return FALSE;
	else
		return TRUE;
}


/*
 * Get a key's value from the store, allocate memory and copy it there.
 * Returned string must be freed after it is no longer needed.
 */

char * ICACHE_FLASH_ATTR 
kvstore_get_string(flash_handle_s *h, const char *key)
{
	char *v;
	
	v = kvstoreGet(h, key);
	
	if(!v)
		return NULL;
		
	return util_strdup(v);
}

/*
 * Return the integer stored by a key
 */

int ICACHE_FLASH_ATTR
kvstore_get_integer(flash_handle_s *h, const char *key, int *int_var)
{
	char *dup, *v;
	
	v = kvstoreGet(h, key);
	INFO("KVS Value: %s\r\n", v);
	
	if(!v){
		return FALSE;
	}
	*int_var = atoi(v);
	return TRUE;	
}



/*
 * Put a key/value in the store. Return nonzero if success
 */
 

bool ICACHE_FLASH_ATTR 
kvstore_put(flash_handle_s *h, const char *key, const char *value)
{
	unsigned idx = kvstoreIndex(h, key);	
	
	// If not in store, return 0
	if(KVS_MAX_ENTRIES == idx){
		// Find an unused entry
		for(idx = 0; idx < KVS_MAX_ENTRIES; idx++){
			//os_printf("IDX: %d First byte: %02x\r\n", idx, h->sector->entries[idx].key[0]);
			if(!h->sector->entries[idx].key[0])
				break;
			}
		if(KVS_MAX_ENTRIES == idx){
			// Error: No room left
			INFO("KVS full!\r\n");
			return FALSE;
		}
	}
	/* Store the new key */
	os_strncpy(h->sector->entries[idx].key, key, KVS_MAX_KEY - 1);
	h->sector->entries[idx].key[KVS_MAX_KEY - 1] = 0;
	
	/* Store the new value */
	os_strncpy(h->sector->entries[idx].value, value, KVS_MAX_VAL - 1);
	h->sector->entries[idx].key[KVS_MAX_VAL - 1] = 0;
	
	//INFO("Value supplied: %s, Stored value: %s\r\n", value, h->sector->entries[idx].value);
	
	/* Set the dirty flag */
	h->dirty = TRUE;
	
	return TRUE;
}

/*
 * Flush the sector back to flash
 */
 
bool ICACHE_FLASH_ATTR 
kvstore_flush(flash_handle_s *h)
{
	if(!h){
		INFO("Bad KVS handle");
		return FALSE;
	}
	if(h->dirty){
			INFO("Flushing sector to flash\r\n");
			spi_flash_erase_sector(h->location);
			spi_flash_write(h->location * SPI_FLASH_SEC_SIZE,
				(uint32 *) h->sector, SPI_FLASH_SEC_SIZE);
			h->dirty = FALSE;
	}
	return TRUE;
}

/*
 * Flush and free memory used to buffer sector
 */

bool ICACHE_FLASH_ATTR kvstore_close(flash_handle_s *h)
{
	if(!kvstore_flush(h))
		return FALSE;
	
	os_free(h->sector);
	os_free(h);
	return TRUE;
}

/*
 * Update or add a config key value
 */

bool ICACHE_FLASH_ATTR kvstore_update_number(flash_handle_s *h, const char *key, int new_val)
{
	int res;
	char *valbuf = util_zalloc(16);
	
	if(!h){
		util_free(valbuf);
		return FALSE;
	}
		
	os_sprintf(valbuf, "%d", new_val);
	
	res = kvstore_put(h, key, valbuf);	
	
	kvstore_flush(h);
	
	util_free(valbuf);
	
	return res;
}


