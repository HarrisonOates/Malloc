CC       = gcc
# https://developers.redhat.com/blog/2018/03/21/compiler-and-linker-flags-gcc
CFLAGS   = -fPIC -Wall -Wextra -Werror=format-security -Werror=implicit-function-declaration -std=gnu17 -pedantic
LIBFLAGS = -shared
MALLOC   = mymalloc
ODIR	 = ./out
LIBTESTFLAGS = -L./out

ifdef RELEASE
CFLAGS += -O3
else
CFLAGS += -g -ggdb3
endif

ifdef LOG
CFLAGS += -DENABLE_LOG
endif

ifeq ($(shell uname -s),Darwin)
# Treat 32-bit tests as 64-bit.
M32_FLAG =
DYLIB_EXT = dylib
else
M32_FLAG = -m32
DYLIB_EXT = so
endif

ALL_TESTS_SRC = $(wildcard tests/*.c)
ALL_TESTS = $(ALL_TESTS_SRC:%.c=%)

all: mymalloc mygc

mymalloc: $(MALLOC).c | $(ODIR)/
	@$(CC) $(CFLAGS) $(LIBFLAGS) -o $(ODIR)/lib$(MALLOC).$(DYLIB_EXT) $<

mygc : mygc.c | $(ODIR)/
	@$(CC) $(CFLAGS) $(LIBFLAGS) -o $(ODIR)/libmygc.$(DYLIB_EXT) $<

tests/%:  *.h tests/*.h tests/%.c | mymalloc
	@$(CC) $(CFLAGS) $(LIBTESTFLAGS) $@.c -l$(MALLOC) -o $@ -Wl,-rpath,"`pwd`"/$(ODIR)

test: $(ALL_TESTS)

bench/%: _force *.h tests/*.h bench/%.c | mymalloc
	@$(CC) $(CFLAGS) $(LIBTESTFLAGS) $@.c -l$(MALLOC) -o $@ -Wl,-rpath,"`pwd`"/$(ODIR)

mygctest : mygctest.c | mygc mymalloc
	@$(CC) $(CFLAGS) $(LIBTESTFLAGS) $@.c -lmygc -o $@ -Wl,-rpath,"`pwd`"/$(ODIR)

$(ODIR)/:
	mkdir -p $(ODIR)

clean:
	rm -rf ./out
	rm -rf ./tests/*.dSYM
	rm -rf *.dSYM
	rm -rf mygctest
	rm -f *.o $(ODIR)/*.o
	rm -f *.$(DYLIB_EXT) $(ODIR)/*.$(DYLIB_EXT)
	@for test in $(ALL_TESTS); do        \
		echo "rm $$test";            \
		rm -f $$test;      	     \
	done

_force:

.PHONY: clean _force all mymalloc mymalloc32

