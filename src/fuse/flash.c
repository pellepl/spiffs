#include "flash.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int fd = -1;
static void* ptr = NULL;

#if SPIFFS_HAL_CALLBACK_EXTRA
int32_t flash_read(void* fs, uint32_t addr, uint32_t size, uint8_t *dst)
#else
int32_t flash_read(uint32_t addr, uint32_t size, uint8_t *dst)
#endif
{
	memcpy(dst, ptr + addr, size);

	return 0;
}

#if SPIFFS_HAL_CALLBACK_EXTRA
int32_t flash_write(void* fs, uint32_t addr, uint32_t size, uint8_t *src)
#else
int32_t flash_write(uint32_t addr, uint32_t size, uint8_t *src)
#endif
{
	memcpy(ptr + addr, src, size);

	return 0;
}
#if SPIFFS_HAL_CALLBACK_EXTRA
int32_t flash_erase(void* fs, uint32_t addr, uint32_t size)
#else
int32_t flash_erase(uint32_t addr, uint32_t size)
#endif
{
	memset(ptr + addr, 0xFF, size);

	return 0;
}

static uint32_t flash_size = 0;
uint32_t flash_get_size(void) {
	if (!flash_size)
		flash_size = lseek(fd, 0, SEEK_END);
	return flash_size;
}

int flash_open(const char* file) {
	fd = open(file, O_RDWR);
	ptr = mmap(NULL, flash_get_size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	printf("%s(%s) = %d bytes\n", __func__, file, flash_get_size());

	if (fd >= 0)
		return 0;
	return -1;
}

int flash_create(const char* file, size_t size) {
	fd = open(file, O_RDWR | O_CREAT, 0644);

	if (fd < 0)
		return -1;

	printf("%s(%s, %zd)\n", __func__, file, size);

	void* mem = malloc(size);
	memset(mem, 0xFF, size);
	if (write(fd, mem, size) < 0) {
		size = 0;
	}

	free(mem);
	flash_size = size;

	ptr = mmap(NULL, flash_get_size(), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	return 0;
}

void flash_exit(void) {
	if (fd >= 0) {
		munmap(ptr, flash_get_size());
		close(fd);
	}
	fd = -1;
	flash_size = 0;
}
