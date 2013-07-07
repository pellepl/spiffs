/*
 * spiffs_check.c
 *
 *  Created on: Jul 7, 2013
 *      Author: petera
 */

#include "spiffs.h"
#include "spiffs_nucleus.h"


static s32_t spiffs_object_check_data_page(
  spiffs *fs,
  spiffs_obj_id obj_id,
  spiffs_span_ix data_spix,
  spiffs_page_ix *pix) {
  s32_t res;
  spiffs_page_ix objix_pix;
  spiffs_page_ix ref_pix;

  // calculate object index span index for given data page span index
  spiffs_span_ix objix_spix = SPIFFS_OBJ_IX_ENTRY_SPAN_IX(fs, data_spix);

  // find obj index for obj id and span index
  res = spiffs_obj_lu_find_id_and_index(fs, obj_id | SPIFFS_OBJ_ID_IX_FLAG, objix_spix, &objix_pix);
  SPIFFS_CHECK_RES(res);

  // load obj index
  res = _spiffs_rd(fs, SPIFFS_OP_T_OBJ_LU2 | SPIFFS_OP_C_READ,
            0, SPIFFS_PAGE_TO_PADDR(fs, objix_pix), SPIFFS_CFG_LOG_PAGE_SZ(fs),
            (u8_t*)fs->work);
  SPIFFS_CHECK_RES(res);

  if (objix_spix == 0) {
    // get referenced page from object index header
    spiffs_page_object_ix_header *objix_hdr = (spiffs_page_object_ix_header *)fs->work;
    ref_pix = ((spiffs_page_ix*)((void*)objix_hdr + sizeof(spiffs_page_object_ix_header)))[data_spix];
  } else {
    // get referenced page from object index
    spiffs_page_object_ix *objix = (spiffs_page_object_ix *)fs->work;
    ref_pix = ((spiffs_page_ix*)((void*)objix + sizeof(spiffs_page_object_ix)))[SPIFFS_OBJ_IX_ENTRY(fs, data_spix)];
  }

  *pix = ref_pix;

  return res;
}

static s32_t spiffs_area_check_validate(spiffs *fs, spiffs_obj_id lu_obj_id, spiffs_page_header *p_hdr,
    spiffs_page_ix cur_pix, spiffs_block_ix cur_block, int cur_entry, int *reload_lu) {
  u8_t delete_page = 0;
  s32_t res = SPIFFS_OK;
  // check validity, take actions
  if (lu_obj_id == SPIFFS_OBJ_ID_ERASED) {
      // TODO
    if (p_hdr->flags & SPIFFS_PH_FLAG_DELET) {
      print("WARNING: pix %04x deleted in lu but not on page\n", cur_pix);
      // page can be removed if not referenced by object index
      spiffs_page_ix ref_pix;
      res = spiffs_object_check_data_page(fs, p_hdr->obj_id, p_hdr->span_ix, &ref_pix);
      if (res == SPIFFS_ERR_NOT_FOUND) {
        // no object with this id, so remove page safely
        res = SPIFFS_OK;
        delete_page = 1;
        ref_pix = cur_pix+1;
      }
      SPIFFS_CHECK_RES(res);
      *reload_lu = 1;
      if (ref_pix != cur_pix) {
        delete_page = 1;
      } else {
        // TODO - page referenced by object index but deleted in lu
      }
    }
  } else if (lu_obj_id == SPIFFS_OBJ_ID_FREE) {
    // TODO
    if (p_hdr->flags != 0xff) {
      print("WARNING: pix %04x free in lu but not on page\n", cur_pix);
      // page can be removed if not referenced by object index
      spiffs_page_ix ref_pix;
      res = spiffs_object_check_data_page(fs, p_hdr->obj_id, p_hdr->span_ix, &ref_pix);
      if (res == SPIFFS_ERR_NOT_FOUND) {
        // no object with this id, so remove page safely
        res = SPIFFS_OK;
        delete_page = 1;
        ref_pix = cur_pix+1;
      }
      SPIFFS_CHECK_RES(res);
      *reload_lu = 1;
      if (ref_pix != cur_pix) {
        delete_page = 1;
      } else {
        // TODO - page referenced by object index but free in lu
      }
    }
  } else {
    // TODO
    if ((p_hdr->flags & SPIFFS_PH_FLAG_DELET) == 0) {
      print("WARNING: pix %04x busy in lu but free on page\n", cur_pix);
      delete_page = 1;
    }
    if ((p_hdr->flags & SPIFFS_PH_FLAG_FINAL)) {
      print("WARNING: pix %04x busy but not final\n", cur_pix);
      // page can be removed if not referenced by object index
      spiffs_page_ix ref_pix;
      res = spiffs_object_check_data_page(fs, p_hdr->obj_id, p_hdr->span_ix, &ref_pix);
      if (res == SPIFFS_ERR_NOT_FOUND) {
        // no object with this id, so remove page safely
        res = SPIFFS_OK;
        delete_page = 1;
        ref_pix = cur_pix+1;
      }
      SPIFFS_CHECK_RES(res);
      *reload_lu = 1;
      if (ref_pix != cur_pix) {
        delete_page = 1;
      } else {
        // TODO - page referenced by object index but not final
      }
    }
    if (p_hdr->obj_id != lu_obj_id) {
      print("WARNING: pix %04x has id %04x in lu but %04x on page\n", cur_pix, lu_obj_id, p_hdr->obj_id);
      if (p_hdr->obj_id == SPIFFS_OBJ_ID_FREE || p_hdr->obj_id == SPIFFS_OBJ_ID_ERASED) {
        delete_page = 1;
      }
    }

    if (lu_obj_id & SPIFFS_OBJ_ID_IX_FLAG) {
      // index page
      if (p_hdr->flags & SPIFFS_PH_FLAG_INDEX) {
        print("WARNING: pix %04x marked as index in lu but as data on page\n", cur_pix);
      }
    } else {
      // data page
      if ((p_hdr->flags & SPIFFS_PH_FLAG_INDEX) == 0) {
        print("WARNING: pix %04x as data in lu but as index on page\n", cur_pix);
      }
    }
  }

  if (delete_page) {
    print("FIX:     deleting page %04x\n", cur_pix);
    res = spiffs_page_delete(fs, cur_pix);
    SPIFFS_CHECK_RES(res);
  }

  return res;
}

static s32_t spiffs_area_check_v(spiffs *fs, spiffs_obj_id obj_id, spiffs_block_ix cur_block, int cur_entry,
    u32_t user_data, void *user_p) {
  s32_t res = SPIFFS_OK;
  spiffs_page_header p_hdr;
  spiffs_page_ix cur_pix = SPIFFS_OBJ_LOOKUP_ENTRY_TO_PIX(fs, cur_block, cur_entry);

  // load header
  res = _spiffs_rd(fs, SPIFFS_OP_T_OBJ_LU2 | SPIFFS_OP_C_READ,
      0, SPIFFS_PAGE_TO_PADDR(fs, cur_pix), sizeof(spiffs_page_header), (u8_t*)&p_hdr);
  SPIFFS_CHECK_RES(res);

  int reload_lu = 0;

  res = spiffs_area_check_validate(fs, obj_id, &p_hdr, cur_pix, cur_block, cur_entry, &reload_lu);

  if (reload_lu) {
    // reload lu
    u32_t entries_per_page = (SPIFFS_CFG_LOG_PAGE_SZ(fs) / sizeof(spiffs_obj_id));
    int obj_lookup_page = cur_entry / entries_per_page;
    res = _spiffs_rd(fs, SPIFFS_OP_T_OBJ_LU2 | SPIFFS_OP_C_READ,
        0, SPIFFS_BLOCK_TO_PADDR(fs, cur_block) + SPIFFS_PAGE_TO_PADDR(fs, obj_lookup_page),
        SPIFFS_CFG_LOG_PAGE_SZ(fs), fs->lu_work);
    SPIFFS_CHECK_RES(res);
  }

  if (res == SPIFFS_OK) {
    return SPIFFS_VIS_COUNTINUE;
  }
  return res;
}


// Scans all object look up. For each entry, corresponding page header is checked for validity.
// If an object index header page is found, this is checked
s32_t spiffs_area_check(spiffs *fs, u8_t check_all_objects) {
  s32_t res = SPIFFS_OK;
  s32_t entry_count = fs->block_count * SPIFFS_OBJ_LOOKUP_MAX_ENTRIES(fs);
  spiffs_block_ix cur_block = 0;
  u32_t cur_block_addr = SPIFFS_BLOCK_TO_PADDR(fs, 0);

  res = spiffs_obj_lu_find_entry_visitor(fs, 0, 0, 0, 0, spiffs_area_check_v, 0, 0, 0, 0);

  if (res == SPIFFS_VIS_END) {
    res = SPIFFS_OK;
  }

  return res;
}

// Scans all object look up for consistency within given object id. When an index is found, it is loaded
// and checked for validity against each referenced page. When an object index header is found, size is
// checked.
// When an index points to a bad page (deleted, other id, bad span index, etc), the whole area is searched
// for correct page, followed by an index update. If no such page is found, a free page is allocated and
// referenced instead meaning there will be a page hole filled with 0xff in the file.
s32_t spiffs_object_check(spiffs *fs, spiffs_obj_id obj_id, u8_t mend) {
  s32_t res = SPIFFS_OK;
  spiffs_page_ix pix;
  spiffs_page_object_ix_header objix_hdr;

  // find obj index header for obj id
  res = spiffs_obj_lu_find_id_and_index(fs, obj_id, 0, &pix);
  SPIFFS_CHECK_RES(res);

  // load header, get size
  res = _spiffs_rd(fs, SPIFFS_OP_T_OBJ_LU2 | SPIFFS_OP_C_READ,
      0, SPIFFS_PAGE_TO_PADDR(fs, pix), sizeof(spiffs_page_object_ix_header), (u8_t*)&objix_hdr);
  SPIFFS_CHECK_RES(res);

  return res;
}
