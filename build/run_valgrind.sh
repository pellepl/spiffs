#!/bin/bash
valgrind  --show-reachable=yes --track-origins=yes --leak-check=full ./linux_spiffs_test &> valgrind_output.txt

