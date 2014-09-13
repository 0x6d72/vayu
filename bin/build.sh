#!/bin/sh

gcc -Wall -Werror -pedantic -s -O3 -o vayu -lm $(find ../src -name "*.c")
