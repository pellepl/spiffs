/*
 * spiffs.h
 *
 *  Created on: May 26, 2013
 *      Author: petera
 */



#ifndef SPIFFS_H_
#define SPIFFS_H_

#include "spiffs_config.h"

#define SPIFFS_OK                       0
#define SPIFFS_ERR_NOT_MOUNTED          -10000
#define SPIFFS_ERR_FULL                 -10001
#define SPIFFS_ERR_NOT_FOUND            -10002
#define SPIFFS_ERR_END_OF_OBJECT        -10003
#define SPIFFS_ERR_DELETED              -10004
#define SPIFFS_ERR_NOT_FINALIZED        -10005
#define SPIFFS_ERR_NOT_INDEX            -10006
#define SPIFFS_ERR_OUT_OF_FILE_DESCS    -10007
#define SPIFFS_ERR_FILE_CLOSED          -10008
#define SPIFFS_ERR_FILE_DELETED         -10009
#define SPIFFS_ERR_BAD_DESCRIPTOR       -10010
#define SPIFFS_ERR_IS_INDEX             -10011
#define SPIFFS_ERR_IS_FREE              -10012
#define SPIFFS_ERR_INDEX_SPAN_MISMATCH  -10013
#define SPIFFS_ERR_DATA_SPAN_MISMATCH   -10014
#define SPIFFS_ERR_INDEX_REF_FREE       -10015
#define SPIFFS_ERR_INDEX_REF_LU         -10016
#define SPIFFS_ERR_INDEX_REF_INVALID    -10017
#define SPIFFS_ERR_INDEX_FREE           -10018
#define SPIFFS_ERR_INDEX_LU             -10019
#define SPIFFS_ERR_INDEX_INVALID        -10020

#define SPIFFS_ERR_INTERNAL             -10050


// spi read call type
typedef s32_t (*spiffs_read)(u32_t addr, u32_t size, u8_t *dst);
// spi write call type
typedef s32_t (*spiffs_write)(u32_t addr, u32_t size, u8_t *src);
// spi erase call type
typedef s32_t (*spiffs_erase)(u32_t addr, u32_t size);


typedef enum {
  SPIFFS_CHECK_LOOKUP = 0,
  SPIFFS_CHECK_INDEX,
  SPIFFS_CHECK_PAGE
} spiffs_check_type;

typedef enum {
  SPIFFS_CHECK_PROGRESS = 0,
  SPIFFS_CHECK_ERROR,
  SPIFFS_CHECK_FIX_INDEX,
  SPIFFS_CHECK_DELETE_ORPHANED_INDEX,
  SPIFFS_CHECK_DELETE_PAGE,
  SPIFFS_CHECK_DELETE_BAD_FILE,
} spiffs_check_report;

typedef void (*spiffs_check_callback)(spiffs_check_type type, spiffs_check_report report,
    u32_t arg1, u32_t arg2);

// size of buffer on stack used when copying data
#define SPIFFS_COPY_BUFFER_STACK        (64)

#ifndef SPIFFS_DBG
#define SPIFFS_DBG(...) \
    print(__VA_ARGS__)
#endif
#ifndef SPIFFS_GC_DBG
#define SPIFFS_GC_DBG(...) printf(__VA_ARGS__)
#endif
#ifndef SPIFFS_CACHE_DBG
#define SPIFFS_CACHE_DBG(...) printf(__VA_ARGS__)
#endif
#ifndef SPIFFS_CHECK_DBG
#define SPIFFS_CHECK_DBG(...) printf(__VA_ARGS__)
#endif

#define SPIFFS_APPEND                   (1<<0)
#define SPIFFS_TRUNC                    (1<<1)
#define SPIFFS_CREAT                    (1<<2)
#define SPIFFS_RDONLY                   (1<<3)
#define SPIFFS_WRONLY                   (1<<4)
#define SPIFFS_RDWR                     (SPIFFS_RDONLY | SPIFFS_WRONLY)
#define SPIFFS_DIRECT                   (1<<5)

#define SPIFFS_SEEK_SET                 (0)
#define SPIFFS_SEEK_CUR                 (1)
#define SPIFFS_SEEK_END                 (2)

#define SPIFFS_TYPE_FILE                (1)
#define SPIFFS_TYPE_DIR                 (2)
#define SPIFFS_TYPE_HARD_LINK           (3)
#define SPIFFS_TYPE_SOFT_LINK           (4)

#ifndef SPIFFS_LOCK
#define SPIFFS_LOCK(fs)
#endif

#ifndef SPIFFS_UNLOCK
#define SPIFFS_UNLOCK(fs)
#endif

// phys structs

// spiffs spi configuration struct
typedef struct {
  // physical read function
  spiffs_read hal_read_f;
  // physical write function
  spiffs_write hal_write_f;
  // physical erase function
  spiffs_erase hal_erase_f;
#ifndef SPIFFS_SINGLETON
  // physical size of the spi flash
  u32_t phys_size;
  // physical offset in spi flash used for spiffs,
  // must be on block boundary
  u32_t phys_addr;
  // physical size when erasing a block
  u32_t phys_erase_block;

  // logical size of a block, must be on physical
  // block size boundary and must never be less than
  // a physical block
  u32_t log_block_size;
  // logical size of a page, must be at least
  // log_block_size / 8
  u32_t log_page_size;
#endif
} spiffs_config;

typedef struct {
  // file system configuration
  spiffs_config cfg;
  // number of logical blocks
  u32_t block_count;

  // cursor for free blocks, block index
  spiffs_block_ix free_cursor_block_ix;
  // cursor for free blocks, entry index
  int free_cursor_obj_lu_entry;
  // cursor when searching, block index
  spiffs_block_ix cursor_block_ix;
  // cursor when searching, entry index
  int cursor_obj_lu_entry;

  // primary work buffer, size of a logical page
  u8_t *lu_work;
  // secondary work buffer, size of a logical page
  u8_t *work;
  // file descriptor memory area
  u8_t *fd_space;
  // available file descriptors
  u32_t fd_count;

  // last error
  s32_t errno;

  // current number of free blocks
  u32_t free_blocks;
  // current number of busy pages
  u32_t stats_p_allocated;
  // current number of deleted pages
  u32_t stats_p_deleted;
  // flag indicating that garbage collector is cleaning
  u8_t cleaning;
  // max erase count amongst all blocks
  spiffs_obj_id max_erase_count;

#if SPIFFS_GC_STATS
  u32_t stats_gc_runs;
#endif

#if SPIFFS_CACHE
  // cache memory
  void *cache;
  // cache size
  u32_t cache_size;
#if SPIFFS_CACHE_STATS
  u32_t cache_hits;
  u32_t cache_misses;
#endif
#endif

  // check callback function
  spiffs_check_callback check_cb_f;
} spiffs;

typedef struct {
  spiffs_obj_id obj_id;
  u32_t size;
  spiffs_obj_type type;
  u8_t name[SPIFFS_OBJ_NAME_LEN];
} spiffs_stat;

struct spiffs_dirent {
  spiffs_obj_id obj_id;
  u8_t name[SPIFFS_OBJ_NAME_LEN];
  spiffs_obj_type type;
  u32_t size;
};

typedef struct {
  spiffs *fs;
  spiffs_block_ix block;
  int entry;
} spiffs_DIR;

// functions

/**
 * Initializes the file system dynamic parameters and mounts the filesystem
 * @param fs            the file system struct
 * @param config        the physical and logical configuration of the file system
 * @param work          a memory work buffer comprising 2*config->log_page_size
 *                      bytes used throughout all file system operations
 * @param fd_space      memory for file descriptors
 * @param fd_space_size memory size of file descriptors
 * @param cache         memory for cache, may be null
 * @param cache_size    memory size of cache
 * @param check_cb_f    callback function for reporting during consistency checks
 */
s32_t SPIFFS_mount(spiffs *fs, spiffs_config *config, u8_t *work,
    u8_t *fd_space, u32_t fd_space_size,
    void *cache, u32_t cache_size,
    spiffs_check_callback check_cb_f);

void SPIFFS_unmount(spiffs *fs);

s32_t SPIFFS_creat(spiffs *fs, const char *path, spiffs_attr attr);
spiffs_file SPIFFS_open(spiffs *fs, const char *path, spiffs_attr attr, spiffs_mode mode);
s32_t SPIFFS_read(spiffs *fs, spiffs_file fh, void *buf, s32_t len);
s32_t SPIFFS_write(spiffs *fs, spiffs_file fh, void *buf, s32_t len);
s32_t SPIFFS_lseek(spiffs *fs, spiffs_file fh, s32_t offs, int whence);
s32_t SPIFFS_remove(spiffs *fs, const char *path);
s32_t SPIFFS_fremove(spiffs *fs, spiffs_file fh);
s32_t SPIFFS_stat(spiffs *fs, const char *path, spiffs_stat *s);
s32_t SPIFFS_fstat(spiffs *fs, spiffs_file fh, spiffs_stat *s);
s32_t SPIFFS_fflush(spiffs *fs, spiffs_file fh);
void SPIFFS_close(spiffs *fs, spiffs_file fh);
s32_t SPIFFS_errno(spiffs *fs);

spiffs_DIR *SPIFFS_opendir(spiffs *fs, const char *name, spiffs_DIR *d);
s32_t SPIFFS_closedir(spiffs_DIR *d);
struct spiffs_dirent *SPIFFS_readdir(spiffs_DIR *d, struct spiffs_dirent *e);

s32_t SPIFFS_check(spiffs *fs);

#if SPIFFS_TEST_VISUALISATION
s32_t SPIFFS_vis(spiffs *fs);
#endif

#endif /* SPIFFS_H_ */
