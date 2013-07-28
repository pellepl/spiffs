/*
 * params_test.h
 *
 *  Created on: May 26, 2013
 *      Author: petera
 */

#ifndef PARAMS_TEST_H_
#define PARAMS_TEST_H_

#define FLASH_SIZE    2*1024*1024
#define SECTOR_SIZE   65536
#define LOG_BLOCK     65536
#define LOG_PAGE      256

#define ASSERT(c, m) real_assert((c),(m), __FILE__, __LINE__);

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

void real_assert(int c, const char *n, const char *file, int l);

typedef signed int s32_t;
typedef unsigned int u32_t;
typedef signed short s16_t;
typedef unsigned short u16_t;
typedef signed char s8_t;
typedef unsigned char u8_t;


#endif /* PARAMS_TEST_H_ */
