#include <stddef.h>
#include <stdint.h>

#include "spiffs.h"

#define FLASH_PAGE_SIZE	256
#define BLOCK_4K	(1 << 12)
#define BLOCK_32K	(1 << 15)
#define BLOCK_64K	(1 << 16)

#if SPIFFS_HAL_CALLBACK_EXTRA
int32_t flash_read(void* fs, uint32_t addr, uint32_t size, uint8_t *dst);
int32_t flash_write(void* fs, uint32_t addr, uint32_t size, uint8_t *src);
int32_t flash_erase(void* fs, uint32_t addr, uint32_t size);
#else
int32_t flash_read(uint32_t addr, uint32_t size, uint8_t *dst);
int32_t flash_write(uint32_t addr, uint32_t size, uint8_t *src);
int32_t flash_erase(uint32_t addr, uint32_t size);
#endif

uint32_t flash_get_size(void);

int flash_open(const char *file);
int flash_create(const char* file, size_t size);
void flash_exit(void);
