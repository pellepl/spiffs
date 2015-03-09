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

CFILES_TEST = main.c \
	test_spiffs.c \
	test_dev.c \
	test_check.c \
	test_hydrogen.c \
	test_bugreports.c \
	testsuites.c \
	testrunner.c
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

-include $(DEPENDENCIES)	   	

# compile c files
$(OBJFILES) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@${CC} $(COMPILEROPTIONS_APP) -g -c -o $@ $<

$(OBJFILES_TEST) : ${builddir}/%.o:%.c
		@echo "... compile $@"
		@${CC} -g -c -o $@ $<

# make dependencies
$(DEPFILES) : ${builddir}/%.d:%.c
		@echo "... depend $@"; \
		rm -f $@; \
		${CC} $(COMPILEROPTIONS) -M $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*, ${builddir}/\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

all: mkdirs $(BINARY) 

mkdirs:
	-@${MKDIR} ${builddir}
	-@${MKDIR} test_data

test: all
		./build/$(BINARY)
	
test_failed: all
		./build/$(BINARY) _tests_fail
	
clean:
	@echo ... removing build files in ${builddir}
	@rm -f ${builddir}/*.o
	@rm -f ${builddir}/*.d
	@rm -f ${builddir}/*.elf
