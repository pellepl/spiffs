#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#include <errno.h>
#include <fuse.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "flash.h"
#include "spiffs.h"
#include "spiffs_mutex.h"

#define LOG_PAGE_SIZE FLASH_PAGE_SIZE
static u8_t spiffs_work_buf[LOG_PAGE_SIZE*2];
static u8_t spiffs_fds[32*4];
static u8_t spiffs_cache_buf[(LOG_PAGE_SIZE+32)*4];

static spiffs spi_fs;

static int getattr_callback(const char *path, struct stat *stbuf) {
	memset(stbuf, 0, sizeof(struct stat));

	// the root directory
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
		return 0;
	}

	++path; // discard '/'

	spiffs_stat s_stat;
	if (SPIFFS_stat(&spi_fs, path, &s_stat) < 0)
		return -ENOENT;

	stbuf->st_mode = S_IFREG | 0644;
	stbuf->st_nlink = 1;
	stbuf->st_size = s_stat.size;

	return 0;
}

static int readdir_callback(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	spiffs_DIR dir;
	struct spiffs_dirent file;

	SPIFFS_opendir(&spi_fs, path, &dir);

	while (SPIFFS_readdir(&dir, &file))
		filler(buf, (char*) file.name, NULL, 0);

	SPIFFS_closedir(&dir);

	return 0;
}

static int map_flags(int flags_in) {
	int flags_out = 0;

	if (O_RDONLY == 0 || (flags_in & O_RDONLY))
		flags_out |= SPIFFS_O_RDONLY;

	if (flags_in & O_WRONLY)
		flags_out |= SPIFFS_O_WRONLY;

	if (flags_in & O_RDWR)
		flags_out |= SPIFFS_O_RDWR;

	if (flags_in & O_APPEND)
		flags_out |= SPIFFS_O_APPEND;

	if (flags_in & O_TRUNC)
		flags_out |= SPIFFS_O_TRUNC;

	if (flags_in & O_CREAT)
		flags_out |= SPIFFS_O_CREAT;

//	if (flags_in & O_DIRECT)
//		flags_out |= SPIFFS_O_DIRECT;

	if (flags_in & O_EXCL)
		flags_out |= SPIFFS_O_EXCL;

	return flags_out;
}

static int create_callback(const char *path, mode_t mode, struct fuse_file_info *fi) {
	++path; // discard '/'
	int fd = SPIFFS_open(&spi_fs, path, map_flags(fi->flags | O_CREAT), mode);

	if (fd <= 0)
		return -ENOENT;

	fi->fh = fd;

	SPIFFS_DBG("%s(%s) fd = %ld\n", __func__, path, fi->fh);

	return 0;
}

static int open_callback(const char *path, struct fuse_file_info *fi) {
	++path; // discard '/'
	int fd = SPIFFS_open(&spi_fs, path, map_flags(fi->flags), 0);

	if (fd <= 0)
		return -ENOENT;

	fi->fh = fd;

	SPIFFS_DBG("%s(%s) fd = %ld\n", __func__, path, fi->fh);

	return 0;
}

static int close_callback(const char *path, struct fuse_file_info *fi) {
	SPIFFS_close(&spi_fs, fi->fh);

	return 0;
}

static int read_callback(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

	SPIFFS_DBG("%s(%s) fd = %ld (read %zd@%zd)\n", __func__, path, fi->fh, size, offset);

	SPIFFS_lseek(&spi_fs, fi->fh, offset, SPIFFS_SEEK_SET);

	ssize_t res = SPIFFS_read(&spi_fs, fi->fh, buf, size);

	if (res < 0)
		return -EIO; // TODO: map SPIFFS error

	return res;
}

static int write_callback(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {

	SPIFFS_DBG("%s(%s) fd = %ld (write %zd@%zd)\n", __func__, path, fi->fh, size, offset);

	SPIFFS_lseek(&spi_fs, fi->fh, offset, SPIFFS_SEEK_SET);

	ssize_t res = SPIFFS_write(&spi_fs, fi->fh, (char*) buf, size);

	if (res < 0)
		return -EIO; // TODO: map SPIFFS error

	return res;
}

static int rename_callback(const char *oldpath, const char *newpath) {

	++oldpath; // discard '/'
	++newpath; // discard '/'

	SPIFFS_remove(&spi_fs, newpath); // overwrite old file

	return SPIFFS_rename(&spi_fs, oldpath, newpath);
}

static int unlink_callback(const char *path) {

	++path; // discard '/'

	SPIFFS_remove(&spi_fs, path);

	return 0;
}

static int init_spiffs(bool check_error) {
	spiffs_config cfg;
	cfg.hal_read_f  = (spiffs_read)  flash_read;
	cfg.hal_write_f = (spiffs_write) flash_write;
	cfg.hal_erase_f = (spiffs_erase) flash_erase;

#if SPIFFS_SINGLETON == 0
	// edit these values according to your configuration
	cfg.phys_size = flash_get_size();
	cfg.phys_addr = 0;
	cfg.phys_erase_block = BLOCK_32K;
	cfg.log_block_size = BLOCK_32K;
	cfg.log_page_size = FLASH_PAGE_SIZE;
#endif

	int ret = SPIFFS_mount(&spi_fs,
		&cfg,
		spiffs_work_buf,
		spiffs_fds,
		sizeof(spiffs_fds),
		spiffs_cache_buf,
		sizeof(spiffs_cache_buf),
		0);

	if (check_error && ret == SPIFFS_ERR_NOT_A_FS) {
		fprintf(stderr, "no valid fs found\n");
		return -1;
	}

	return 0;
}

static void destroy_callback(void* ctx) {
	SPIFFS_unmount(&spi_fs);
	flash_exit();
}

static struct fuse_operations fuse_operations = {
	.create = create_callback,
	.destroy = destroy_callback,
	.getattr = getattr_callback,
	.open = open_callback,
	.readdir = readdir_callback,
	.read = read_callback,
	.release = close_callback,
	.rename = rename_callback,
	.unlink = unlink_callback,
	.write = write_callback,
};

static size_t human_toi(char* s) {
	char *endp = s;
	int sh;

	size_t x = strtoumax(s, &endp, 10);
	if (endp == s)
		return 0;

	switch(*endp) {
		case 'k': sh=10; break;
		case 'M': sh=20; break;
		case 'G': sh=30; break;
		case 0: sh=0; break;
		default: return 0;
	}

	if (x > SIZE_MAX>>sh)
		return 0;

	return x << sh;
}

int main(int argc, char *argv[]) {

	const char* file = argv[1];

	spiffs_mutex_init();

	if (strncmp(argv[1], "-c", 2) == 0) {

		if (argc < 4) {
			fprintf(stderr, "too few arguments\n");
			return -1;
		}

		size_t size = human_toi(argv[2]);

		if (!size)
			return -1;

		file = argv[3];

		flash_create(file, size);
		init_spiffs(false);
		SPIFFS_format(&spi_fs);

		argc -= 3;
		argv += 3;
	} else {
		flash_open(file);

		argc -= 1;
		argv += 1;
	}

	if (init_spiffs(true)) {
		flash_exit();
		return -1;
	}

	if (argc < 2) {
		fprintf(stderr, "usage: %s [-c size] <image> <mount_point>\n", argv[0]);
		return -1;
	}

	return fuse_main(argc, argv, &fuse_operations, NULL);
}
