/*
 * test_spiffs.c
 *
 *  Created on: Jun 19, 2013
 *      Author: petera
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stypes.h"
#include "spiffs.h"
#include "spiffs_nucleus.h"

#include "testrunner.h"

#include "test_spiffs.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

#define SECTOR_SIZE 65536  //(0x10000)
#define PAGE_SIZE   256    //256
#define LOG_BLOCK   65536
#define LOG_PAGE    PAGE_SIZE

static unsigned char area[1024*1024*1];

static int erases[sizeof(area)/SECTOR_SIZE];
static char _path[256];

spiffs __fs;
static u8_t _work[LOG_PAGE*2];
static u8_t _fds[256+128];
static u8_t _cache[256*5];

#define TEST_PATH "/home/petera/proj/spiffs_test_/test_data/"

char *make_test_fname(const char *name) {
  sprintf(_path, "%s%s", TEST_PATH, name);
  return _path;
}

void clear_test_path() {
  DIR *dp;
  struct dirent *ep;
  dp = opendir(TEST_PATH);

  if (dp != NULL) {
    while ((ep = readdir(dp))) {
      if (ep->d_name[0] != '.') {
        sprintf(_path, "%s%s", TEST_PATH, ep->d_name);
        remove(_path);
      }
    }
    closedir(dp);
  }
}

static s32_t _read(u32_t addr, u32_t size, u8_t *dst) {
  memcpy(dst, &area[addr], size);
  return 0;
}

static s32_t _write(u32_t addr, u32_t size, u8_t *src) {
  int i;
  //printf("wr %08x %i\n", addr, size);
  for (i = 0; i < size; i++) {
    if (((addr + i) & (PAGE_SIZE-1)) != offsetof(spiffs_page_header, flags)) {
      if (((area[addr + i] ^ src[i]) & src[i])) {
        printf("trying to write %02x to %02x at addr %08x\n", src[i], area[addr + i], addr+i);
        spiffs_page_ix pix = (addr + i) / PAGE_SIZE;
        dump_page(&__fs, pix);
        return -1;
      }
    }
    area[addr + i] &= src[i];
  }
  return 0;
}

static s32_t _erase(u32_t addr, u32_t size) {
  if (addr & (SECTOR_SIZE-1)) {
    return -1;
  }
  if (size & (SECTOR_SIZE-1)) {
    return -1;
  }
  erases[addr/SECTOR_SIZE]++;
  memset(&area[addr], 0xff, size);
  return 0;
}

void hexdump_mem(u8_t *b, u32_t len) {
  while (len--) {
    if ((((u32_t)b)&0x1f) == 0) {
      printf("\n");
    }
    printf("%02x", *b++);
  }
  printf("\n");
}

void hexdump(u32_t addr, u32_t len) {
  int remainder = (addr % 32) == 0 ? 0 : 32 - (addr % 32);
  u32_t a;
  for (a = addr - remainder; a < addr+len; a++) {
    if ((a & 0x1f) == 0) {
      if (a != addr) {
        printf("  ");
        int j;
        for (j = 0; j < 32; j++) {
          if (a-32+j < addr)
            printf(" ");
          else {
            printf("%c", (area[a-32+j] < 32 || area[a-32+j] >= 0x7f) ? '.' : area[a-32+j]);
          }
        }
      }
      printf("%s    %08x: ", a<=addr ? "":"\n", a);
    }
    if (a < addr) {
      printf("  ");
    } else {
      printf("%02x", area[a]);
    }
  }
  int j;
  printf("  ");
  for (j = 0; j < 32; j++) {
    if (a-32+j < addr)
      printf(" ");
    else {
      printf("%c", (area[a-32+j] < 32 || area[a-32+j] >= 0x7f) ? '.' : area[a-32+j]);
    }
  }
  printf("\n");
}

void dump_page(spiffs *fs, spiffs_page_ix p) {
  printf("page %04x  ", p);
  u32_t addr = SPIFFS_PAGE_TO_PADDR(fs, p);
  if (p % SPIFFS_PAGES_PER_BLOCK(fs) < SPIFFS_OBJ_LOOKUP_PAGES(fs)) {
    // obj lu page
    printf("OBJ_LU");
  } else {
    // data page
    spiffs_page_header *ph = (spiffs_page_header *)&area[addr];
    printf("DATA %04x:%04x  ", ph->obj_id & ~SPIFFS_OBJ_ID_IX_FLAG, ph->span_ix);
    printf("%s", ((ph->flags & SPIFFS_PH_FLAG_FINAL) == 0) ? "F" : "f");
    printf("%s", ((ph->flags & SPIFFS_PH_FLAG_DELET) == 0) ? "D" : "d");
    printf("%s", ((ph->flags & SPIFFS_PH_FLAG_INDEX) == 0) ? "I" : "i");
    printf("%s  ", ((ph->flags & SPIFFS_PH_FLAG_CORRU) == 0) ? "C" : "c");
    if (ph->obj_id & SPIFFS_OBJ_ID_IX_FLAG) {
      // object index
      printf("OBJ_IX");
      if (ph->span_ix == 0) {
        printf("_HDR  ");
        spiffs_page_object_ix_header *oix_hdr = (spiffs_page_object_ix_header *)&area[addr];
        printf("'%s'  %i bytes  type:%02x", oix_hdr->name, oix_hdr->size, oix_hdr->type);
      }
    } else {
      // data page
      printf("CONTENT");
    }
  }
  printf("\n");
  u32_t len = fs->cfg.log_page_size;
  hexdump(addr, len);
}

int read_and_verify(char *name) {
  s32_t res;
  int fd = SPIFFS_open(&__fs, name, 0, 0);
  if (fd < 0) {
    printf("  read_and_verify: could not open file %s\n", name);
    return fd;
  }
  int pfd = open(make_test_fname(name), O_RDONLY);
  spiffs_stat s;
  res = SPIFFS_fstat(&__fs, fd, &s);
  if (res < 0) {
    printf("  read_and_verify: could not stat file %s\n", name);
    return res;
  }
  if (s.size == 0) {
    SPIFFS_close(&__fs, fd);
    close(pfd);
    return 0;
  }

  //printf("verifying %s, len %i\n", name, s.size);
  int offs = 0;
  u8_t buf_d[256];
  u8_t buf_v[256];
  while (offs < s.size) {
    int read_len = MIN(s.size - offs, sizeof(buf_d));
    res = SPIFFS_read(&__fs, fd, buf_d, read_len);
    if (res < 0) {
      printf("  read_and_verify: could not read file %s offs:%i len:%i filelen:%i\n", name, offs, read_len, s.size);
      return res;
    }
    int pres = read(pfd, buf_v, read_len);
    (void)pres;
    //printf("reading offs:%i len:%i spiffs_res:%i posix_res:%i\n", offs, read_len, res, pres);
    int i;
    int veri_ok = 1;
    for (i = 0; veri_ok && i < read_len; i++) {
      if (buf_d[i] != buf_v[i]) {
        printf("file verification mismatch @ %i, %02x %c != %02x %c\n", offs+i, buf_d[i], buf_d[i], buf_v[i], buf_v[i]);
        int j = MAX(0, i-16);
        int k = MIN(sizeof(buf_d), i+16);
        k = MIN(s.size-offs, k);
        int l;
        for (l = j; l < k; l++) {
          printf("%c", buf_d[l] > 31 ? buf_d[l] : '.');
        }
        printf("\n");
        for (l = j; l < k; l++) {
          printf("%c", buf_v[l] > 31 ? buf_v[l] : '.');
        }
        printf("\n");
        veri_ok = 0;
      }
    }
    if (!veri_ok) {
      SPIFFS_close(&__fs, fd);
      close(pfd);
      printf("data mismatch\n");
      return -1;
    }

    offs += read_len;
  }

  SPIFFS_close(&__fs, fd);
  close(pfd);

  return 0;
}

void area_write(u32_t addr, u8_t *buf, u32_t size) {
  int i;
  for (i = 0; i < size; i++) {
    area[addr + i] = *buf++;
  }
}

void fs_reset() {
  memset(area, 0xff, sizeof(area));
  spiffs_config c;
  c.hal_erase_f = _erase;
  c.hal_read_f = _read;
  c.hal_write_f = _write;
  c.log_block_size = LOG_BLOCK;
  c.log_page_size = LOG_PAGE;
  c.phys_addr = 0;
  c.phys_erase_block = SECTOR_SIZE;
  c.phys_size = sizeof(area);
  memset(erases,0,sizeof(erases));
  memset(_cache,0,sizeof(_cache));
  SPIFFS_mount(&__fs, &c, _work, _fds, sizeof(_fds), _cache, sizeof(_cache));
}

void real_assert(int c, const char *n, const char *file, int l) {
  if (c == 0) {
    printf("ASSERT: %s %s @ %i\n", (n ? n : ""), file, l);
    printf("fs errno:%i\n", __fs.errno);
    exit(0);
  }
}

