# SPIFFS (SPI Flash File System) 
**V0.3.3**

Copyright (c) 2013-2015 Peter Andersson (pelleplutt1976 at gmail.com)

For legal stuff, see [LICENCE](https://github.com/pellepl/spiffs/blob/master/LICENSE). Basically, you may do whatever you want with the source. Use, modify, sell, print it out, roll it and smoke it - as long as I won't be held responsible.

Love to hear feedback though!


## INTRODUCTION

Spiffs is a file system intended for SPI NOR flash devices on embedded targets.

Spiffs is designed with following characteristics in mind:
 - Small (embedded) targets, sparse RAM without heap
 - Only big areas of data (blocks) can be erased
 - An erase will reset all bits in block to ones
 - Writing pulls one to zeroes
 - Zeroes can only be pulled to ones by erase
 - Wear leveling


## FEATURES

What spiffs does:
 - Specifically designed for low ram usage
 - Uses statically sized ram buffers, independent of number of files
 - Posix-like api: open, close, read, write, seek, stat, etc
 - It can be run on any NOR flash, not only SPI flash - theoretically also on embedded flash of an microprocessor
 - Multiple spiffs configurations can be run on same target - and even on same SPI flash device
 - Implements static wear leveling 
 - Built in file system consistency checks
 
What spiffs does not:
 - Presently, spiffs does not support directories. It produces a flat structure. Creating a file with path *tmp/myfile.txt* will create a file called *tmp/myfile.txt* instead of a *myfile.txt* under directory *tmp*. 
 - It is not a realtime stack. One write operation might take much longer than another.
 - Poor scalability. Spiffs is intended for small memory devices - the normal sizes for SPI flashes. Going beyond ~128MB is probably a bad idea. This is a side effect of the design goal to use as little ram as possible.
 - Presently, it does not detect or handle bad blocks.

 
## MORE INFO 
 
See the [wiki](https://github.com/pellepl/spiffs/wiki) for configuring, integrating and using spiffs.
 
For design, see [docs/TECH_SPEC](https://github.com/pellepl/spiffs/blob/master/docs/TECH_SPEC).

## HISTORY

### 0.3.3
**Might not be compatible with 0.3.2 structures. See issue #40**
- Possibility to add integer offset to file handles
- Truncate function presumes too few free pages #49
- Bug in truncate function #48 (thanks @PawelDefee)
- Update spiffs_gc.c - remove unnecessary parameter (thanks @PawelDefee)
- Update INTEGRATION docs (thanks @PawelDefee)
- Fix pointer truncation in 64-bit platforms (thanks @igrr)
- Zero-sized files cannot be read #44 (thanks @rojer)
- (More) correct calculation of max_id in obj_lu_find #42 #41 (thanks @lishen2)
- Check correct error code in obj_lu_find_free #41 (thanks @lishen2)
- Moar comments for SPIFFS_lseek (thanks @igrr)
- Fixed padding in spiffs_page_object_ix #40 (thanks @jmattsson @lishen2)
- Fixed gc_quick test (thanks @jmattsson)
- Add SPIFFS_EXCL flag #36 
- SPIFFS_close may fail silently if cache is enabled #37 
- User data in callbacks #34
- Ignoring SINGLETON build in cache setup (thanks Luca)
- Compilation error fixed #32 (thanks @chotasanjiv)
- Align cand_scores (thanks @hefloryd)
- Fix build warnings when SPIFFS_CACHE is 0 (thanks @ajaybhargav)

New config defines:
- `SPIFFS_FILEHDL_OFFSET`

### 0.3.2
- Limit cache size if too much cache is given (thanks pgeiem)
- New feature - Controlled erase. #23
- SPIFFS_rename leaks file descriptors #28 (thanks benpicco)
- moved dbg print defines in test framework to params_test.h
- lseek should return the resulting offset (thanks hefloryd)
- fixed type on dbg ifdefs
- silence warning about signed/unsigned comparison when spiffs_obj_id is 32 bit (thanks benpicco)
- Possible error in test_spiffs.c #21 (thanks yihcdaso-yeskela)
- Cache might writethrough too often #16
- even moar testrunner updates
- Test framework update and some added tests
- Some thoughts for next gen
- Test sigsevs when having too many sectors #13  (thanks alonewolfx2)
- GC might be suboptimal #11
- Fix eternal readdir when objheader at last block, last entry
  
New API functions:
- `SPIFFS_gc_quick` - call a nonintrusive gc
- `SPIFFS_gc` - call a full-scale intrusive gc

### 0.3.1
- Removed two return warnings, was too triggerhappy on release

### 0.3.0
- Added existing namecheck when creating files
- Lots of static analysis bugs #6
- Added rename func
- Fix SPIFFS_read length when reading beyond file size
- Added reading beyond file length testcase
- Made build a bit more configurable
- Changed name in spiffs from "errno" to "err_code" due to conflicts compiling in mingw
- Improved GC checks, fixed an append bug, more robust truncate for very special case
- GC checks preempts GC, truncate even less picky
- Struct alignment needed for some targets, define in spiffs config #10
- Spiffs filesystem magic, definable in config

New config defines:
- `SPIFFS_USE_MAGIC` - enable or disable magic check upon mount
- `SPIFFS_ALIGNED_OBJECT_INDEX_TABLES` - alignment for certain targets

New API functions:
- `SPIFFS_rename` - rename files
- `SPIFFS_clearerr` - clears last errno
- `SPIFFS_info` - returns info on used and total bytes in fs
- `SPIFFS_format` - formats the filesystem
- `SPIFFS_mounted` - checks if filesystem is mounted
