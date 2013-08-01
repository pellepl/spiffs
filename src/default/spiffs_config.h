/*
 * spiffs_config.h
 *
 *  Created on: Jul 3, 2013
 *      Author: petera
 */

#ifndef SPIFFS_CONFIG_H_
#define SPIFFS_CONFIG_H_

// ----------- 8< ------------
// Following includes are for the linux test build of spiffs
// These may/should/must be removed/altered/replaced in your target
#include "params_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
// ----------- >8 ------------

// compile time switches

// Set generic spiffs debug output call.
#define SPIFFS_DBG(...)
// Set spiffs debug output call for garbage collecting.
#define SPIFFS_GC_DBG(...)
// Set spiffs debug output call for caching.
#define SPIFFS_CACHE_DBG(...)
// Set spiffs debug output call for system consistency checks.
#define SPIFFS_CHECK_DBG(...)

// Enable/disable API functions to determine exact number of bytes
// for filedescriptor and cache buffers. Once decided for a configuration,
// this can be disabled to reduce flash.
#define SPIFFS_BUFFER_HELP              0

// Enables/disable memory read caching of nucleus file system operations.
// If enabled, memory area must be provided for cache in SPIFFS_mount.
#define SPIFFS_CACHE                    1
#if SPIFFS_CACHE
// Enables memory write caching for file descriptors in hydrogen
#define SPIFFS_CACHE_WR                 1
// Enable/disable statistics on caching. Debug/test purpose only.
#define SPIFFS_CACHE_STATS              1
#endif

// Always check header of each accessed page to ensure consistent state.
// If enabled it will increase number of reads, will increase flash.
#define SPIFFS_PAGE_CHECK               1

// Define maximum number of gc runs to perform to reach desired free pages.
#define SPIFFS_GC_MAX_RUNS              3

// Enable/disable statistics on gc. Debug/test purpose only.
#define SPIFFS_GC_STATS                 1

// Garbage collecting examines all pages in a block which and sums up
// to a block score. Deleted pages normally gives positive score and
// used pages normally gives a negative score (as these must be moved).
// To have a fair wear-leveling, the erase age is also included in score,
// whose factor normally is the most positive.
// The larger the score, the more likely it is that the block will
// picked for garbage collection.

// Farbage collecting heuristics - weight used for deleted pages.
#define SPIFFS_GC_HEUR_W_DELET          (10)
// Farbage collecting heuristics - weight used for used pages.
#define SPIFFS_GC_HEUR_W_USED           (-1)
// Farbage collecting heuristics - weight used for time between
// last erased and erase of this block.
#define SPIFFS_GC_HEUR_W_ERASE_AGE      (30)

// Object name maximum length.
#define SPIFFS_OBJ_NAME_LEN             (32)

// Size of buffer allocated on stack used when copying data.
// Lower value generates more read/writes. No meaning having it bigger
// than logical page size.
#define SPIFFS_COPY_BUFFER_STACK        (64)

// SPIFFS_LOCK and SPIFFS_UNLOCK protects spiffs from reentrancy on api level
// These should be defined on a multithreaded system

// define this to entering a mutex if you're running on a multithreaded system
#define SPIFFS_LOCK(fs)
// define this to exiting a mutex if you're running on a multithreaded system
#define SPIFFS_UNLOCK(fs)


// Enable if only one spiffs instance with constant configuration will exist
// on the target. This will reduce calculations, flash and memory accesses.
// Parts of configuration must be defined below instead of at time of mount.
#define SPIFFS_SINGLETON 0

#if SPIFFS_SINGLETON
// Instead of giving parameters in config struct, singleton build must
// give parameters in defines below.
#define SPIFFS_CFG_PHYS_SZ(ignore)        (1024*1024*2)
#define SPIFFS_CFG_PHYS_ERASE_SZ(ignore)  (65536)
#define SPIFFS_CFG_PHYS_ADDR(ignore)      (0)
#define SPIFFS_CFG_LOG_PAGE_SZ(ignore)    (256)
#define SPIFFS_CFG_LOG_BLOCK_SZ(ignore)   (65536)
#endif

// Set SPFIFS_TEST_VISUALISATION to non-zero to enable SPIFFS_vis function
// in the api. This function will visualize all filesystem using given printf
// function.
#define SPIFFS_TEST_VISUALISATION         1
#if SPIFFS_TEST_VISUALISATION
#define spiffs_printf(...)                printf(__VA_ARGS__)
// spiffs_printf argument for a free page
#define SPIFFS_TEST_VIS_FREE_STR          "_"
// spiffs_printf argument for a deleted page
#define SPIFFS_TEST_VIS_DELE_STR          "/"
// spiffs_printf argument for an index page for given object id
#define SPIFFS_TEST_VIS_INDX_STR(id)      "i"
// spiffs_printf argument for a data page for given object id
#define SPIFFS_TEST_VIS_DATA_STR(id)      "d"
#endif

// Types depending on configuration such as the amount of flash bytes
// given to spiffs file system in total (spiffs_file_system_size),
// the logical block size (log_block_size), and the logical page size
// (log_page_size)

// Block index type. Make sure the size of this type can hold
// the highest number of all blocks - i.e. spiffs_file_system_size / log_block_size
typedef u8_t spiffs_block_ix;
// Page index type. Make sure the size of this type can hold
// the highest page number of all pages - i.e. spiffs_file_system_size / log_page_size
typedef u16_t spiffs_page_ix;
// Object id type - most significant bit is reserved for index flag. Make sure the
// size of this type can hold the highest object id on a full system,
// i.e. 2 + (spiffs_file_system_size / (2*log_page_size))*2
typedef u16_t spiffs_obj_id;
// Object span index type. Make sure the size of this type can
// hold the largest possible span index on the system -
// i.e. (spiffs_file_system_size / log_page_size) - 1
typedef u16_t spiffs_span_ix;

#endif /* SPIFFS_CONFIG_H_ */
