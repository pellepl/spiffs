/*
 * spiffs_hydrogen.c
 *
 *  Created on: Jun 16, 2013
 *      Author: petera
 */

#include "spiffs.h"
#include "spiffs_nucleus.h"

static s32_t spiffs_fflush_cache(spiffs *fs, spiffs_file fh);

s32_t SPIFFS_creat(spiffs *fs, const char *path, spiffs_attr attr) {
  SPIFFS_LOCK(fs);
  spiffs_obj_id obj_id;
  s32_t res;

  res = spiffs_obj_lu_find_free_obj_id(fs, &obj_id);
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
  res = spiffs_object_create(fs, obj_id, (u8_t*)path, SPIFFS_TYPE_FILE, 0);
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
  SPIFFS_UNLOCK(fs);
  return 0;
}

spiffs_file SPIFFS_open(spiffs *fs, const char *path, spiffs_attr attr, spiffs_mode mode) {
  SPIFFS_LOCK(fs);

  spiffs_fd *fd;
  spiffs_page_ix pix;

  s32_t res = spiffs_fd_find_new(fs, &fd);
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);

  res = spiffs_object_find_object_index_header_by_name(fs, (u8_t*)path, &pix);
  if ((mode & SPIFFS_CREAT) == 0) {
    if (res < SPIFFS_OK) {
      spiffs_fd_return(fs, fd->file_nbr);
    }
    SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
  }

  if ((mode & SPIFFS_CREAT) && res == SPIFFS_ERR_NOT_FOUND) {
    spiffs_obj_id obj_id;
    res = spiffs_obj_lu_find_free_obj_id(fs, &obj_id);
    if (res < SPIFFS_OK) {
      spiffs_fd_return(fs, fd->file_nbr);
    }
    SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
    res = spiffs_object_create(fs, obj_id, (u8_t*)path, SPIFFS_TYPE_FILE, &pix);
    if (res < SPIFFS_OK) {
      spiffs_fd_return(fs, fd->file_nbr);
    }
    SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
    mode &= ~SPIFFS_TRUNC;
  } else {
    if (res < SPIFFS_OK) {
      spiffs_fd_return(fs, fd->file_nbr);
    }
    SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
  }
  res = spiffs_object_open_by_page(fs, pix, fd, attr, mode);
  if (res < SPIFFS_OK) {
    spiffs_fd_return(fs, fd->file_nbr);
  }
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
  if (mode & SPIFFS_TRUNC) {
    res = spiffs_object_truncate(fd, 0, 0);
    if (res < SPIFFS_OK) {
      spiffs_fd_return(fs, fd->file_nbr);
    }
    SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
  }

  fd->fdoffset = 0;

  SPIFFS_UNLOCK(fs);

  return fd->file_nbr;
}

s32_t SPIFFS_read(spiffs *fs, spiffs_file fh, void *buf, s32_t len) {
  SPIFFS_LOCK(fs);

  spiffs_fd *fd;
  s32_t res;

  res = spiffs_fd_get(fs, fh, &fd);
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);

#if SPIFFS_CACHE_WR
  spiffs_fflush_cache(fs, fh);
#endif

  if (fd->fdoffset + len >= fd->size) {
    // reading beyond file size
    s32_t avail = fd->size - fd->fdoffset;
    if (avail <= 0) {
      SPIFFS_API_CHECK_RES_UNLOCK(fs, SPIFFS_ERR_END_OF_OBJECT);
    }
    res = spiffs_object_read(fd, fd->fdoffset, avail, (u8_t*)buf);
    if (res == SPIFFS_ERR_END_OF_OBJECT) {
      fd->fdoffset += avail;
      SPIFFS_UNLOCK(fs);
      return avail;
    } else {
      SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
    }
  } else {
    // reading within file size
    res = spiffs_object_read(fd, fd->fdoffset, len, (u8_t*)buf);
    SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
  }
  fd->fdoffset += len;

  SPIFFS_UNLOCK(fs);

  return len;
}

static s32_t spiffs_hydro_write(spiffs *fs, spiffs_fd *fd, void *buf, u32_t offset, s32_t len) {
  s32_t res = SPIFFS_OK;
  s32_t remaining = len;
  if (fd->size != SPIFFS_UNDEFINED_LEN && offset < fd->size) {
    s32_t m_len = MIN(fd->size - offset, len);
    res = spiffs_object_modify(fd, offset, (u8_t *)buf, m_len);
    SPIFFS_CHECK_RES(res);
    remaining -= m_len;
    buf += m_len;
    offset += m_len;
  }
  if (remaining > 0) {
    res = spiffs_object_append(fd, offset, (u8_t *)buf, remaining);
    SPIFFS_CHECK_RES(res);
  }
  return len;

}

s32_t SPIFFS_write(spiffs *fs, spiffs_file fh, void *buf, s32_t len) {
  SPIFFS_LOCK(fs);

  spiffs_fd *fd;
  s32_t res;
  u32_t offset;

  res = spiffs_fd_get(fs, fh, &fd);
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);

  offset = fd->fdoffset;

#if SPIFFS_CACHE_WR
  if (fd->cache_page == 0) {
    // see if object id is associated with cache already
    fd->cache_page = spiffs_cache_page_get_by_fd(fs, fd);
  }
#endif
  if (fd->mode & SPIFFS_APPEND) {
    if (fd->size == SPIFFS_UNDEFINED_LEN) {
      offset = 0;
    } else {
      offset = fd->size;
    }
#if SPIFFS_CACHE_WR
    if (fd->cache_page) {
      offset = MAX(offset, fd->cache_page->offset + fd->cache_page->size);
    }
#endif
  }

  SPIFFS_DBG("SPIFFS_write %i %04x offs:%i len %i\n", fh, fd->obj_id, offset, len);

#if SPIFFS_CACHE_WR
  if ((fd->mode & SPIFFS_DIRECT) == 0) {
    if (len < SPIFFS_CFG_LOG_PAGE_SZ(fs)) {
      // small write, try to cache it
      u8_t alloc_cpage = 1;
      if (fd->cache_page) {
        // have a cached page for this fd already, check cache page boundaries
        if (offset < fd->cache_page->offset || // writing before cache
            offset > fd->cache_page->offset + fd->cache_page->size || // writing after cache
            offset + len > fd->cache_page->offset + SPIFFS_CFG_LOG_PAGE_SZ(fs)) // writing beyond cache page
        {
          // boundary violation, write back cache first and allocate new
          SPIFFS_CACHE_DBG("CACHE_WR_DUMP: dumping cache page %i for fd %i:&04x, boundary viol, offs:%i size:%i\n",
              fd->cache_page->ix, fd->file_nbr, fd->obj_id, fd->cache_page->offset, fd->cache_page->size);
          res = spiffs_hydro_write(fs, fd,
              get_cache_page(fs, get_cache(fs), fd->cache_page->ix), fd->cache_page->offset, fd->cache_page->size);
          spiffs_cache_fh_release(fs, fd->cache_page);
        } else {
          // writing within cache
          alloc_cpage = 0;
        }
      }

      if (alloc_cpage) {
        fd->cache_page = spiffs_cache_page_allocate_by_fd(fs, fd);
        if (fd->cache_page) {
          fd->cache_page->offset = offset;
          fd->cache_page->size = 0;
          SPIFFS_CACHE_DBG("CACHE_WR_ALLO: allocating cache page %i for fd %i:%04x\n",
              fd->cache_page->ix, fd->file_nbr, fd->obj_id);
        }
      }

      if (fd->cache_page) {
        u32_t offset_in_cpage = offset - fd->cache_page->offset;
        SPIFFS_CACHE_DBG("CACHE_WR_WRITE: storing to cache page %i for fd %i:%04x, offs %i:%i len %i\n",
            fd->cache_page->ix, fd->file_nbr, fd->obj_id,
            offset, offset_in_cpage, len);
        spiffs_cache *cache = get_cache(fs);
        u8_t *cpage_data = get_cache_page(fs, cache, fd->cache_page->ix);
        memcpy(&cpage_data[offset_in_cpage], buf, len);
        fd->cache_page->size = MAX(fd->cache_page->size, offset_in_cpage + len);
        fd->fdoffset += len;
        SPIFFS_UNLOCK(fs);
        return len;
      } else {
        res = spiffs_hydro_write(fs, fd, buf, offset, len);
        SPIFFS_API_CHECK_RES(fs, res);
        fd->fdoffset += len;
        SPIFFS_UNLOCK(fs);
        return res;
      }
    } else {
      // big write, no need to cache it - but first check if there is a cached write already
      if (fd->cache_page) {
        // write back cache first
        SPIFFS_CACHE_DBG("CACHE_WR_DUMP: dumping cache page %i for fd %i:%04x, big write, offs:%i size:%i\n",
            fd->cache_page->ix, fd->file_nbr, fd->obj_id, fd->cache_page->offset, fd->cache_page->size);
        res = spiffs_hydro_write(fs, fd,
            get_cache_page(fs, get_cache(fs), fd->cache_page->ix), fd->cache_page->offset, fd->cache_page->size);
        spiffs_cache_fh_release(fs, fd->cache_page);
        res = spiffs_hydro_write(fs, fd, buf, offset, len);
        SPIFFS_API_CHECK_RES(fs, res);
      }
    }
  }
#endif

  res = spiffs_hydro_write(fs, fd, buf, offset, len);
  SPIFFS_API_CHECK_RES(fs, res);
  fd->fdoffset += len;

  SPIFFS_UNLOCK(fs);

  return res;
}

s32_t SPIFFS_lseek(spiffs *fs, spiffs_file fh, s32_t offs, int whence) {
  SPIFFS_LOCK(fs);

  spiffs_fd *fd;
  s32_t res;
  res = spiffs_fd_get(fs, fh, &fd);
  SPIFFS_API_CHECK_RES(fs, res);

#if SPIFFS_CACHE_WR
  spiffs_fflush_cache(fs, fh);
#endif

  switch (whence) {
  case SPIFFS_SEEK_CUR:
    offs = fd->fdoffset+offs;
    break;
  case SPIFFS_SEEK_END:
    offs = (fd->size == SPIFFS_UNDEFINED_LEN ? 0 : fd->size) + offs;
    break;
  }

  if (offs > fd->size) {
    res = SPIFFS_ERR_END_OF_OBJECT;
  }
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);

  spiffs_span_ix data_spix = offs / SPIFFS_DATA_PAGE_SIZE(fs);
  spiffs_span_ix objix_spix = SPIFFS_OBJ_IX_ENTRY_SPAN_IX(fs, data_spix);
  if (fd->cursor_objix_spix != objix_spix) {
    spiffs_page_ix pix;
    res = spiffs_obj_lu_find_id_and_index(
        fs, fd->obj_id | SPIFFS_OBJ_ID_IX_FLAG, objix_spix, &pix);
    SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
    fd->cursor_objix_spix = objix_spix;
    fd->cursor_objix_pix = pix;
  }
  fd->fdoffset = offs;

  SPIFFS_UNLOCK(fs);

  return 0;
}

s32_t SPIFFS_remove(spiffs *fs, const char *path) {
  SPIFFS_LOCK(fs);

  spiffs_fd fd;
  spiffs_page_ix pix;
  s32_t res;
  fd.file_nbr = 0;
  fd.fs = fs;
  res = spiffs_object_find_object_index_header_by_name(fs, (u8_t*)path, &pix);
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
  res = spiffs_object_open_by_page(fs, pix, &fd, 0,0);
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);
  res = spiffs_object_truncate(&fd, 0, 1);
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);

  SPIFFS_UNLOCK(fs);
  return 0;
}

s32_t SPIFFS_fremove(spiffs *fs, spiffs_file fh) {
  SPIFFS_LOCK(fs);

  spiffs_fd *fd;
  s32_t res;
  res = spiffs_fd_get(fs, fh, &fd);
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);

#if SPIFFS_CACHE_WR
  spiffs_cache_fh_release(fs, fd->cache_page);
#endif

  res = spiffs_object_truncate(fd, 0, 1);

  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);

  SPIFFS_UNLOCK(fs);

  return 0;
}

s32_t SPIFFS_fstat(spiffs *fs, spiffs_file fh, spiffs_stat *s) {
  SPIFFS_LOCK(fs);

  spiffs_fd *fd;
  s32_t res;
  spiffs_page_object_ix_header objix_hdr;

  res = spiffs_fd_get(fs, fh, &fd);
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);

#if SPIFFS_CACHE_WR
  spiffs_fflush_cache(fs, fh);
#endif

  res = spiffs_phys_rd(fs,
#if SPIFFS_CACHE
      SPIFFS_OP_T_OBJ_IX | SPIFFS_OP_C_READ, fh,
#endif
      SPIFFS_PAGE_TO_PADDR(fs, fd->objix_hdr_pix), sizeof(spiffs_page_object_ix_header), (u8_t *)&objix_hdr);
  SPIFFS_API_CHECK_RES_UNLOCK(fs, res);

  s->obj_id = objix_hdr.p_hdr.obj_id;
  s->type = objix_hdr.type;
  s->size = objix_hdr.size == SPIFFS_UNDEFINED_LEN ? 0 : objix_hdr.size;
  strncpy((char *)s->name, (char *)objix_hdr.name, SPIFFS_OBJ_NAME_LEN);

  SPIFFS_UNLOCK(fs);

  return res;
}

static s32_t spiffs_fflush_cache(spiffs *fs, spiffs_file fh) {
  s32_t res = SPIFFS_OK;
#if SPIFFS_CACHE_WR

  spiffs_fd *fd;
  res = spiffs_fd_get(fs, fh, &fd);
  SPIFFS_API_CHECK_RES(fs, res);

  if ((fd->mode & SPIFFS_DIRECT) == 0) {
    if (fd->cache_page == 0) {
      // see if object id is associated with cache already
      fd->cache_page = spiffs_cache_page_get_by_fd(fs, fd);
    }
    if (fd->cache_page) {
      SPIFFS_CACHE_DBG("CACHE_WR_DUMP: dumping cache page %i for fd %i:%04x, flush, offs:%i size:%i\n",
          fd->cache_page->ix, fd->file_nbr,  fd->obj_id, fd->cache_page->offset, fd->cache_page->size);
      res = spiffs_hydro_write(fs, fd,
          get_cache_page(fs, get_cache(fs), fd->cache_page->ix), fd->cache_page->offset, fd->cache_page->size);
      spiffs_cache_fh_release(fs, fd->cache_page);
    }
  }
#endif

  return res;
}

s32_t SPIFFS_fflush(spiffs *fs, spiffs_file fh) {
  s32_t res = SPIFFS_OK;
#if SPIFFS_CACHE_WR
  SPIFFS_LOCK(fs);
  res = spiffs_fflush_cache(fs, fh);
  SPIFFS_API_CHECK_RES_UNLOCK(fs,res);
  SPIFFS_UNLOCK(fs);
#endif

  return res;
}

void SPIFFS_close(spiffs *fs, spiffs_file fh) {
  SPIFFS_LOCK(fs);

#if SPIFFS_CACHE
  spiffs_fflush_cache(fs, fh);
#endif
  spiffs_fd_return(fs, fh);

  SPIFFS_UNLOCK(fs);
}

spiffs_DIR *SPIFFS_opendir(spiffs *fs, const char *name, spiffs_DIR *d) {
  d->fs = fs;
  d->block = 0;
  d->entry = 0;
  return d;
}

static s32_t spiffs_read_dir_v(
    spiffs *fs,
    spiffs_obj_id obj_id,
    spiffs_block_ix bix,
    int ix_entry,
    u32_t user_data,
    void *user_p) {
  s32_t res;
  spiffs_page_object_ix_header objix_hdr;
  if (obj_id == SPIFFS_OBJ_ID_FREE || obj_id == SPIFFS_OBJ_ID_ERASED ||
      (obj_id & SPIFFS_OBJ_ID_IX_FLAG) == 0) {
    return SPIFFS_COUNTINUE;
  }

  spiffs_page_ix pix = SPIFFS_OBJ_LOOKUP_ENTRY_TO_PIX(fs, bix, ix_entry);
  res = _spiffs_rd(fs, SPIFFS_OP_T_OBJ_LU2 | SPIFFS_OP_C_READ,
      0, SPIFFS_PAGE_TO_PADDR(fs, pix), sizeof(spiffs_page_object_ix_header), (u8_t *)&objix_hdr);
  if (res != SPIFFS_OK) return res;
  if ((objix_hdr.p_hdr.obj_id & SPIFFS_OBJ_ID_IX_FLAG) &&
      objix_hdr.p_hdr.span_ix == 0 &&
      (objix_hdr.p_hdr.flags& (SPIFFS_PH_FLAG_DELET | SPIFFS_PH_FLAG_FINAL)) == SPIFFS_PH_FLAG_DELET) {
    struct spiffs_dirent *e = (struct spiffs_dirent *)user_p;
    e->obj_id = objix_hdr.p_hdr.obj_id;
    strcpy((char *)e->name, (char *)objix_hdr.name);
    e->type = objix_hdr.type;
    e->size = objix_hdr.size == SPIFFS_UNDEFINED_LEN ? 0 : objix_hdr.size;
    return SPIFFS_OK;
  }

  return SPIFFS_COUNTINUE;
}

struct spiffs_dirent *SPIFFS_readdir(spiffs_DIR *d, struct spiffs_dirent *e) {
  SPIFFS_LOCK(fs);

  spiffs_block_ix bix;
  int entry;
  s32_t res;
  struct spiffs_dirent *ret = 0;

  res = spiffs_obj_lu_find_entry_visitor(d->fs,
      d->block,
      d->entry,
      SPIFFS_VIS_NO_WRAP,
      0,
      spiffs_read_dir_v,
      0,
      e,
      &bix,
      &entry);
  if (res == SPIFFS_OK) {
    d->block = bix;
    d->entry = entry + 1;
    ret = e;
  } else {
    d->fs->errno = res;
  }
  SPIFFS_UNLOCK(fs);
  return ret;
}

s32_t SPIFFS_closedir(spiffs_DIR *d) {
  return SPIFFS_OK;
}

s32_t SPIFFS_check(spiffs *fs) {
  s32_t res;
  SPIFFS_LOCK(fs);

  res = spiffs_area_check(fs, 0);

  SPIFFS_UNLOCK(fs);
  return res;
}
