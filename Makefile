CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror
CPPFLAGS += -Iinclude
LDLIBS  += -lm

.PHONY: all test clean
all: libcanfd_bittiming.a canfd-bt

libcanfd_bittiming.a: src/canfd_bittiming.o
	$(AR) rcs $@ $^

canfd-bt: examples/canfd-bt.c libcanfd_bittiming.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDLIBS)

tests/test_bittiming: tests/test_bittiming.c libcanfd_bittiming.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ $(LDLIBS)

test: tests/test_bittiming
	./tests/test_bittiming

clean:
	rm -f src/*.o libcanfd_bittiming.a canfd-bt tests/test_bittiming
