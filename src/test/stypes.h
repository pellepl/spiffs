/*
 * stypes.h
 *
 *  Created on: May 26, 2013
 *      Author: petera
 */

#ifndef STYPES_H_
#define STYPES_H_

#define print(...) printf(__VA_ARGS__)

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


#endif /* STYPES_H_ */
