/*
 * test_spiffs.h
 *
 *  Created on: Jun 19, 2013
 *      Author: petera
 */

#ifndef TEST_SPIFFS_H_
#define TEST_SPIFFS_H_

#include "spiffs.h"

#define FS &__fs

extern spiffs __fs;

void fs_reset();
int read_and_verify(char *name);
void dump_page(spiffs *fs, spiffs_page_ix p);
void hexdump(u32_t addr, u32_t len);
char *make_test_fname(const char *name);
void clear_test_path();
void area_write(u32_t addr, u8_t *buf, u32_t size);
void area_read(u32_t addr, u8_t *buf, u32_t size);


#endif /* TEST_SPIFFS_H_ */
