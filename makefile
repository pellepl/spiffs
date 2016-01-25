BINARY = linux_spiffs_test

############
#
# Paths
#
############

sourcedir = src
builddir = build


#############
#
# Build tools
#
#############

CC = gcc $(COMPILEROPTIONS)
LD = ld
GDB = gdb
OBJCOPY = objcopy
OBJDUMP = objdump
MKDIR = mkdir -p

###############
#
# Files and libs
#
###############

NO_TEST ?= 0
CFLAGS = $(FLAGS)
ifeq (1, $(strip $(NO_TEST)))
CFILES_TEST = main.c
CFLAGS += -DNO_TEST
else
CFILES_TEST = main.c \
	test_spiffs.c \
	test_dev.c \
	test_check.c \
	test_hydrogen.c \
	test_bugreports.c \
	testsuites.c \
	testrunner.c
endif
include files.mk
INCLUDE_DIRECTIVES = -I./${sourcedir} -I./${sourcedir}/default -I./${sourcedir}/test 
COMPILEROPTIONS = $(INCLUDE_DIRECTIVES)

COMPILEROPTIONS_APP = \
-Wall -Wno-format-y2k -W -Wstrict-prototypes -Wmissing-prototypes \
-Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch \
-Wshadow -Wcast-align -Wchar-subscripts -Winline -Wnested-externs\
-Wredundant-decls
		
############
#
# Tasks
#
############

vpath %.c ${sourcedir} ${sourcedir}/default ${sourcedir}/test

OBJFILES = $(CFILES:%.c=${builddir}/%.o)
OBJFILES_TEST = $(CFILES_TEST:%.c=${builddir}/%.o)

DEPFILES = $(CFILES:%.c=${builddir}/%.d) $(CFILES_TEST:%.c=${builddir}/%.d)

ALLOBJFILES += $(OBJFILES) $(OBJFILES_TEST)

DEPENDENCIES = $(DEPFILES) 

# link object files, create binary
$(BINARY): $(ALLOBJFILES)
	@echo "... linking"
	@${CC} $(LINKEROPTIONS) -o ${builddir}/$(BINARY) $(ALLOBJFILES) $(LIBS)
ifeq (1, $(strip $(NO_TEST)))
	@echo "size: `du -b ${builddir}/${BINARY} | sed 's/\([0-9]*\).*/\1/g '` bytes"
endif


-include $(DEPENDENCIES)	   	

# compile c files
$(OBJFILES) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@${CC} $(COMPILEROPTIONS_APP) $(CFLAGS) -g -c -o $@ $<

$(OBJFILES_TEST) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@${CC} $(CFLAGS) -g -c -o $@ $<

# make dependencies
#		@echo "... depend $@"; 
$(DEPFILES) : ${builddir}/%.d:%.c
		@rm -f $@; \
		${CC} $(COMPILEROPTIONS) -M $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*, ${builddir}/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

all: mkdirs $(BINARY) 

mkdirs:
	-@${MKDIR} ${builddir}
	-@${MKDIR} test_data

FILTER ?=

test: $(BINARY)
ifdef $(FILTER)
		./build/$(BINARY)
else
		./build/$(BINARY) -f $(FILTER)
endif

test_failed: $(BINARY)
		./build/$(BINARY) _tests_fail
	
clean:
	@echo ... removing build files in ${builddir}
	@rm -f ${builddir}/*.o
	@rm -f ${builddir}/*.d
	@rm -f ${builddir}/*.elf
	
build-all:
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=0 -DSPIFFS_SINGLETON=0 -DSPIFFS_CACHE=0 -DSPIFFS_READ_ONLY=0" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=0 -DSPIFFS_SINGLETON=0 -DSPIFFS_CACHE=1 -DSPIFFS_READ_ONLY=0" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=0 -DSPIFFS_SINGLETON=1 -DSPIFFS_CACHE=0 -DSPIFFS_READ_ONLY=0" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=0 -DSPIFFS_SINGLETON=1 -DSPIFFS_CACHE=1 -DSPIFFS_READ_ONLY=0" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=1 -DSPIFFS_SINGLETON=0 -DSPIFFS_CACHE=0 -DSPIFFS_READ_ONLY=0" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=1 -DSPIFFS_SINGLETON=0 -DSPIFFS_CACHE=1 -DSPIFFS_READ_ONLY=0" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=1 -DSPIFFS_SINGLETON=1 -DSPIFFS_CACHE=0 -DSPIFFS_READ_ONLY=0" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=1 -DSPIFFS_SINGLETON=1 -DSPIFFS_CACHE=1 -DSPIFFS_READ_ONLY=0" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=0 -DSPIFFS_SINGLETON=0 -DSPIFFS_CACHE=0 -DSPIFFS_READ_ONLY=1" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=0 -DSPIFFS_SINGLETON=0 -DSPIFFS_CACHE=1 -DSPIFFS_READ_ONLY=1" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=0 -DSPIFFS_SINGLETON=1 -DSPIFFS_CACHE=0 -DSPIFFS_READ_ONLY=1" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=0 -DSPIFFS_SINGLETON=1 -DSPIFFS_CACHE=1 -DSPIFFS_READ_ONLY=1" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=1 -DSPIFFS_SINGLETON=0 -DSPIFFS_CACHE=0 -DSPIFFS_READ_ONLY=1" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=1 -DSPIFFS_SINGLETON=0 -DSPIFFS_CACHE=1 -DSPIFFS_READ_ONLY=1" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=1 -DSPIFFS_SINGLETON=1 -DSPIFFS_CACHE=0 -DSPIFFS_READ_ONLY=1" NO_TEST=1
	$(MAKE) clean && $(MAKE) FLAGS="-DSPIFFS_HAL_CALLBACK_EXTRA=1 -DSPIFFS_SINGLETON=1 -DSPIFFS_CACHE=1 -DSPIFFS_READ_ONLY=1" NO_TEST=1
	