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
AS = gcc $(ASSEMBLEROPTIONS)
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

FILES = main.c \
	test_spiffs.c \
	test_dev.c \
	test_check.c \
	test_hydrogen.c \
	testsuites.c \
	testrunner.c
include files.mk
INCLUDE_DIRECTIVES = -I./${sourcedir} -I./${sourcedir}/default 
COMPILEROPTIONS = $(INCLUDE_DIRECTIVES) 
		
############
#
# Tasks
#
############

vpath %.c ${sourcedir} ${sourcedir}/default 

OBJFILES = $(FILES:%.c=${builddir}/%.o)

DEPFILES = $(FILES:%.c=${builddir}/%.d)

ALLOBJFILES += $(OBJFILES)

DEPENDENCIES = $(DEPFILES) 

# link object files, create binary for flashing
$(BINARY): $(ALLOBJFILES)
	@echo "... linking"
	@${CC} $(LINKEROPTIONS) -o ${builddir}/$(BINARY) $(ALLOBJFILES) $(LIBS)

-include $(DEPENDENCIES)	   	

# compile c files
$(OBJFILES) : ${builddir}/%.o:%.c
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
	
clean:
	@echo ... removing build files in ${builddir}
	@rm -f ${builddir}/*.o
	@rm -f ${builddir}/*.d
	@rm -f ${builddir}/*.out
	@rm -f ${builddir}/*.hex
	@rm -f ${builddir}/*.elf
	@rm -f ${builddir}/*.map
	@rm -f ${builddir}/*_disasm.s
