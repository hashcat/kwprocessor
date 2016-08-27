##
##  Makefile for kwp
##

CFLAGS = -W -Wall -std=c99 -O2 -s
#CFLAGS = -W -Wall -std=c99 -g

CC_NATIVE         = gcc

CC_WINDOWS32      = /usr/bin/i686-w64-mingw32-gcc
CC_WINDOWS64      = /usr/bin/x86_64-w64-mingw32-gcc

CFLAGS_NATIVE     = $(CFLAGS)

CFLAGS_WINDOWS32  = $(CFLAGS) -m32 -DWINDOWS
CFLAGS_WINDOWS64  = $(CFLAGS) -m64 -DWINDOWS

all: kwp

windows: kwp32.exe kwp64.exe

clean:
	rm -f kwp kwp32.exe kwp64.exe

kwp: src/kwp.c
	$(CC_NATIVE)    $(CFLAGS_NATIVE)    -o $@ $^

kwp32.exe: src/kwp.c
	$(CC_WINDOWS32) $(CFLAGS_WINDOWS32) -o $@ $^

kwp64.exe: src/kwp.c
	$(CC_WINDOWS64) $(CFLAGS_WINDOWS64) -o $@ $^
