/*
 * test_bugreports.c
 *
 *  Created on: Mar 8, 2015
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


SUITE(bug_tests)
void setup() {
  _setup_test_only();
}
void teardown() {
  _teardown();
}

TEST(nodemcu_full_fs_1) {
  fs_reset_specific(0, 4096*20, 4096, 4096, 256);

  int res;
  spiffs_file fd;

  printf("  fill up system by writing one byte a lot\n");
  fd = SPIFFS_open(FS, "test1.txt", SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC, 0);
  TEST_CHECK(fd > 0);
  int i;
  spiffs_stat s;
  res = SPIFFS_OK;
  for (i = 0; i < 100*1000; i++) {
    u8_t buf = 'x';
    res = SPIFFS_write(FS, fd, &buf, 1);
  }

  int errno = SPIFFS_errno(FS);
  int res2 = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res2 == SPIFFS_OK);
  printf("  >>> file %s size: %i\n", s.name, s.size);

  TEST_CHECK(errno == SPIFFS_ERR_FULL);
  SPIFFS_close(FS, fd);

  printf("  remove big file\n");
  res = SPIFFS_remove(FS, "test1.txt");

  printf("res:%i errno:%i\n",res, SPIFFS_errno(FS));

  TEST_CHECK(res == SPIFFS_OK);
  res2 = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res2 == -1);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res2 = SPIFFS_stat(FS, "test1.txt", &s);
  TEST_CHECK(res2 == -1);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_NOT_FOUND);

  printf("  create small file\n");
  fd = SPIFFS_open(FS, "test2.txt", SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC, 0);
  TEST_CHECK(fd > 0);
  res = SPIFFS_OK;
  for (i = 0; res >= 0 && i < 1000; i++) {
    u8_t buf = 'x';
    res = SPIFFS_write(FS, fd, &buf, 1);
  }
  TEST_CHECK(res >= SPIFFS_OK);

  res2 = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res2 == SPIFFS_OK);
  printf("  >>> file %s size: %i\n", s.name, s.size);

  TEST_CHECK(s.size == 1000);
  SPIFFS_close(FS, fd);

  return TEST_RES_OK;

} TEST_END(nodemcu_full_fs_1)

TEST(nodemcu_full_fs_2) {
  fs_reset_specific(0, 4096*22, 4096, 4096, 256);

  int res;
  spiffs_file fd;

  printf("  fill up system by writing one byte a lot\n");
  fd = SPIFFS_open(FS, "test1.txt", SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC, 0);
  TEST_CHECK(fd > 0);
  int i;
  spiffs_stat s;
  res = SPIFFS_OK;
  for (i = 0; i < 100*1000; i++) {
    u8_t buf = 'x';
    res = SPIFFS_write(FS, fd, &buf, 1);
  }

  int errno = SPIFFS_errno(FS);
  int res2 = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res2 == SPIFFS_OK);
  printf("  >>> file %s size: %i\n", s.name, s.size);

  TEST_CHECK(errno == SPIFFS_ERR_FULL);
  SPIFFS_close(FS, fd);

  res2 = SPIFFS_stat(FS, "test1.txt", &s);
  TEST_CHECK(res2 == SPIFFS_OK);

  SPIFFS_clearerr(FS);
  printf("  create small file\n");
  fd = SPIFFS_open(FS, "test2.txt", SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC, 0);
#if 0
  // before gc in v3.1
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_OK);
  TEST_CHECK(fd > 0);

  for (i = 0; i < 1000; i++) {
    u8_t buf = 'x';
    res = SPIFFS_write(FS, fd, &buf, 1);
  }

  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FULL);
  res2 = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res2 == SPIFFS_OK);
  printf("  >>> file %s size: %i\n", s.name, s.size);
  TEST_CHECK(s.size == 0);
  SPIFFS_clearerr(FS);
#else
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FULL);
  SPIFFS_clearerr(FS);
#endif
  printf("  remove files\n");
  res = SPIFFS_remove(FS, "test1.txt");
  TEST_CHECK(res == SPIFFS_OK);
#if 0
  res = SPIFFS_remove(FS, "test2.txt");
  TEST_CHECK(res == SPIFFS_OK);
#endif

  printf("  create medium file\n");
  fd = SPIFFS_open(FS, "test3.txt", SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC, 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_OK);
  TEST_CHECK(fd > 0);

  for (i = 0; i < 20*1000; i++) {
    u8_t buf = 'x';
    res = SPIFFS_write(FS, fd, &buf, 1);
  }
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_OK);

  res2 = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res2 == SPIFFS_OK);
  printf("  >>> file %s size: %i\n", s.name, s.size);
  TEST_CHECK(s.size == 20*1000);

  return TEST_RES_OK;

} TEST_END(nodemcu_full_fs_2)

TEST(magic_test) {
  // one obj lu page, not full
  fs_reset_specific(0, 4096*16, 4096, 4096*1, 128);
  TEST_CHECK(SPIFFS_CHECK_MAGIC_POSSIBLE(FS));
  // one obj lu page, full
  fs_reset_specific(0, 4096*16, 4096, 4096*2, 128);
  TEST_CHECK(!SPIFFS_CHECK_MAGIC_POSSIBLE(FS));
  // two obj lu pages, not full
  fs_reset_specific(0, 4096*16, 4096, 4096*4, 128);
  TEST_CHECK(SPIFFS_CHECK_MAGIC_POSSIBLE(FS));

  return TEST_RES_OK;

} TEST_END(magic_test)

TEST(nodemcu_309) {
  fs_reset_specific(0, 4096*20, 4096, 4096, 256);

  int res;
  spiffs_file fd;
  int j;

  for (j = 1; j <= 3; j++) {
    char fname[32];
    sprintf(fname, "20K%i.txt", j);
    fd = SPIFFS_open(FS, fname, SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_DIRECT, 0);
    TEST_CHECK(fd > 0);
    int i;
    spiffs_stat s;
    res = SPIFFS_OK;
    for (i = 1; i <= 1280; i++) {
      char *buf = "0123456789ABCDE";
      res = SPIFFS_write(FS, fd, buf, strlen(buf));
      if (res < 0) printf("err @ %i,%i\n", i, j);
    }
  }

  int errno = SPIFFS_errno(FS);
  TEST_CHECK(errno == SPIFFS_ERR_FULL);

  u32_t total;
  u32_t used;

  SPIFFS_info(FS, &total, &used);
  printf("total:%i\nused:%i\nremain:%i\nerrno:%i\n", total, used, total-used, errno);
  TEST_CHECK(total-used < 10000);

  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;

  SPIFFS_opendir(FS, "/", &d);
  int spoon_guard = 0;
  while ((pe = SPIFFS_readdir(&d, pe))) {
    printf("%s [%04x] size:%i\n", pe->name, pe->obj_id, pe->size);
    TEST_CHECK(spoon_guard++ < 3);
  }
  TEST_CHECK(spoon_guard == 3);
  SPIFFS_closedir(&d);

  return TEST_RES_OK;

} TEST_END(nodemcu_309)

SUITE_END(bug_tests)
