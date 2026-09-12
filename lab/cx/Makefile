CRYS = ../crystalline-math
CC   = cc
CFLAGS = -O2 -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -I. -I$(CRYS)/include

# two library variants:
#   libcrystalline.a       as shipped (+ patch #1): uint64 fast paths call native + - *
#   libcrystalline_pure.a  patch #2: CX_PURE_GEOMETRIC, fast paths compiled out -> bead algorithms only
all: cx_selftest cx_selftest_pure

cx64.o: cx64.c cx64.h
	$(CC) $(CFLAGS) -c cx64.c -o cx64.o

cx_aes.o: cx_aes.c cx_aes.h cx64.h
	$(CC) $(CFLAGS) -c cx_aes.c -o cx_aes.o

cx_dump.o: cx_dump.c cx_dump.h cx64.h
	$(CC) $(CFLAGS) -c cx_dump.c -o cx_dump.o

cx_selftest: cx_selftest.c cx64.o $(CRYS)/libcrystalline.a
	$(CC) $(CFLAGS) cx_selftest.c cx64.o $(CRYS)/libcrystalline.a -o cx_selftest

cx_selftest_pure: cx_selftest.c cx64.o $(CRYS)/libcrystalline_pure.a
	$(CC) $(CFLAGS) cx_selftest.c cx64.o $(CRYS)/libcrystalline_pure.a -o cx_selftest_pure

clean:
	rm -f cx64.o cx_selftest cx_selftest_pure

.PHONY: all clean
