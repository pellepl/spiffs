/*
 * test_dev.c
 *
 *  Created on: Jul 14, 2013
 *      Author: petera
 */


#include "testrunner.h"
#include "test_spiffs.h"
#include "spiffs_nucleus.h"
#include "spiffs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>


SUITE(dev_tests)
void setup() {
  _setup();
}
void teardown() {
  _teardown();
}

SUITE_END(dev_tests)
