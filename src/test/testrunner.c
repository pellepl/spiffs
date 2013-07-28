/*
 * testrunner.c
 *
 *  Created on: Jun 18, 2013
 *      Author: petera
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "testrunner.h"

static struct {
  test *tests;
  test *_last_test;
  int test_count;
  void (*on_stop)(test *t);
  test_res *failed;
  test_res *failed_last;
  test_res *stopped;
  test_res *stopped_last;
} test_main;

void test_init(void (*on_stop)(test *t)) {
  test_main.on_stop = on_stop;
}

void add_test(test_f f, char *name, void (*setup)(test *t), void (*teardown)(test *t)) {
  if (f == 0) return;
  DBGT("adding test %s\n", name);
  test *t = malloc(sizeof(test));
  memset(t, 0, sizeof(test));
  t->f = f;
  strcpy(t->name, name);
  t->setup = setup;
  t->teardown = teardown;
  if (test_main.tests == 0) {
    test_main.tests = t;
  } else {
    test_main._last_test->_next = t;
  }
  test_main._last_test = t;
  test_main.test_count++;
}

static void add_res(test *t, test_res **head, test_res **last) {
  test_res *tr = malloc(sizeof(test_res));
  memset(tr,0,sizeof(test_res));
  strcpy(tr->name, t->name);
  if (*head == 0) {
    *head = tr;
  } else {
    (*last)->_next = tr;
  }
  *last = tr;
}

static void dump_res(test_res **head) {
  test_res *tr = (*head);
  while (tr) {
    test_res *next_tr = tr->_next;
    printf("  %s\n", tr->name);
    free(tr);
    tr = next_tr;
  }
}

void run_tests() {
  memset(&test_main, 0, sizeof(test_main));
  DBGT("adding suites...\n");
  add_suites();
  DBGT("%i tests added\n", test_main.test_count);
  DBGT("running tests...\n");
  int ok = 0;
  int failed = 0;
  int stopped = 0;
  test *cur_t = test_main.tests;
  int i = 1;
  while (cur_t) {
    cur_t->setup(cur_t);
    test *next_test = cur_t->_next;
    DBGT("TEST %i/%i : running test %s\n", i, test_main.test_count, cur_t->name);
    i++;
    int res = cur_t->f(cur_t);
    cur_t->teardown(cur_t);
    switch (res) {
    case TEST_RES_OK:
      ok++;
      printf("  .. ok\n");
      break;
    case TEST_RES_FAIL:
      failed++;
      printf("  .. FAILED\n");
      if (test_main.on_stop) test_main.on_stop(cur_t);
      add_res(cur_t, &test_main.failed, &test_main.failed_last);
      break;
    case TEST_RES_ASSERT:
      stopped++;
      printf("  .. ABORTED\n");
      if (test_main.on_stop) test_main.on_stop(cur_t);
      add_res(cur_t, &test_main.stopped, &test_main.stopped_last);
      break;
    }
    free(cur_t);
    cur_t = next_test;
  }
  DBGT("ran %i tests\n", test_main.test_count);
  printf("Test report, %i tests\n", test_main.test_count);
  printf("%i succeeded\n", ok);
  printf("%i failed\n", failed);
  dump_res(&test_main.failed);
  printf("%i stopped\n", stopped);
  dump_res(&test_main.stopped);
  if (ok < test_main.test_count) {
    printf("\nFAILED\n");
  } else {
    printf("\nALL TESTS OK\n");
  }
}
