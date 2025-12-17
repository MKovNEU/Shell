CC=gcc
CFLAGS=-Werror -g -std=gnu11

SHELL_OBJS=$(patsubst %.c,%.o,$(wildcard *.c))

ifeq ($(shell uname), Darwin)
	LEAKTEST ?= leaks --atExit --
else
	LEAKTEST ?= valgrind --leak-check=full
endif

.PHONY: all valgrind clean test

all: shell

valgrind: shell
	$(LEAKTEST) ./shell

shell-tests : %-tests: %
	env python3 tests/$*_tests.py

test: shell-tests 

clean: 
	rm -rf *.o
	rm -f shell

shell: $(SHELL_OBJS) libtokens.a
	$(CC) $(CFLAGS) -o $@ $^ -L. -ltokens

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

