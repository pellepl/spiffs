/*
 ============================================================================
 Name        : spiffs_test_.c
 Author      : 
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stypes.h"
#include "spiffs.h"
#include "spiffs_nucleus.h"

#include "testrunner.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

int main(void) {
  run_tests();
	return EXIT_SUCCESS;
}

