# wrapper Makefile, not fully-featured

SOURCES := $(wildcard src/*.c)

TARGET_LIB = build/libctt/libctt.so build/libctt/libctt.h # build/libctt/libctt.a
TARGET_BIN = build/ctt
TARGETS = $(TARGET_LIB) $(TARGET_BIN)

all: $(TARGETS)

$(TARGETS):
	cc -o nob nob.c
	./nob

install:
	cc -o nob nob.c
	./nob install

clean:
	./nob clean

.PHONY: all clean
