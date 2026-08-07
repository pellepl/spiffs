SPIFFS_SRC := ..
SPIFFS_FILES := $(SPIFFS_SRC)/*.c

FUSE_FILES := fuse-main.c spiffs_mutex.c flash.c

ifeq (1, $(strip $(TEST)))
CFLAGS += -I$(SPIFFS_SRC)/default
CFLAGS += -I$(SPIFFS_SRC)/test
CFLAGS += -DNO_TEST
else
CFLAGS += -include spiffs_mutex.h
CFLAGS += -DSPIFFS_LOCK=spiffs_mutex_lock
CFLAGS += -DSPIFFS_UNLOCK=spiffs_mutex_unlock
endif

CFLAGS += -Wall -Werror `pkg-config fuse --cflags` -I. -I$(SPIFFS_SRC) -O2
LDFLAGS+=`pkg-config fuse --libs` -Wl,--gc-sections
VPATH = .:$(SPIFFS_SRC)

mount.spiffs: Makefile $(SPIFFS_FILES) $(FUSE_FILES)
	$(CC) $(CFLAGS) -o $@ $(SPIFFS_FILES) $(FUSE_FILES) $(LDFLAGS)
