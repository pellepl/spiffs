/*
 * test_suites.c
 *
 *  Created on: Jun 19, 2013
 *      Author: petera
 */


#include "testrunner.h"
#include "test_spiffs.h"
#include "spiffs_nucleus.h"
#include "spiffs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

//#define REMOVE_LONG_TESTS

static void test_on_stop(test *t) {
  printf("  spiffs errno:%i\n", SPIFFS_errno(&__fs));
}

#define CHECK(r) if (!(r)) return -1;
#define CHECK_RES(r) if (r < 0) return -1;
#define FS_PURE_DATA_PAGES(fs) \
    ((fs)->cfg.phys_size / (fs)->cfg.log_page_size - (fs)->block_count * SPIFFS_OBJ_LOOKUP_PAGES(fs))
#define FS_PURE_DATA_SIZE(fs) \
    FS_PURE_DATA_PAGES(fs) * SPIFFS_DATA_PAGE_SIZE(fs)

void memrand(u8_t *b, int len) {
  int i;
  for (i = 0; i < len; i++) {
    b[i] = rand();
  }
}

int test_create_file(char *name) {
  spiffs_stat s;
  spiffs_file fd;
  int res = SPIFFS_creat(FS, name, 0);
  CHECK_RES(res);
  fd = SPIFFS_open(FS, name, 0, SPIFFS_RDONLY);
  CHECK(fd >= 0);
  res = SPIFFS_fstat(FS, fd, &s);
  CHECK_RES(res);
  CHECK(strcmp((char*)s.name, name) == 0);
  CHECK(s.size == 0);
  SPIFFS_close(FS, fd);
  return 0;
}

int test_create_and_write_file(char *name, int size, int chunk_size) {
  int res;
  spiffs_file fd;
  printf("    create and write %s", name);
  res = test_create_file(name);
  CHECK(res >= 0);
  fd = SPIFFS_open(FS, name, 0, SPIFFS_APPEND | SPIFFS_RDWR);
  CHECK(fd >= 0);
  int pfd = open(make_test_fname(name), O_APPEND | O_TRUNC | O_CREAT | O_RDWR);
  int offset = 0;
  int mark = 0;
  while (offset < size) {
    int len = MIN(size-offset, chunk_size);
    if (offset > mark) {
      mark += size/16;
      printf(".");
      fflush(stdout);
    }
    u8_t *buf = malloc(len);
    memrand(buf, len);
    res = SPIFFS_write(FS, fd, buf, len);
    write(pfd, buf, len);
    free(buf);
    if (res < 0) {
      printf("\n  error @ offset %i, res %i\n", offset, res);
    }
    offset += len;
    CHECK(res >= 0);
  }
  printf("\n");
  close(pfd);

  spiffs_stat stat;
  res = SPIFFS_fstat(FS, fd, &stat);
  CHECK(res >= 0);
  CHECK(stat.size == size);

  SPIFFS_close(FS, fd);
  return 0;
}

#if SPIFFS_CACHE
#if SPIFFS_CACHE_STATS
static u32_t chits_tot = 0;
static u32_t cmiss_tot = 0;
#endif
#endif

void _setup() {
  fs_reset();
  test_init(test_on_stop);
}

void _teardown() {
  clear_test_path();
  printf("  free blocks     : %i of %i\n", (FS)->free_blocks, (FS)->block_count);
  printf("  pages allocated : %i\n", (FS)->stats_p_allocated);
  printf("  pages deleted   : %i\n", (FS)->stats_p_deleted);
#if SPIFFS_GC_STATS
  printf("  gc runs         : %i\n", (FS)->stats_gc_runs);
#endif
#if SPIFFS_CACHE
#if SPIFFS_CACHE_STATS
  chits_tot += (FS)->cache_hits;
  cmiss_tot += (FS)->cache_misses;
  printf("  cache hits      : %i (sum %i)\n", (FS)->cache_hits, chits_tot);
  printf("  cache misses    : %i (sum %i)\n", (FS)->cache_misses, cmiss_tot);
  printf("  cache utiliz    : %f\n", ((float)chits_tot/(float)(chits_tot + cmiss_tot)));
#endif
#endif


}

typedef enum {
  EMPTY,
  SMALL,
  MEDIUM,
  LARGE,
} tfile_size;

typedef enum {
  UNTAMPERED,
  APPENDED,
  MODIFIED,
  REWRITTEN,
} tfile_type;

typedef enum {
  SHORT = 4,
  NORMAL = 20,
  LONG = 100,
} tfile_life;

typedef struct  {
  tfile_size tsize;
  tfile_type ttype;
  tfile_life tlife;
} tfile_conf;

typedef struct  {
  int state;
  spiffs_file fd;
  tfile_conf cfg;
  char name[32];
} tfile;

u32_t tfile_get_size(tfile_size s) {
  switch (s) {
  case EMPTY:
    return 0;
  case SMALL:
    return SPIFFS_DATA_PAGE_SIZE(FS)/2;
  case MEDIUM:
    return SPIFFS_DATA_PAGE_SIZE(FS) * (SPIFFS_PAGES_PER_BLOCK(FS) - SPIFFS_OBJ_LOOKUP_PAGES(FS));
  case LARGE:
    return (FS)->cfg.phys_size/3;
  }
  return 0;
}

int run_file_config(int cfg_count, tfile_conf* cfgs, int max_runs, int max_concurrent_files, int dbg) {
  int res;
  tfile *tfiles = malloc(sizeof(tfile) * max_concurrent_files);
  memset(tfiles, 0, sizeof(tfile) * max_concurrent_files);
  int run = 0;
  int cur_config_ix = 0;
  char name[32];
  while (run < max_runs)  {
    if (dbg) printf(" run %i/%i\n", run, max_runs);
    int i;
    for (i = 0; i < max_concurrent_files; i++) {
      sprintf(name, "file%i_%i", (1+run), i);
      tfile *tf = &tfiles[i];
      if (tf->state == 0 && cur_config_ix < cfg_count) {
// create a new file
        strcpy(tf->name, name);
        if (dbg) printf("   create new %s with cfg %i/%i\n", name, (1+cur_config_ix), cfg_count);
        tf->state = 1;
        tf->cfg = cfgs[cur_config_ix];
        if (tf->cfg.tsize == EMPTY) {
          res = SPIFFS_creat(FS, name, 0);
          CHECK_RES(res);
          int pfd = open(make_test_fname(name), O_TRUNC | O_CREAT | O_RDWR);
          close(pfd);
          int extra_flags = tf->cfg.ttype == APPENDED ? SPIFFS_APPEND : 0;
          spiffs_file fd = SPIFFS_open(FS, name, 0, extra_flags | SPIFFS_RDWR);
          CHECK(fd > 0);
          tf->fd = fd;
        } else {
          int extra_flags = tf->cfg.ttype == APPENDED ? SPIFFS_APPEND : 0;
          spiffs_file fd = SPIFFS_open(FS, name, 0, extra_flags | SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_RDWR);
          CHECK(fd > 0);
          extra_flags = tf->cfg.ttype == APPENDED ? O_APPEND : 0;
          int pfd = open(make_test_fname(name), extra_flags | O_TRUNC | O_CREAT | O_RDWR);
          tf->fd = fd;
          int size = tfile_get_size(tf->cfg.tsize);
          u8_t *buf = malloc(size);
          memrand(buf, size);
          res = SPIFFS_write(FS, fd, buf, size);
          CHECK_RES(res);
          write(pfd, buf, size);
          close(pfd);
          free(buf);
          res = read_and_verify(name);
          CHECK_RES(res);
        }

        cur_config_ix++;
      } else if (tf->state > 0) {
// hande file lifecycle
        switch (tf->cfg.ttype) {
        case UNTAMPERED: {
          break;
        }
        case APPENDED: {
          if (dbg) printf("   appending %s\n", tf->name);
          int size = SPIFFS_DATA_PAGE_SIZE(FS)*3;
          u8_t *buf = malloc(size);
          memrand(buf, size);
          res = SPIFFS_write(FS, tf->fd, buf, size);
          CHECK_RES(res);
          int pfd = open(make_test_fname(tf->name), O_APPEND | O_RDWR);
          write(pfd, buf, size);
          close(pfd);
          free(buf);
          res = read_and_verify(tf->name);
          CHECK_RES(res);
          break;
        }
        case MODIFIED: {
          if (dbg) printf("   modify %s\n", tf->name);
          spiffs_stat stat;
          res = SPIFFS_fstat(FS, tf->fd, &stat);
          CHECK_RES(res);
          int size = stat.size / tf->cfg.tlife + SPIFFS_DATA_PAGE_SIZE(FS)/3;
          int offs = (stat.size / tf->cfg.tlife) * tf->state;
          res = SPIFFS_lseek(FS, tf->fd, offs, SPIFFS_SEEK_SET);
          CHECK_RES(res);
          u8_t *buf = malloc(size);
          memrand(buf, size);
          res = SPIFFS_write(FS, tf->fd, buf, size);
          CHECK_RES(res);
          int pfd = open(make_test_fname(tf->name), O_RDWR);
          lseek(pfd, offs, SEEK_SET);
          write(pfd, buf, size);
          close(pfd);
          free(buf);
          res = read_and_verify(tf->name);
          CHECK_RES(res);
          break;
        }
        case REWRITTEN: {
          if (tf->fd > 0) {
            SPIFFS_close(FS, tf->fd);
          }
          if (dbg) printf("   rewriting %s\n", tf->name);
          spiffs_file fd = SPIFFS_open(FS, tf->name, 0, SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_RDWR);
          CHECK(fd > 0);
          int pfd = open(make_test_fname(tf->name), O_TRUNC | O_CREAT | O_RDWR);
          tf->fd = fd;
          int size = tfile_get_size(tf->cfg.tsize);
          u8_t *buf = malloc(size);
          memrand(buf, size);
          res = SPIFFS_write(FS, fd, buf, size);
          CHECK_RES(res);
          write(pfd, buf, size);
          close(pfd);
          free(buf);
          res = read_and_verify(tf->name);
          CHECK_RES(res);
          break;
        }
        }
        tf->state++;
        if (tf->state > tf->cfg.tlife) {
// file outlived its time, kill it
          if (tf->fd > 0) {
            SPIFFS_close(FS, tf->fd);
          }
          if (dbg) printf("   removing %s\n", tf->name);
          res = read_and_verify(tf->name);
          CHECK_RES(res);
          res = SPIFFS_remove(FS, tf->name);
          CHECK_RES(res);
          remove(make_test_fname(tf->name));
          memset(tf, 0, sizeof(tf));
        }

      }
    }

    run++;
  }
  free(tfiles);
  return 0;
}




SUITE(hydrogen_tests)
void setup() {
  _setup();
}
void teardown() {
  _teardown();
}


TEST(missing_file)
{
  spiffs_file fd = SPIFFS_open(FS, "this_wont_exist", 0, 0);
  TEST_CHECK(fd < 0);
  return TEST_RES_OK;
}
TEST_END(missing_file)


TEST(bad_fd)
{
  int res;
  spiffs_stat s;
  spiffs_file fd = SPIFFS_open(FS, "this_wont_exist", 0, 0);
  TEST_CHECK(fd < 0);
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_BAD_DESCRIPTOR);
  res = SPIFFS_fremove(FS, fd);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_BAD_DESCRIPTOR);
  res = SPIFFS_lseek(FS, fd, 0, SPIFFS_SEEK_CUR);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_BAD_DESCRIPTOR);
  res = SPIFFS_read(FS, fd, 0, 0);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_BAD_DESCRIPTOR);
  res = SPIFFS_write(FS, fd, 0, 0);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_BAD_DESCRIPTOR);
  return TEST_RES_OK;
}
TEST_END(bad_fd)


TEST(closed_fd)
{
  int res;
  spiffs_stat s;
  res = test_create_file("file");
  spiffs_file fd = SPIFFS_open(FS, "file", 0, SPIFFS_RDONLY);
  TEST_CHECK(fd >= 0);
  SPIFFS_close(FS, fd);

  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res = SPIFFS_fremove(FS, fd);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res = SPIFFS_lseek(FS, fd, 0, SPIFFS_SEEK_CUR);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res = SPIFFS_read(FS, fd, 0, 0);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res = SPIFFS_write(FS, fd, 0, 0);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  return TEST_RES_OK;
}
TEST_END(closed_fd)


TEST(deleted_same_fd)
{
  int res;
  spiffs_stat s;
  spiffs_file fd;
  res = test_create_file("remove");
  fd = SPIFFS_open(FS, "remove", 0, SPIFFS_RDONLY);
  TEST_CHECK(fd >= 0);
  fd = SPIFFS_open(FS, "remove", 0, SPIFFS_RDONLY);
  TEST_CHECK(fd >= 0);
  res = SPIFFS_fremove(FS, fd);
  TEST_CHECK(res >= 0);

  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res = SPIFFS_fremove(FS, fd);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res = SPIFFS_lseek(FS, fd, 0, SPIFFS_SEEK_CUR);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res = SPIFFS_read(FS, fd, 0, 0);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res = SPIFFS_write(FS, fd, 0, 0);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);

  return TEST_RES_OK;
}
TEST_END(deleted_same_fd)


TEST(deleted_other_fd)
{
  int res;
  spiffs_stat s;
  spiffs_file fd, fd_orig;
  res = test_create_file("remove");
  fd_orig = SPIFFS_open(FS, "remove", 0, SPIFFS_RDONLY);
  TEST_CHECK(fd_orig >= 0);
  fd = SPIFFS_open(FS, "remove", 0, SPIFFS_RDONLY);
  TEST_CHECK(fd >= 0);
  res = SPIFFS_fremove(FS, fd_orig);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd_orig);

  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res = SPIFFS_fremove(FS, fd);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res = SPIFFS_lseek(FS, fd, 0, SPIFFS_SEEK_CUR);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res = SPIFFS_read(FS, fd, 0, 0);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res = SPIFFS_write(FS, fd, 0, 0);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);

  return TEST_RES_OK;
}
TEST_END(deleted_other_fd)


TEST(file_by_open)
{
  int res;
  spiffs_stat s;
  spiffs_file fd = SPIFFS_open(FS, "filebopen", 0, SPIFFS_CREAT);
  TEST_CHECK(fd >= 0);
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  TEST_CHECK(strcmp((char*)s.name, "filebopen") == 0);
  TEST_CHECK(s.size == 0);
  SPIFFS_close(FS, fd);

  fd = SPIFFS_open(FS, "filebopen", 0, SPIFFS_RDONLY);
  TEST_CHECK(fd >= 0);
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  TEST_CHECK(strcmp((char*)s.name, "filebopen") == 0);
  TEST_CHECK(s.size == 0);
  SPIFFS_close(FS, fd);
  return TEST_RES_OK;
}
TEST_END(file_by_open)


TEST(file_by_creat)
{
  int res;
  res = test_create_file("filebcreat");
  TEST_CHECK(res >= 0);
  return TEST_RES_OK;
}
TEST_END(file_by_creat)




TEST(list_dir)
{
  int res;
  res = test_create_file("file1");
  TEST_CHECK(res >= 0);
  res = test_create_file("file2");
  TEST_CHECK(res >= 0);
  res = test_create_file("file3");
  TEST_CHECK(res >= 0);
  res = test_create_file("file4");
  TEST_CHECK(res >= 0);

  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;

  SPIFFS_opendir(FS, "/", &d);
  while ((pe = SPIFFS_readdir(&d, pe))) {
    printf("  %s\n", pe->name);
    // TODO verify
  }
  SPIFFS_closedir(&d);

  return TEST_RES_OK;
}
TEST_END(list_dir)



TEST(remove_single_by_path)
{
  int res;
  spiffs_file fd;
  res = test_create_file("remove");
  TEST_CHECK(res >= 0);
  res = SPIFFS_remove(FS, "remove");
  TEST_CHECK(res >= 0);
  fd = SPIFFS_open(FS, "remove", 0, SPIFFS_RDONLY);
  TEST_CHECK(fd < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_NOT_FOUND);

  return TEST_RES_OK;
}
TEST_END(remove_single_by_path)


TEST(remove_single_by_fd)
{
  int res;
  spiffs_file fd;
  res = test_create_file("remove");
  TEST_CHECK(res >= 0);
  fd = SPIFFS_open(FS, "remove", 0, SPIFFS_RDONLY);
  TEST_CHECK(fd >= 0);
  res = SPIFFS_fremove(FS, fd);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);
  fd = SPIFFS_open(FS, "remove", 0, SPIFFS_RDONLY);
  TEST_CHECK(fd < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_NOT_FOUND);

  return TEST_RES_OK;
}
TEST_END(remove_single_by_fd)


TEST(write_big_file_chunks_page)
{
  int size = ((50*(FS)->cfg.phys_size)/100);
  printf("  filesize %i\n", size);
  int res = test_create_and_write_file("bigfile", size, SPIFFS_DATA_PAGE_SIZE(FS));
  TEST_CHECK(res >= 0);
  res = read_and_verify("bigfile");
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
}
TEST_END(write_big_file_chunks_page)


TEST(write_big_files_chunks_page)
{
  char name[32];
  int f;
  int files = 10;
  int res;
  int size = ((50*(FS)->cfg.phys_size)/100)/files;
  printf("  filesize %i\n", size);
  for (f = 0; f < files; f++) {
    sprintf(name, "bigfile%i", f);
    res = test_create_and_write_file(name, size, SPIFFS_DATA_PAGE_SIZE(FS));
    TEST_CHECK(res >= 0);
  }
  for (f = 0; f < files; f++) {
    sprintf(name, "bigfile%i", f);
    res = read_and_verify(name);
    TEST_CHECK(res >= 0);
  }

  return TEST_RES_OK;
}
TEST_END(write_big_files_chunks_page)


TEST(write_big_file_chunks_index)
{
  int size = ((50*(FS)->cfg.phys_size)/100);
  printf("  filesize %i\n", size);
  int res = test_create_and_write_file("bigfile", size, SPIFFS_DATA_PAGE_SIZE(FS) * SPIFFS_OBJ_HDR_IX_LEN(FS));
  TEST_CHECK(res >= 0);
  res = read_and_verify("bigfile");
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
}
TEST_END(write_big_file_chunks_index)


TEST(write_big_files_chunks_index)
{
  char name[32];
  int f;
  int files = 10;
  int res;
  int size = ((50*(FS)->cfg.phys_size)/100)/files;
  printf("  filesize %i\n", size);
  for (f = 0; f < files; f++) {
    sprintf(name, "bigfile%i", f);
    res = test_create_and_write_file(name, size, SPIFFS_DATA_PAGE_SIZE(FS) * SPIFFS_OBJ_HDR_IX_LEN(FS));
    TEST_CHECK(res >= 0);
  }
  for (f = 0; f < files; f++) {
    sprintf(name, "bigfile%i", f);
    res = read_and_verify(name);
    TEST_CHECK(res >= 0);
  }

  return TEST_RES_OK;
}
TEST_END(write_big_files_chunks_index)


TEST(write_big_file_chunks_huge)
{
  int size = (FS_PURE_DATA_PAGES(FS) / 2) * SPIFFS_DATA_PAGE_SIZE(FS);
  printf("  filesize %i\n", size);
  int res = test_create_and_write_file("bigfile", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("bigfile");
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
}
TEST_END(write_big_file_chunks_huge)


TEST(write_big_files_chunks_huge)
{
  char name[32];
  int f;
  int files = 10;
  int res;
  int size = ((50*(FS)->cfg.phys_size)/100)/files;
  printf("  filesize %i\n", size);
  for (f = 0; f < files; f++) {
    sprintf(name, "bigfile%i", f);
    res = test_create_and_write_file(name, size, size);
    TEST_CHECK(res >= 0);
  }
  for (f = 0; f < files; f++) {
    sprintf(name, "bigfile%i", f);
    res = read_and_verify(name);
    TEST_CHECK(res >= 0);
  }

  return TEST_RES_OK;
}
TEST_END(write_big_files_chunks_huge)


TEST(truncate_big_file)
{
  int size = (FS_PURE_DATA_PAGES(FS) / 2) * SPIFFS_DATA_PAGE_SIZE(FS);
  printf("  filesize %i\n", size);
  int res = test_create_and_write_file("bigfile", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("bigfile");
  TEST_CHECK(res >= 0);
  spiffs_file fd = SPIFFS_open(FS, "bigfile", 0, SPIFFS_RDWR);
  TEST_CHECK(fd > 0);
  res = SPIFFS_fremove(FS, fd);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  fd = SPIFFS_open(FS, "bigfile", 0, SPIFFS_RDWR);
  TEST_CHECK(fd < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_NOT_FOUND);

  return TEST_RES_OK;
}
TEST_END(truncate_big_file)



TEST(file_uniqueness)
{
  int res;
  spiffs_file fd;
  char fname[32];
  int files = (FS_PURE_DATA_PAGES(FS) / 2) - SPIFFS_PAGES_PER_BLOCK(FS)*3;
  int i;
  printf("  creating %i files\n", files);
  for (i = 0; i < files; i++) {
    char content[20];
    sprintf(fname, "file%i", i);
    sprintf(content, "%i", i);
    res = test_create_file(fname);
    TEST_CHECK(res >= 0);
    fd = SPIFFS_open(FS, fname, 0, SPIFFS_APPEND | SPIFFS_RDWR);
    TEST_CHECK(fd >= 0);
    res = SPIFFS_write(FS, fd, content, strlen(content)+1);
    TEST_CHECK(res >= 0);
    SPIFFS_close(FS, fd);
  }
  printf("  checking %i files\n", files);
  for (i = 0; i < files; i++) {
    char content[20];
    char ref_content[20];
    sprintf(fname, "file%i", i);
    sprintf(content, "%i", i);
    fd = SPIFFS_open(FS, fname, 0, SPIFFS_RDONLY);
    TEST_CHECK(fd >= 0);
    res = SPIFFS_read(FS, fd, ref_content, strlen(content)+1);
    TEST_CHECK(res >= 0);
    TEST_CHECK(strcmp(ref_content, content) == 0);
    SPIFFS_close(FS, fd);
  }
  printf("  removing %i files\n", files/2);
  for (i = 0; i < files; i += 2) {
    sprintf(fname, "file%i", i);
    res = SPIFFS_remove(FS, fname);
    TEST_CHECK(res >= 0);
  }
  printf("  creating %i files\n", files/2);
  for (i = 0; i < files; i += 2) {
    char content[20];
    sprintf(fname, "file%i", i);
    sprintf(content, "new%i", i);
    res = test_create_file(fname);
    TEST_CHECK(res >= 0);
    fd = SPIFFS_open(FS, fname, 0, SPIFFS_APPEND | SPIFFS_RDWR);
    TEST_CHECK(fd >= 0);
    res = SPIFFS_write(FS, fd, content, strlen(content)+1);
    TEST_CHECK(res >= 0);
    SPIFFS_close(FS, fd);
  }
  printf("  checking %i files\n", files);
  for (i = 0; i < files; i++) {
    char content[20];
    char ref_content[20];
    sprintf(fname, "file%i", i);
    if ((i & 1) == 0) {
      sprintf(content, "new%i", i);
    } else {
      sprintf(content, "%i", i);
    }
    fd = SPIFFS_open(FS, fname, 0, SPIFFS_RDONLY);
    TEST_CHECK(fd >= 0);
    res = SPIFFS_read(FS, fd, ref_content, strlen(content)+1);
    TEST_CHECK(res >= 0);
    TEST_CHECK(strcmp(ref_content, content) == 0);
    SPIFFS_close(FS, fd);
  }

  return TEST_RES_OK;
}
TEST_END(file_uniqueness)

int create_and_read_back(int size, int chunk) {
  char *name = "file";
  spiffs_file fd;
  s32_t res;

  u8_t *buf = malloc(size);
  memrand(buf, size);

  res = test_create_file(name);
  CHECK(res >= 0);
  fd = SPIFFS_open(FS, name, 0, SPIFFS_APPEND | SPIFFS_RDWR);
  CHECK(fd >= 0);
  res = SPIFFS_write(FS, fd, buf, size);
  CHECK(res >= 0);

  spiffs_stat stat;
  res = SPIFFS_fstat(FS, fd, &stat);
  CHECK(res >= 0);
  CHECK(stat.size == size);

  SPIFFS_close(FS, fd);

  fd = SPIFFS_open(FS, name, 0, SPIFFS_RDONLY);
  CHECK(fd >= 0);

  u8_t *rbuf = malloc(size);
  int offs = 0;
  while (offs < size) {
    int len = MIN(size - offs, chunk);
    res = SPIFFS_read(FS, fd, &rbuf[offs], len);
    CHECK(res >= 0);
    CHECK(memcmp(&rbuf[offs], &buf[offs], len) == 0);

    offs += chunk;
  }

  CHECK(memcmp(&rbuf[0], &buf[0], size) == 0);

  SPIFFS_close(FS, fd);

  free(rbuf);
  free(buf);

  return 0;
}

TEST(read_chunk_1)
{
  TEST_CHECK(create_and_read_back(SPIFFS_DATA_PAGE_SIZE(FS)*8, 1) == 0);
  return TEST_RES_OK;
}
TEST_END(read_chunk_1)


TEST(read_chunk_page)
{
  TEST_CHECK(create_and_read_back(SPIFFS_DATA_PAGE_SIZE(FS)*(SPIFFS_PAGES_PER_BLOCK(FS) - SPIFFS_OBJ_LOOKUP_PAGES(FS))*2,
      SPIFFS_DATA_PAGE_SIZE(FS)) == 0);
  return TEST_RES_OK;
}
TEST_END(read_chunk_page)


TEST(read_chunk_index)
{
  TEST_CHECK(create_and_read_back(SPIFFS_DATA_PAGE_SIZE(FS)*(SPIFFS_PAGES_PER_BLOCK(FS) - SPIFFS_OBJ_LOOKUP_PAGES(FS))*4,
      SPIFFS_DATA_PAGE_SIZE(FS)*(SPIFFS_PAGES_PER_BLOCK(FS) - SPIFFS_OBJ_LOOKUP_PAGES(FS))) == 0);
  return TEST_RES_OK;
}
TEST_END(read_chunk_index)


TEST(read_chunk_huge)
{
  int sz = (2*(FS)->cfg.phys_size)/3;
  TEST_CHECK(create_and_read_back(sz, sz) == 0);
  return TEST_RES_OK;
}
TEST_END(read_chunk_huge)


TEST(lseek_simple_modification) {
  int res;
  spiffs_file fd;
  char *fname = "seekfile";
  int i;
  int len = 4096;
  fd = SPIFFS_open(FS, fname, 0, SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_RDWR);
  TEST_CHECK(fd > 0);
  int pfd = open(make_test_fname(fname), O_TRUNC | O_CREAT | O_RDWR);
  u8_t *buf = malloc(len);
  memrand(buf, len);
  res = SPIFFS_write(FS, fd, buf, len);
  TEST_CHECK(res >= 0);
  write(pfd, buf, len);
  free(buf);
  res = read_and_verify(fname);
  TEST_CHECK(res >= 0);

  res = SPIFFS_lseek(FS, fd, len/2, SPIFFS_SEEK_SET);
  TEST_CHECK(res >= 0);
  lseek(pfd, len/2, SEEK_SET);
  len = len/4;
  buf = malloc(len);
  memrand(buf, len);
  res = SPIFFS_write(FS, fd, buf, len);
  TEST_CHECK(res >= 0);
  write(pfd, buf, len);
  free(buf);

  res = read_and_verify(fname);
  TEST_CHECK(res >= 0);

  SPIFFS_close(FS, fd);
  close(pfd);

  return TEST_RES_OK;
}
TEST_END(lseek_simple_modification)


TEST(lseek_modification_append) {
  int res;
  spiffs_file fd;
  char *fname = "seekfile";
  int i;
  int len = 4096;
  fd = SPIFFS_open(FS, fname, 0, SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_RDWR);
  TEST_CHECK(fd > 0);
  int pfd = open(make_test_fname(fname), O_TRUNC | O_CREAT | O_RDWR);
  u8_t *buf = malloc(len);
  memrand(buf, len);
  res = SPIFFS_write(FS, fd, buf, len);
  TEST_CHECK(res >= 0);
  write(pfd, buf, len);
  free(buf);
  res = read_and_verify(fname);
  TEST_CHECK(res >= 0);

  res = SPIFFS_lseek(FS, fd, len/2, SPIFFS_SEEK_SET);
  TEST_CHECK(res >= 0);
  lseek(pfd, len/2, SEEK_SET);

  buf = malloc(len);
  memrand(buf, len);
  res = SPIFFS_write(FS, fd, buf, len);
  TEST_CHECK(res >= 0);
  write(pfd, buf, len);
  free(buf);

  res = read_and_verify(fname);
  TEST_CHECK(res >= 0);

  SPIFFS_close(FS, fd);
  close(pfd);

  return TEST_RES_OK;
}
TEST_END(lseek_modification_append)


TEST(lseek_modification_append_multi) {
  int res;
  spiffs_file fd;
  char *fname = "seekfile";
  int len = 1024;
  int runs = (FS_PURE_DATA_PAGES(FS) / 2) * SPIFFS_DATA_PAGE_SIZE(FS) / (len/2);

  fd = SPIFFS_open(FS, fname, 0, SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_RDWR);
  TEST_CHECK(fd > 0);
  int pfd = open(make_test_fname(fname), O_TRUNC | O_CREAT | O_RDWR);
  u8_t *buf = malloc(len);
  memrand(buf, len);
  res = SPIFFS_write(FS, fd, buf, len);
  TEST_CHECK(res >= 0);
  write(pfd, buf, len);
  free(buf);
  res = read_and_verify(fname);
  TEST_CHECK(res >= 0);

  while (runs--) {
    res = SPIFFS_lseek(FS, fd, -len/2, SPIFFS_SEEK_END);
    TEST_CHECK(res >= 0);
    lseek(pfd, -len/2, SEEK_END);

    buf = malloc(len);
    memrand(buf, len);
    res = SPIFFS_write(FS, fd, buf, len);
    TEST_CHECK(res >= 0);
    write(pfd, buf, len);
    free(buf);

    res = read_and_verify(fname);
    TEST_CHECK(res >= 0);
  }

  SPIFFS_close(FS, fd);
  close(pfd);

  return TEST_RES_OK;
}
TEST_END(lseek_modification_append_multi)


TEST(lseek_read) {
  int res;
  spiffs_file fd;
  char *fname = "seekfile";
  int len = (FS_PURE_DATA_PAGES(FS) / 2) * SPIFFS_DATA_PAGE_SIZE(FS);
  int runs = 100000;

  fd = SPIFFS_open(FS, fname, 0, SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_RDWR);
  TEST_CHECK(fd > 0);
  u8_t *refbuf = malloc(len);
  memrand(refbuf, len);
  res = SPIFFS_write(FS, fd, refbuf, len);
  TEST_CHECK(res >= 0);

  int offs = 0;
  res = SPIFFS_lseek(FS, fd, 0, SPIFFS_SEEK_SET);
  TEST_CHECK(res >= 0);

  while (runs--) {
    int i;
    u8_t buf[64];
    if (offs + 41 + sizeof(buf) >= len) {
      offs = (offs + 41 + sizeof(buf)) % len;
      res = SPIFFS_lseek(FS, fd, offs, SPIFFS_SEEK_SET);
      TEST_CHECK(res >= 0);
    }
    res = SPIFFS_lseek(FS, fd, 41, SPIFFS_SEEK_CUR);
    TEST_CHECK(res >= 0);
    offs += 41;
    res = SPIFFS_read(FS, fd, buf, sizeof(buf));
    TEST_CHECK(res >= 0);
    for (i = 0; i < sizeof(buf); i++) {
      if (buf[i] != refbuf[offs+i]) {
        printf("  mismatch at offs %i\n", offs);
      }
      TEST_CHECK(buf[i] == refbuf[offs+i]);
    }
    offs += sizeof(buf);

    res = SPIFFS_lseek(FS, fd, -(sizeof(buf)+11), SPIFFS_SEEK_CUR);
    TEST_CHECK(res >= 0);
    offs -= (sizeof(buf)+11);
    res = SPIFFS_read(FS, fd, buf, sizeof(buf));
    TEST_CHECK(res >= 0);
    for (i = 0; i < sizeof(buf); i++) {
      if (buf[i] != refbuf[offs+i]) {
        printf("  mismatch at offs %i\n", offs);
      }
      TEST_CHECK(buf[i] == refbuf[offs+i]);
    }
    offs += sizeof(buf);
  }

  free(refbuf);
  SPIFFS_close(FS, fd);

  return TEST_RES_OK;
}
TEST_END(lseek_read)


#ifndef REMOVE_LONG_TESTS
TEST(write_small_file_chunks_1)
{
  int res = test_create_and_write_file("smallfile", 256, 1);
  TEST_CHECK(res >= 0);
  res = read_and_verify("smallfile");
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
}
TEST_END(write_small_file_chunks_1)


TEST(write_small_files_chunks_1)
{
  char name[32];
  int f;
  int size = 512;
  int files = ((20*(FS)->cfg.phys_size)/100)/size;
  int res;
  for (f = 0; f < files; f++) {
    sprintf(name, "smallfile%i", f);
    res = test_create_and_write_file(name, size, 1);
    TEST_CHECK(res >= 0);
  }
  for (f = 0; f < files; f++) {
    sprintf(name, "smallfile%i", f);
    res = read_and_verify(name);
    TEST_CHECK(res >= 0);
  }

  return TEST_RES_OK;
}
TEST_END(write_small_files_chunks_1)

TEST(write_big_file_chunks_1)
{
  int size = ((50*(FS)->cfg.phys_size)/100);
  printf("  filesize %i\n", size);
  int res = test_create_and_write_file("bigfile", size, 1);
  TEST_CHECK(res >= 0);
  res = read_and_verify("bigfile");
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
}
TEST_END(write_big_file_chunks_1)

TEST(write_big_files_chunks_1)
{
  char name[32];
  int f;
  int files = 10;
  int res;
  int size = ((50*(FS)->cfg.phys_size)/100)/files;
  printf("  filesize %i\n", size);
  for (f = 0; f < files; f++) {
    sprintf(name, "bigfile%i", f);
    res = test_create_and_write_file(name, size, 1);
    TEST_CHECK(res >= 0);
  }
  for (f = 0; f < files; f++) {
    sprintf(name, "bigfile%i", f);
    res = read_and_verify(name);
    TEST_CHECK(res >= 0);
  }

  return TEST_RES_OK;
}
TEST_END(write_big_files_chunks_1)
#endif // REMOVE_LONG_TESTS

TEST(long_run_config_many_small_one_long)
{
  tfile_conf cfgs[] = {
      {   .tsize = LARGE,     .ttype = MODIFIED,      .tlife = LONG
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = LONG
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = LONG
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = LONG
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = LONG
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = LONG
      },
  };

  int res = run_file_config(sizeof(cfgs)/sizeof(cfgs[0]), &cfgs[0], 206, 5, 0);
  TEST_CHECK(res >= 0);
  return TEST_RES_OK;
}
TEST_END(long_run_config_many_small_one_long)

TEST(long_run_config_many_medium)
{
  tfile_conf cfgs[] = {
      {   .tsize = MEDIUM,    .ttype = MODIFIED,      .tlife = LONG
      },
      {   .tsize = MEDIUM,    .ttype = APPENDED,      .tlife = LONG
      },
      {   .tsize = LARGE,     .ttype = MODIFIED,      .tlife = LONG
      },
      {   .tsize = MEDIUM,    .ttype = APPENDED,      .tlife = LONG
      },
      {   .tsize = MEDIUM,    .ttype = MODIFIED,      .tlife = LONG
      },
      {   .tsize = MEDIUM,    .ttype = MODIFIED,      .tlife = LONG
      },
      {   .tsize = MEDIUM,    .ttype = APPENDED,      .tlife = LONG
      },
      {   .tsize = LARGE,     .ttype = MODIFIED,      .tlife = LONG
      },
      {   .tsize = MEDIUM,    .ttype = APPENDED,      .tlife = LONG
      },
      {   .tsize = MEDIUM,    .ttype = MODIFIED,      .tlife = LONG
      },
      {   .tsize = MEDIUM,    .ttype = MODIFIED,      .tlife = LONG
      },
      {   .tsize = MEDIUM,    .ttype = APPENDED,      .tlife = LONG
      },
      {   .tsize = LARGE,     .ttype = MODIFIED,      .tlife = LONG
      },
      {   .tsize = MEDIUM,    .ttype = APPENDED,      .tlife = LONG
      },
      {   .tsize = MEDIUM,    .ttype = MODIFIED,      .tlife = LONG
      },
  };

  int res = run_file_config(sizeof(cfgs)/sizeof(cfgs[0]), &cfgs[0], 305, 5, 0);
  TEST_CHECK(res >= 0);
  return TEST_RES_OK;
}
TEST_END(long_run_config_many_medium)


TEST(long_run_config_many_small)
{
  tfile_conf cfgs[] = {
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = LONG
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = LONG
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = MEDIUM
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
  };

  int res = run_file_config(sizeof(cfgs)/sizeof(cfgs[0]), &cfgs[0], 115, 6, 0);
  TEST_CHECK(res >= 0);
  return TEST_RES_OK;
}
TEST_END(long_run_config_many_small)


TEST(long_run)
{
  tfile_conf cfgs[] = {
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = MEDIUM
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = SHORT
      },
      {   .tsize = MEDIUM,    .ttype = MODIFIED,      .tlife = SHORT
      },
      {   .tsize = MEDIUM,    .ttype = APPENDED,      .tlife = SHORT
      },
  };

  int macro_runs = 500;
  printf("  ");
  while (macro_runs--) {
    //printf("  ---- run %i ----\n", macro_runs);
    if ((macro_runs % 20) == 0) {
      printf(".");
      fflush(stdout);
    }
    int res = run_file_config(sizeof(cfgs)/sizeof(cfgs[0]), &cfgs[0], 11, 2, 0);
    TEST_CHECK(res >= 0);
  }
  printf("\n");
  return TEST_RES_OK;
}
TEST_END(long_run)

SUITE_END(hydrogen_tests)

SUITE(dev_tests)
void setup() {
  _setup();
}
void teardown() {
  _teardown();
}
TEST(check1) {
  int size = SPIFFS_DATA_PAGE_SIZE(FS)*3;
  int res = test_create_and_write_file("file", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  spiffs_file fd = SPIFFS_open(FS, "file", 0, 0);
  TEST_CHECK(fd > 0);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  // modify lu entry data page index 1
  spiffs_page_ix pix;
  res = spiffs_obj_lu_find_id_and_index(FS, s.obj_id & ~SPIFFS_OBJ_ID_IX_FLAG, 1, &pix);
  TEST_CHECK(res >= 0);

  // reset lu entry to being erased, but keep page data
  spiffs_obj_id obj_id = SPIFFS_OBJ_ID_ERASED;
  spiffs_block_ix bix = SPIFFS_BLOCK_FOR_PAGE(FS, pix);
  int entry = SPIFFS_OBJ_LOOKUP_ENTRY_FOR_PAGE(FS, pix);
  u32_t addr = SPIFFS_BLOCK_TO_PADDR(FS, bix) + entry*sizeof(spiffs_obj_id);

  area_write(addr, (u8_t*)&obj_id, sizeof(spiffs_obj_id));

  spiffs_cache *cache = get_cache(FS);
  cache->cpage_use_map = 0;

  SPIFFS_check(FS);


  return TEST_RES_OK;
} TEST_END(check1)

SUITE_END(dev_tests)

