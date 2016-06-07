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

SUITE(hydrogen_tests)
static void setup() {
  _setup();
}
static void teardown() {
  _teardown();
}

TEST(info)
{
  u32_t used, total;
  int res = SPIFFS_info(FS, &total, &used);
  TEST_CHECK(res == SPIFFS_OK);
  TEST_CHECK(used == 0);
  TEST_CHECK(total < __fs.cfg.phys_size);
  return TEST_RES_OK;
}
TEST_END

#if SPIFFS_USE_MAGIC
TEST(magic)
{
  fs_reset_specific(0, 0, 65536*16, 65536, 65536, 256);
  SPIFFS_unmount(FS);

  TEST_CHECK_EQ(fs_mount_specific(0, 65536*16, 65536, 65536, 256), SPIFFS_OK);
  SPIFFS_unmount(FS);

  TEST_CHECK_NEQ(fs_mount_specific(0, 65536*16, 65536, 65536, 128), SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NOT_A_FS);

  TEST_CHECK_NEQ(fs_mount_specific(4, 65536*16, 65536, 65536, 256), SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NOT_A_FS);

  return TEST_RES_OK;
}
TEST_END


#if SPIFFS_USE_MAGIC_LENGTH
TEST(magic_length)
{
  fs_reset_specific(0, 0, 65536*16, 65536, 65536, 256);
  SPIFFS_unmount(FS);

  TEST_CHECK_EQ(fs_mount_specific(0, 65536*16, 65536, 65536, 256), SPIFFS_OK);
  SPIFFS_unmount(FS);

  TEST_CHECK_NEQ(fs_mount_specific(0, 65536*8, 65536, 65536, 256), SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NOT_A_FS);

  TEST_CHECK_NEQ(fs_mount_specific(0, 65536*15, 65536, 65536, 256), SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NOT_A_FS);

  TEST_CHECK_NEQ(fs_mount_specific(0, 65536*17, 65536, 65536, 256), SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NOT_A_FS);

  TEST_CHECK_NEQ(fs_mount_specific(0, 65536*256, 65536, 65536, 256), SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NOT_A_FS);

  return TEST_RES_OK;
}
TEST_END

#if SPIFFS_SINGLETON==0
TEST(magic_length_probe)
{
  fs_reset_specific(0, 0, 65536*16, 65536, 65536, 256);
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), 65536*16);

  fs_reset_specific(0, 0, 65536*24, 65536, 65536, 256);
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), 65536*24);

  fs_reset_specific(0, 0, 32768*16, 32768, 32768, 128);
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), 32768*16);

  fs_reset_specific(0, 0, 16384*37, 16384, 16384, 128);
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), 16384*37);

  fs_reset_specific(0, 0, 4096*11, 4096, 4096, 256);
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), 4096*11);

  fs_reset_specific(0, 0, 4096*8, 4096, 4096, 256);
  __fs.cfg.log_page_size = 128;
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), SPIFFS_ERR_PROBE_NOT_A_FS);
  __fs.cfg.log_page_size = 512;
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), SPIFFS_ERR_PROBE_NOT_A_FS);
  __fs.cfg.log_page_size = 256;
  __fs.cfg.log_block_size = 8192;
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), SPIFFS_ERR_PROBE_NOT_A_FS);
  __fs.cfg.log_block_size = 2048;
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), SPIFFS_ERR_PROBE_NOT_A_FS);
  __fs.cfg.log_block_size = 4096;
  __fs.cfg.phys_addr += 2;
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), SPIFFS_ERR_PROBE_NOT_A_FS);

  fs_reset_specific(0, 0, 4096*8, 4096, 4096, 256);
  __fs.cfg.phys_addr += 4096*6;
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), SPIFFS_ERR_PROBE_TOO_FEW_BLOCKS);

  fs_reset_specific(0, 0, 4096*8, 4096, 4096, 256);
  area_set(4096*0, 0xff, 4096); // "erase" block 0
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), 4096*8);

  fs_reset_specific(0, 0, 4096*8, 4096, 4096, 256);
  area_set(4096*0, 0xff, 4096); // "erase" block 1
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), 4096*8);

  fs_reset_specific(0, 0, 4096*8, 4096, 4096, 256);
  area_set(4096*0, 0xff, 4096); // "erase" block 2
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), 4096*8);

  fs_reset_specific(0, 0, 4096*8, 4096, 4096, 256);
  area_set(4096*0, 0xff, 4096*2); // "erase" block 0 & 1
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), SPIFFS_ERR_PROBE_NOT_A_FS);

  fs_reset_specific(0, 0, 4096*8, 4096, 4096, 256);
  area_set(4096*0, 0xff, 4096*2); // "erase" block 0
  area_set(4096*0, 0xff, 4096); // "erase" block 2
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), SPIFFS_ERR_PROBE_NOT_A_FS);

  fs_reset_specific(0, 0, 4096*8, 4096, 4096, 256);
  area_set(4096*1, 0xff, 4096*2); // "erase" block 1 & 2
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), SPIFFS_ERR_PROBE_NOT_A_FS);

  fs_reset_specific(0, 0, 4096*8, 4096, 4096, 256);
  area_set(4096*0, 0xff, 4096*8); // "erase" all
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), SPIFFS_ERR_PROBE_NOT_A_FS);

  fs_reset_specific(0, 0, 4096*8, 4096, 4096, 256);
  area_set(4096*0, 0xdd, 4096*8); // garble all
  TEST_CHECK_EQ(SPIFFS_probe_fs(&__fs.cfg), SPIFFS_ERR_PROBE_NOT_A_FS);

  return TEST_RES_OK;
}
TEST_END

#endif // SPIFFS_SINGLETON==0

#endif // SPIFFS_USE_MAGIC_LENGTH

#endif // SPIFFS_USE_MAGIC

TEST(missing_file)
{
  spiffs_file fd = SPIFFS_open(FS, "this_wont_exist", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd < 0);
  return TEST_RES_OK;
}
TEST_END


TEST(bad_fd)
{
  int res;
  spiffs_stat s;
  spiffs_file fd = SPIFFS_open(FS, "this_wont_exist", SPIFFS_RDONLY, 0);
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
TEST_END


TEST(closed_fd)
{
  int res;
  spiffs_stat s;
  res = test_create_file("file");
  spiffs_file fd = SPIFFS_open(FS, "file", SPIFFS_RDONLY, 0);
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
TEST_END


TEST(deleted_same_fd)
{
  int res;
  spiffs_stat s;
  spiffs_file fd;
  res = test_create_file("remove");
  fd = SPIFFS_open(FS, "remove", SPIFFS_RDWR, 0);
  TEST_CHECK(fd >= 0);
  fd = SPIFFS_open(FS, "remove", SPIFFS_RDWR, 0);
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
TEST_END


TEST(deleted_other_fd)
{
  int res;
  spiffs_stat s;
  spiffs_file fd, fd_orig;
  res = test_create_file("remove");
  fd_orig = SPIFFS_open(FS, "remove", SPIFFS_RDWR, 0);
  TEST_CHECK(fd_orig >= 0);
  fd = SPIFFS_open(FS, "remove", SPIFFS_RDWR, 0);
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
TEST_END


TEST(file_by_open)
{
  int res;
  spiffs_stat s;
  spiffs_file fd = SPIFFS_open(FS, "filebopen", SPIFFS_CREAT, 0);
  TEST_CHECK(fd >= 0);
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  TEST_CHECK(strcmp((char*)s.name, "filebopen") == 0);
  TEST_CHECK(s.size == 0);
  SPIFFS_close(FS, fd);

  fd = SPIFFS_open(FS, "filebopen", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd >= 0);
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  TEST_CHECK(strcmp((char*)s.name, "filebopen") == 0);
  TEST_CHECK(s.size == 0);
  SPIFFS_close(FS, fd);
  return TEST_RES_OK;
}
TEST_END


TEST(file_by_creat)
{
  int res;
  res = test_create_file("filebcreat");
  TEST_CHECK(res >= 0);
  res = SPIFFS_creat(FS, "filebcreat", 0);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS)==SPIFFS_ERR_CONFLICTING_NAME);
  return TEST_RES_OK;
}
TEST_END

TEST(file_by_open_excl)
{
  int res;
  spiffs_stat s;
  spiffs_file fd = SPIFFS_open(FS, "filebexcl", SPIFFS_CREAT | SPIFFS_EXCL, 0);
  TEST_CHECK(fd >= 0);
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  TEST_CHECK(strcmp((char*)s.name, "filebexcl") == 0);
  TEST_CHECK(s.size == 0);
  SPIFFS_close(FS, fd);

  fd = SPIFFS_open(FS, "filebexcl", SPIFFS_CREAT | SPIFFS_EXCL, 0);
  TEST_CHECK(fd < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_EXISTS);

  return TEST_RES_OK;
}
TEST_END

#if SPIFFS_FILEHDL_OFFSET
TEST(open_fh_offs)
{
  int res;
  spiffs_stat s;
  spiffs_file fd1, fd2, fd3;
  fd1 = SPIFFS_open(FS, "1", SPIFFS_CREAT | SPIFFS_EXCL, 0);
  fd2 = SPIFFS_open(FS, "2", SPIFFS_CREAT | SPIFFS_EXCL, 0);
  fd3 = SPIFFS_open(FS, "3", SPIFFS_CREAT | SPIFFS_EXCL, 0);
  TEST_CHECK(fd1 >= TEST_SPIFFS_FILEHDL_OFFSET);
  TEST_CHECK(fd2 >= TEST_SPIFFS_FILEHDL_OFFSET);
  TEST_CHECK(fd3 >= TEST_SPIFFS_FILEHDL_OFFSET);
  SPIFFS_close(FS, fd1);
  fd1 = SPIFFS_open(FS, "2", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd1 >= TEST_SPIFFS_FILEHDL_OFFSET);
  SPIFFS_close(FS, fd2);
  fd2 = SPIFFS_open(FS, "3", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd2 >= TEST_SPIFFS_FILEHDL_OFFSET);
  SPIFFS_close(FS, fd3);
  fd3 = SPIFFS_open(FS, "1", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd3 >= TEST_SPIFFS_FILEHDL_OFFSET);
  SPIFFS_close(FS, fd1);
  SPIFFS_close(FS, fd2);
  SPIFFS_close(FS, fd3);
  fd1 = SPIFFS_open(FS, "3", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd1 >= TEST_SPIFFS_FILEHDL_OFFSET);
  SPIFFS_close(FS, fd1);
  fd1 = SPIFFS_open(FS, "foo", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd1 < TEST_SPIFFS_FILEHDL_OFFSET);

  return TEST_RES_OK;
}
TEST_END

#endif //SPIFFS_FILEHDL_OFFSET

TEST(list_dir)
{
  int res;

  char *files[4] = {
      "file1",
      "file2",
      "file3",
      "file4"
  };
  int file_cnt = sizeof(files)/sizeof(char *);

  int i;

  for (i = 0; i < file_cnt; i++) {
    res = test_create_file(files[i]);
    TEST_CHECK(res >= 0);
  }

  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;

  SPIFFS_opendir(FS, "/", &d);
  int found = 0;
  while ((pe = SPIFFS_readdir(&d, pe))) {
    printf("  %s [%04x] size:%i\n", pe->name, pe->obj_id, pe->size);
    for (i = 0; i < file_cnt; i++) {
      if (strcmp(files[i], (char *)pe->name) == 0) {
        found++;
        break;
      }
    }
  }
  SPIFFS_closedir(&d);

  TEST_CHECK(found == file_cnt);

  return TEST_RES_OK;
}
TEST_END


TEST(open_by_dirent) {
  int res;

  char *files[4] = {
      "file1",
      "file2",
      "file3",
      "file4"
  };
  int file_cnt = sizeof(files)/sizeof(char *);

  int i;
  int size = SPIFFS_DATA_PAGE_SIZE(FS);

  for (i = 0; i < file_cnt; i++) {
    res = test_create_and_write_file(files[i], size, size);
    TEST_CHECK(res >= 0);
  }

  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;

  int found = 0;
  SPIFFS_opendir(FS, "/", &d);
  while ((pe = SPIFFS_readdir(&d, pe))) {
    spiffs_file fd = SPIFFS_open_by_dirent(FS, pe, SPIFFS_RDWR, 0);
    TEST_CHECK(fd >= 0);
    res = read_and_verify_fd(fd, (char *)pe->name);
    TEST_CHECK(res == SPIFFS_OK);
    fd = SPIFFS_open_by_dirent(FS, pe, SPIFFS_RDWR, 0);
    TEST_CHECK(fd >= 0);
    res = SPIFFS_fremove(FS, fd);
    TEST_CHECK(res == SPIFFS_OK);
    SPIFFS_close(FS, fd);
    found++;
  }
  SPIFFS_closedir(&d);

  TEST_CHECK(found == file_cnt);

  found = 0;
  SPIFFS_opendir(FS, "/", &d);
  while ((pe = SPIFFS_readdir(&d, pe))) {
    found++;
  }
  SPIFFS_closedir(&d);

  TEST_CHECK(found == 0);

  return TEST_RES_OK;

} TEST_END


TEST(open_by_page) {
  int res;

  char *files[4] = {
      "file1",
      "file2",
      "file3",
      "file4"
  };
  int file_cnt = sizeof(files)/sizeof(char *);

  int i;
  int size = SPIFFS_DATA_PAGE_SIZE(FS);

  for (i = 0; i < file_cnt; i++) {
    res = test_create_and_write_file(files[i], size, size);
    TEST_CHECK(res >= 0);
  }

  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;

  int found = 0;
  SPIFFS_opendir(FS, "/", &d);
  while ((pe = SPIFFS_readdir(&d, pe))) {
    spiffs_file fd = SPIFFS_open_by_dirent(FS, pe, SPIFFS_RDWR, 0);
    TEST_CHECK(fd >= 0);
    res = read_and_verify_fd(fd, (char *)pe->name);
    TEST_CHECK(res == SPIFFS_OK);
    fd = SPIFFS_open_by_page(FS, pe->pix, SPIFFS_RDWR, 0);
    TEST_CHECK(fd >= 0);
    res = SPIFFS_fremove(FS, fd);
    TEST_CHECK(res == SPIFFS_OK);
    SPIFFS_close(FS, fd);
    found++;
  }
  SPIFFS_closedir(&d);

  TEST_CHECK(found == file_cnt);

  found = 0;
  SPIFFS_opendir(FS, "/", &d);
  while ((pe = SPIFFS_readdir(&d, pe))) {
    found++;
  }
  SPIFFS_closedir(&d);

  TEST_CHECK(found == 0);

  spiffs_file fd;
  // test opening a lookup page
  fd = SPIFFS_open_by_page(FS, 0, SPIFFS_RDWR, 0);
  TEST_CHECK_LT(fd, 0);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NOT_A_FILE);
  // test opening a proper page but not being object index
  fd = SPIFFS_open_by_page(FS, SPIFFS_OBJ_LOOKUP_PAGES(FS)+1, SPIFFS_RDWR, 0);
  TEST_CHECK_LT(fd, 0);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NOT_A_FILE);

  return TEST_RES_OK;
} TEST_END


static struct {
  u32_t calls;
  spiffs_fileop_type op;
  spiffs_obj_id obj_id;
  spiffs_page_ix pix;
} ucb;

void test_cb(spiffs *fs, spiffs_fileop_type op, spiffs_obj_id obj_id, spiffs_page_ix pix) {
  ucb.calls++;
  ucb.op = op;
  ucb.obj_id = obj_id;
  ucb.pix = pix;
  //printf("%4i  op:%i objid:%04x pix:%04x\n", ucb.calls, ucb.op, ucb.obj_id, ucb.pix);
}

TEST(user_callback_basic) {
  SPIFFS_set_file_callback_func(FS, test_cb);
  int res;
  memset(&ucb, 0, sizeof(ucb));
  spiffs_file fd = SPIFFS_open(FS, "foo", SPIFFS_CREAT | SPIFFS_APPEND | SPIFFS_RDWR, 0);
  TEST_CHECK_GE(fd, 0);
  TEST_CHECK_EQ(ucb.calls, 1);
  TEST_CHECK_EQ(ucb.op, SPIFFS_CB_CREATED);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK_GE(res, 0);
  TEST_CHECK_EQ(ucb.obj_id, s.obj_id);
  TEST_CHECK_EQ(ucb.pix, s.pix);

  res = SPIFFS_write(FS, fd, "howdy partner", 14);
  TEST_CHECK_GE(res, 0);
  res = SPIFFS_fflush(FS, fd);
  TEST_CHECK_GE(res, 0);
  TEST_CHECK_EQ(ucb.calls, 2);
  TEST_CHECK_EQ(ucb.op, SPIFFS_CB_UPDATED);
  TEST_CHECK_EQ(ucb.obj_id, s.obj_id);
  TEST_CHECK_EQ(ucb.pix, s.pix);

  res = SPIFFS_fremove(FS, fd);
  TEST_CHECK_GE(res, 0);
  TEST_CHECK_EQ(ucb.calls, 3);
  TEST_CHECK_EQ(ucb.op, SPIFFS_CB_DELETED);
  TEST_CHECK_EQ(ucb.obj_id, s.obj_id);
  TEST_CHECK_EQ(ucb.pix, s.pix);

  return TEST_RES_OK;
} TEST_END


TEST(user_callback_gc) {
  SPIFFS_set_file_callback_func(FS, test_cb);

  char name[32];
  int f;
  int size = SPIFFS_DATA_PAGE_SIZE(FS);
  int pages_per_block = SPIFFS_PAGES_PER_BLOCK(FS) - SPIFFS_OBJ_LOOKUP_PAGES(FS);
  int files = (pages_per_block-1)/2;
  int res;

  // fill block with files
  for (f = 0; f < files; f++) {
    sprintf(name, "file%i", f);
    res = test_create_and_write_file(name, size, 1);
    TEST_CHECK(res >= 0);
  }
  for (f = 0; f < files; f++) {
    sprintf(name, "file%i", f);
    res = read_and_verify(name);
    TEST_CHECK(res >= 0);
  }
  // remove all files in block
  for (f = 0; f < files; f++) {
    sprintf(name, "file%i", f);
    res = SPIFFS_remove(FS, name);
    TEST_CHECK(res >= 0);
  }

  memset(&ucb, 0, sizeof(ucb));
  spiffs_file fd = SPIFFS_open(FS, "foo", SPIFFS_CREAT | SPIFFS_APPEND | SPIFFS_RDWR, 0);
  TEST_CHECK_GE(fd, 0);
  TEST_CHECK_EQ(ucb.calls, 1);
  TEST_CHECK_EQ(ucb.op, SPIFFS_CB_CREATED);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK_GE(res, 0);
  TEST_CHECK_EQ(ucb.obj_id, s.obj_id);
  TEST_CHECK_EQ(ucb.pix, s.pix);

  res = SPIFFS_write(FS, fd, "howdy partner", 14);
  TEST_CHECK_GE(res, 0);
  res = SPIFFS_fflush(FS, fd);
  TEST_CHECK_GE(res, 0);
  TEST_CHECK_EQ(ucb.calls, 2);
  TEST_CHECK_EQ(ucb.op, SPIFFS_CB_UPDATED);
  TEST_CHECK_EQ(ucb.obj_id, s.obj_id);
  TEST_CHECK_EQ(ucb.pix, s.pix);

  u32_t tot, us;
  SPIFFS_info(FS, &tot, &us);

  // do a hard gc, should move our file
  res = SPIFFS_gc(FS, tot-us*2);
  TEST_CHECK_GE(res, 0);

  TEST_CHECK_GE(res, 0);
  TEST_CHECK_EQ(ucb.calls, 3);
  TEST_CHECK_EQ(ucb.op, SPIFFS_CB_UPDATED);
  TEST_CHECK_EQ(ucb.obj_id, s.obj_id);
  TEST_CHECK_NEQ(ucb.pix, s.pix);

  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK_GE(res, 0);
  TEST_CHECK_EQ(ucb.pix, s.pix);

  res = SPIFFS_fremove(FS, fd);
  TEST_CHECK_GE(res, 0);
  TEST_CHECK_EQ(ucb.calls, 4);
  TEST_CHECK_EQ(ucb.op, SPIFFS_CB_DELETED);
  TEST_CHECK_EQ(ucb.obj_id, s.obj_id);
  TEST_CHECK_EQ(ucb.pix, s.pix);

  return TEST_RES_OK;
} TEST_END


TEST(name_too_long) {
  char name[SPIFFS_OBJ_NAME_LEN*2];
  memset(name, 0, sizeof(name));
  int i;
  for (i = 0; i < SPIFFS_OBJ_NAME_LEN; i++) {
    name[i] = 'A';
  }

  TEST_CHECK_LT(SPIFFS_creat(FS, name, 0), SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NAME_TOO_LONG);

  TEST_CHECK_LT(SPIFFS_open(FS, name, SPIFFS_CREAT | SPIFFS_TRUNC, 0), SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NAME_TOO_LONG);

  TEST_CHECK_LT(SPIFFS_remove(FS, name), SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NAME_TOO_LONG);

  spiffs_stat s;
  TEST_CHECK_LT(SPIFFS_stat(FS, name, &s), SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NAME_TOO_LONG);

  TEST_CHECK_LT(SPIFFS_rename(FS, name, "a"), SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NAME_TOO_LONG);

  TEST_CHECK_LT(SPIFFS_rename(FS, "a", name), SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NAME_TOO_LONG);

  return TEST_RES_OK;
} TEST_END


TEST(rename) {
  int res;

  char *src_name = "baah";
  char *dst_name = "booh";
  char *dst_name2 = "beeh";
  int size = SPIFFS_DATA_PAGE_SIZE(FS);

  res = test_create_and_write_file(src_name, size, size);
  TEST_CHECK(res >= 0);

  res = SPIFFS_rename(FS, src_name, dst_name);
  TEST_CHECK(res >= 0);

  res = SPIFFS_rename(FS, dst_name, dst_name);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_CONFLICTING_NAME);

  res = SPIFFS_rename(FS, src_name, dst_name2);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_NOT_FOUND);

  return TEST_RES_OK;
} TEST_END


TEST(remove_single_by_path)
{
  int res;
  spiffs_file fd;
  res = test_create_file("remove");
  TEST_CHECK(res >= 0);
  res = SPIFFS_remove(FS, "remove");
  TEST_CHECK(res >= 0);
  fd = SPIFFS_open(FS, "remove", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_NOT_FOUND);

  return TEST_RES_OK;
}
TEST_END


TEST(remove_single_by_fd)
{
  int res;
  spiffs_file fd;
  res = test_create_file("remove");
  TEST_CHECK(res >= 0);
  fd = SPIFFS_open(FS, "remove", SPIFFS_RDWR, 0);
  TEST_CHECK(fd >= 0);
  res = SPIFFS_fremove(FS, fd);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);
  fd = SPIFFS_open(FS, "remove", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_NOT_FOUND);

  return TEST_RES_OK;
}
TEST_END


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
TEST_END


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
TEST_END


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
TEST_END


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
TEST_END


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
TEST_END


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
TEST_END


TEST(truncate_big_file)
{
  int size = (FS_PURE_DATA_PAGES(FS) / 2) * SPIFFS_DATA_PAGE_SIZE(FS);
  printf("  filesize %i\n", size);
  int res = test_create_and_write_file("bigfile", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("bigfile");
  TEST_CHECK(res >= 0);
  spiffs_file fd = SPIFFS_open(FS, "bigfile", SPIFFS_RDWR, 0);
  TEST_CHECK(fd > 0);
  res = SPIFFS_fremove(FS, fd);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  fd = SPIFFS_open(FS, "bigfile", SPIFFS_RDWR, 0);
  TEST_CHECK(fd < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_NOT_FOUND);

  return TEST_RES_OK;
}
TEST_END


TEST(simultaneous_write) {
  int res = SPIFFS_creat(FS, "simul1", 0);
  TEST_CHECK(res >= 0);

  spiffs_file fd1 = SPIFFS_open(FS, "simul1", SPIFFS_RDWR, 0);
  TEST_CHECK(fd1 > 0);
  spiffs_file fd2 = SPIFFS_open(FS, "simul1", SPIFFS_RDWR, 0);
  TEST_CHECK(fd2 > 0);
  spiffs_file fd3 = SPIFFS_open(FS, "simul1", SPIFFS_RDWR, 0);
  TEST_CHECK(fd3 > 0);

  u8_t data1 = 1;
  u8_t data2 = 2;
  u8_t data3 = 3;

  res = SPIFFS_write(FS, fd1, &data1, 1);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd1);
  res = SPIFFS_write(FS, fd2, &data2, 1);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd2);
  res = SPIFFS_write(FS, fd3, &data3, 1);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd3);

  spiffs_stat s;
  res = SPIFFS_stat(FS, "simul1", &s);
  TEST_CHECK(res >= 0);

  TEST_CHECK(s.size == 1);

  u8_t rdata;
  spiffs_file fd = SPIFFS_open(FS, "simul1", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);
  res = SPIFFS_read(FS, fd, &rdata, 1);
  TEST_CHECK(res >= 0);

  TEST_CHECK(rdata == data3);

  return TEST_RES_OK;
}
TEST_END


TEST(simultaneous_write_append) {
  int res = SPIFFS_creat(FS, "simul2", 0);
  TEST_CHECK(res >= 0);

  spiffs_file fd1 = SPIFFS_open(FS, "simul2", SPIFFS_RDWR | SPIFFS_APPEND, 0);
  TEST_CHECK(fd1 > 0);
  spiffs_file fd2 = SPIFFS_open(FS, "simul2", SPIFFS_RDWR | SPIFFS_APPEND, 0);
  TEST_CHECK(fd2 > 0);
  spiffs_file fd3 = SPIFFS_open(FS, "simul2", SPIFFS_RDWR | SPIFFS_APPEND, 0);
  TEST_CHECK(fd3 > 0);

  u8_t data1 = 1;
  u8_t data2 = 2;
  u8_t data3 = 3;

  res = SPIFFS_write(FS, fd1, &data1, 1);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd1);
  res = SPIFFS_write(FS, fd2, &data2, 1);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd2);
  res = SPIFFS_write(FS, fd3, &data3, 1);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd3);

  spiffs_stat s;
  res = SPIFFS_stat(FS, "simul2", &s);
  TEST_CHECK(res >= 0);

  TEST_CHECK(s.size == 3);

  u8_t rdata[3];
  spiffs_file fd = SPIFFS_open(FS, "simul2", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);
  res = SPIFFS_read(FS, fd, &rdata, 3);
  TEST_CHECK(res >= 0);

  TEST_CHECK(rdata[0] == data1);
  TEST_CHECK(rdata[1] == data2);
  TEST_CHECK(rdata[2] == data3);

  return TEST_RES_OK;
}
TEST_END

TEST(file_uniqueness)
{
  int res;
  spiffs_file fd;
  char fname[32];
  int files = ((SPIFFS_CFG_PHYS_SZ(FS) * 75) / 100) / 2 / SPIFFS_CFG_LOG_PAGE_SZ(FS);
      //(FS_PURE_DATA_PAGES(FS) / 2) - SPIFFS_PAGES_PER_BLOCK(FS)*8;
  int i;
  printf("  creating %i files\n", files);
  for (i = 0; i < files; i++) {
    char content[20];
    sprintf(fname, "file%i", i);
    sprintf(content, "%i", i);
    res = test_create_file(fname);
    TEST_CHECK(res >= 0);
    fd = SPIFFS_open(FS, fname, SPIFFS_APPEND | SPIFFS_RDWR, 0);
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
    fd = SPIFFS_open(FS, fname, SPIFFS_RDONLY, 0);
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
    fd = SPIFFS_open(FS, fname, SPIFFS_APPEND | SPIFFS_RDWR, 0);
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
    fd = SPIFFS_open(FS, fname, SPIFFS_RDONLY, 0);
    TEST_CHECK(fd >= 0);
    res = SPIFFS_read(FS, fd, ref_content, strlen(content)+1);
    TEST_CHECK(res >= 0);
    TEST_CHECK(strcmp(ref_content, content) == 0);
    SPIFFS_close(FS, fd);
  }

  return TEST_RES_OK;
}
TEST_END

int create_and_read_back(int size, int chunk) {
  char *name = "file";
  spiffs_file fd;
  s32_t res;

  u8_t *buf = malloc(size);
  memrand(buf, size);

  res = test_create_file(name);
  CHECK(res >= 0);
  fd = SPIFFS_open(FS, name, SPIFFS_APPEND | SPIFFS_RDWR, 0);
  CHECK(fd >= 0);
  res = SPIFFS_write(FS, fd, buf, size);
  CHECK(res >= 0);

  spiffs_stat stat;
  res = SPIFFS_fstat(FS, fd, &stat);
  CHECK(res >= 0);
  CHECK(stat.size == size);

  SPIFFS_close(FS, fd);

  fd = SPIFFS_open(FS, name, SPIFFS_RDONLY, 0);
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
TEST_END


TEST(read_chunk_page)
{
  TEST_CHECK(create_and_read_back(SPIFFS_DATA_PAGE_SIZE(FS)*(SPIFFS_PAGES_PER_BLOCK(FS) - SPIFFS_OBJ_LOOKUP_PAGES(FS))*2,
      SPIFFS_DATA_PAGE_SIZE(FS)) == 0);
  return TEST_RES_OK;
}
TEST_END


TEST(read_chunk_index)
{
  TEST_CHECK(create_and_read_back(SPIFFS_DATA_PAGE_SIZE(FS)*(SPIFFS_PAGES_PER_BLOCK(FS) - SPIFFS_OBJ_LOOKUP_PAGES(FS))*4,
      SPIFFS_DATA_PAGE_SIZE(FS)*(SPIFFS_PAGES_PER_BLOCK(FS) - SPIFFS_OBJ_LOOKUP_PAGES(FS))) == 0);
  return TEST_RES_OK;
}
TEST_END


TEST(read_chunk_huge)
{
  int sz = (2*(FS)->cfg.phys_size)/3;
  TEST_CHECK(create_and_read_back(sz, sz) == 0);
  return TEST_RES_OK;
}
TEST_END


TEST(read_beyond)
{
  char *name = "file";
  spiffs_file fd;
  s32_t res;
  u32_t size = SPIFFS_DATA_PAGE_SIZE(FS)*2;

  u8_t *buf = malloc(size);
  memrand(buf, size);

  res = test_create_file(name);
  CHECK(res >= 0);
  fd = SPIFFS_open(FS, name, SPIFFS_APPEND | SPIFFS_RDWR, 0);
  CHECK(fd >= 0);
  res = SPIFFS_write(FS, fd, buf, size);
  CHECK(res >= 0);

  spiffs_stat stat;
  res = SPIFFS_fstat(FS, fd, &stat);
  CHECK(res >= 0);
  CHECK(stat.size == size);

  SPIFFS_close(FS, fd);

  fd = SPIFFS_open(FS, name, SPIFFS_RDONLY, 0);
  CHECK(fd >= 0);

  u8_t *rbuf = malloc(size+10);
  res = SPIFFS_read(FS, fd, rbuf, size+10);

  SPIFFS_close(FS, fd);

  free(rbuf);
  free(buf);

  TEST_CHECK(res == size);

  return TEST_RES_OK;
}
TEST_END


TEST(bad_index_1) {
  int size = SPIFFS_DATA_PAGE_SIZE(FS)*3;
  int res = test_create_and_write_file("file", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  spiffs_file fd = SPIFFS_open(FS, "file", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  // modify object index, find object index header
  spiffs_page_ix pix;
  res = spiffs_obj_lu_find_id_and_span(FS, s.obj_id | SPIFFS_OBJ_ID_IX_FLAG, 0, 0, &pix);
  TEST_CHECK(res >= 0);

  // set object index entry 2 to a bad page, free
  u32_t addr = SPIFFS_PAGE_TO_PADDR(FS, pix) + sizeof(spiffs_page_object_ix_header) + 2 * sizeof(spiffs_page_ix);
  spiffs_page_ix bad_pix_ref = (spiffs_page_ix)-1;
  area_write(addr, (u8_t*)&bad_pix_ref, sizeof(spiffs_page_ix));

#if SPIFFS_CACHE
  // delete all cache
  spiffs_cache *cache = spiffs_get_cache(FS);
  cache->cpage_use_map = 0;
#endif

  res = read_and_verify("file");
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_INDEX_REF_FREE);

  return TEST_RES_OK;
} TEST_END


TEST(bad_index_2) {
  int size = SPIFFS_DATA_PAGE_SIZE(FS)*3;
  int res = test_create_and_write_file("file", size, size);
  TEST_CHECK(res >= 0);
  res = read_and_verify("file");
  TEST_CHECK(res >= 0);

  spiffs_file fd = SPIFFS_open(FS, "file", SPIFFS_RDONLY, 0);
  TEST_CHECK(fd > 0);
  spiffs_stat s;
  res = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res >= 0);
  SPIFFS_close(FS, fd);

  // modify object index, find object index header
  spiffs_page_ix pix;
  res = spiffs_obj_lu_find_id_and_span(FS, s.obj_id | SPIFFS_OBJ_ID_IX_FLAG, 0, 0, &pix);
  TEST_CHECK(res >= 0);

  // set object index entry 2 to a bad page, lu
  u32_t addr = SPIFFS_PAGE_TO_PADDR(FS, pix) + sizeof(spiffs_page_object_ix_header) + 2 * sizeof(spiffs_page_ix);
  spiffs_page_ix bad_pix_ref = SPIFFS_OBJ_LOOKUP_PAGES(FS)-1;
  area_write(addr, (u8_t*)&bad_pix_ref, sizeof(spiffs_page_ix));

#if SPIFFS_CACHE
  // delete all cache
  spiffs_cache *cache = spiffs_get_cache(FS);
  cache->cpage_use_map = 0;
#endif

  res = read_and_verify("file");
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_INDEX_REF_LU);

  return TEST_RES_OK;
} TEST_END


TEST(lseek_simple_modification) {
  int res;
  spiffs_file fd;
  char *fname = "seekfile";
  int i;
  int len = 4096;
  fd = SPIFFS_open(FS, fname, SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_RDWR, 0);
  TEST_CHECK(fd > 0);
  int pfd = open(make_test_fname(fname), O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
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
TEST_END


TEST(lseek_modification_append) {
  int res;
  spiffs_file fd;
  char *fname = "seekfile";
  int i;
  int len = 4096;
  fd = SPIFFS_open(FS, fname, SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_RDWR, 0);
  TEST_CHECK(fd > 0);
  int pfd = open(make_test_fname(fname), O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
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
TEST_END


TEST(lseek_modification_append_multi) {
  int res;
  spiffs_file fd;
  char *fname = "seekfile";
  int len = 1024;
  int runs = (FS_PURE_DATA_PAGES(FS) / 2) * SPIFFS_DATA_PAGE_SIZE(FS) / (len/2);

  fd = SPIFFS_open(FS, fname, SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_RDWR, 0);
  TEST_CHECK(fd > 0);
  int pfd = open(make_test_fname(fname), O_TRUNC | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
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
TEST_END


TEST(lseek_read) {
  int res;
  spiffs_file fd;
  char *fname = "seekfile";
  int len = (FS_PURE_DATA_PAGES(FS) / 2) * SPIFFS_DATA_PAGE_SIZE(FS);
  int runs = 100000;

  fd = SPIFFS_open(FS, fname, SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_RDWR, 0);
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

    res = SPIFFS_lseek(FS, fd, -((u32_t)sizeof(buf)+11), SPIFFS_SEEK_CUR);
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
TEST_END


TEST(gc_quick)
{
  char name[32];
  int f;
  int size = SPIFFS_DATA_PAGE_SIZE(FS);
  int pages_per_block=SPIFFS_PAGES_PER_BLOCK(FS) - SPIFFS_OBJ_LOOKUP_PAGES(FS);
  int files = (pages_per_block+1)/2;
  int res;

  // negative, try quick gc on clean sys
  res = SPIFFS_gc_quick(FS, 0);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_NO_DELETED_BLOCKS);

  // fill block with files
  for (f = 0; f < files; f++) {
    sprintf(name, "file%i", f);
    res = test_create_and_write_file(name, size, 1);
    TEST_CHECK(res >= 0);
  }
  for (f = 0; f < files; f++) {
    sprintf(name, "file%i", f);
    res = read_and_verify(name);
    TEST_CHECK(res >= 0);
  }
  // remove all files in block
  for (f = 0; f < files; f++) {
    sprintf(name, "file%i", f);
    res = SPIFFS_remove(FS, name);
    TEST_CHECK(res >= 0);
  }

  // do a quick gc
  res = SPIFFS_gc_quick(FS, 0);
  TEST_CHECK(res >= 0);

  // fill another block with files but two pages
  // We might have one deleted page left over from the previous gc, in case pages_per_block is odd.
  int pages_already=2*files-pages_per_block;
  int files2=(pages_per_block-pages_already+1)/2;

  for (f = 0; f < files2 - 1; f++) {
    sprintf(name, "file%i", f);
    res = test_create_and_write_file(name, size, 1);
    TEST_CHECK(res >= 0);
  }
  for (f = 0; f < files2 - 1; f++) {
    sprintf(name, "file%i", f);
    res = read_and_verify(name);
    TEST_CHECK(res >= 0);
  }
  // remove all files in block leaving two free pages in block
  for (f = 0; f < files2 - 1; f++) {
    sprintf(name, "file%i", f);
    res = SPIFFS_remove(FS, name);
    TEST_CHECK(res >= 0);
  }

  // negative, try quick gc where no fully deleted blocks exist
  res = SPIFFS_gc_quick(FS, 0);
  TEST_CHECK(res < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_NO_DELETED_BLOCKS);

  // positive, try quick gc where allowing two free pages
  res = SPIFFS_gc_quick(FS, 2);
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
}
TEST_END


TEST(write_small_file_chunks_1)
{
  int res = test_create_and_write_file("smallfile", 256, 1);
  TEST_CHECK(res >= 0);
  res = read_and_verify("smallfile");
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
}
TEST_END


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
TEST_END

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
TEST_END

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
TEST_END


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
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = LONG
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = LONG
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = LONG
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = LONG
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = LONG
      },
  };

  int res = run_file_config(sizeof(cfgs)/sizeof(cfgs[0]), &cfgs[0], 206, 5, 0);
  TEST_CHECK(res >= 0);
  return TEST_RES_OK;
}
TEST_END

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
TEST_END


TEST(long_run_config_many_small)
{
  tfile_conf cfgs[] = {
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = LONG
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = LONG
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = MODIFIED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,      .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = REWRITTEN,     .tlife = SHORT
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = NORMAL
      },
      {   .tsize = SMALL,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = SHORT
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = NORMAL
      },
      {   .tsize = EMPTY,     .ttype = UNTAMPERED,    .tlife = SHORT
      },
  };

  int res = run_file_config(sizeof(cfgs)/sizeof(cfgs[0]), &cfgs[0], 115, 6, 0);
  TEST_CHECK(res >= 0);
  return TEST_RES_OK;
}
TEST_END


TEST(long_run)
{
  tfile_conf cfgs[] = {
      {   .tsize = EMPTY,     .ttype = APPENDED,      .tlife = NORMAL
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
  u32_t clob_size = SPIFFS_CFG_PHYS_SZ(FS)/4;
  int res = test_create_and_write_file("long_clobber", clob_size, clob_size);
  TEST_CHECK(res >= 0);

  res = read_and_verify("long_clobber");
  TEST_CHECK(res >= 0);

  while (macro_runs--) {
    //printf("  ---- run %i ----\n", macro_runs);
    if ((macro_runs % 20) == 0) {
      printf(".");
      fflush(stdout);
    }
    res = run_file_config(sizeof(cfgs)/sizeof(cfgs[0]), &cfgs[0], 20, 2, 0);
    TEST_CHECK(res >= 0);
  }
  printf("\n");

  res = read_and_verify("long_clobber");
  TEST_CHECK(res >= 0);

  res = SPIFFS_check(FS);
  TEST_CHECK(res >= 0);

  return TEST_RES_OK;
}
TEST_END

SUITE_TESTS(hydrogen_tests)
  ADD_TEST(info)
#if SPIFFS_USE_MAGIC
  ADD_TEST(magic)
#if SPIFFS_USE_MAGIC_LENGTH
  ADD_TEST(magic_length)
#if SPIFFS_SINGLETON==0
  ADD_TEST(magic_length_probe)
#endif
#endif
#endif
  ADD_TEST(missing_file)
  ADD_TEST(bad_fd)
  ADD_TEST(closed_fd)
  ADD_TEST(deleted_same_fd)
  ADD_TEST(deleted_other_fd)
  ADD_TEST(file_by_open)
  ADD_TEST(file_by_creat)
  ADD_TEST(file_by_open_excl)
#if SPIFFS_FILEHDL_OFFSET
  ADD_TEST(open_fh_offs)
#endif
  ADD_TEST(list_dir)
  ADD_TEST(open_by_dirent)
  ADD_TEST(open_by_page)
  ADD_TEST(user_callback_basic)
  ADD_TEST(user_callback_gc)
  ADD_TEST(name_too_long)
  ADD_TEST(rename)
  ADD_TEST(remove_single_by_path)
  ADD_TEST(remove_single_by_fd)
  ADD_TEST(write_big_file_chunks_page)
  ADD_TEST(write_big_files_chunks_page)
  ADD_TEST(write_big_file_chunks_index)
  ADD_TEST(write_big_files_chunks_index)
  ADD_TEST(write_big_file_chunks_huge)
  ADD_TEST(write_big_files_chunks_huge)
  ADD_TEST(truncate_big_file)
  ADD_TEST(simultaneous_write)
  ADD_TEST(simultaneous_write_append)
  ADD_TEST(file_uniqueness)
  ADD_TEST(read_chunk_1)
  ADD_TEST(read_chunk_page)
  ADD_TEST(read_chunk_index)
  ADD_TEST(read_chunk_huge)
  ADD_TEST(read_beyond)
  ADD_TEST(bad_index_1)
  ADD_TEST(bad_index_2)
  ADD_TEST(lseek_simple_modification)
  ADD_TEST(lseek_modification_append)
  ADD_TEST(lseek_modification_append_multi)
  ADD_TEST(lseek_read)
  ADD_TEST(gc_quick)
  ADD_TEST(write_small_file_chunks_1)
  ADD_TEST(write_small_files_chunks_1)
  ADD_TEST(write_big_file_chunks_1)
  ADD_TEST(write_big_files_chunks_1)
  ADD_TEST(long_run_config_many_small_one_long)
  ADD_TEST(long_run_config_many_medium)
  ADD_TEST(long_run_config_many_small)
  ADD_TEST(long_run)

SUITE_END(hydrogen_tests)

