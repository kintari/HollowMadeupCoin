
ARCH:=x86_64-linux-gnu
BINDIR:=build/bin
TARGET:=$(BINDIR)/$(ARCH)/main
SOURCES:=$(wildcard src/*.c)
OBJECTS:=$(patsubst src/%.c,$(BINDIR)/$(ARCH)/%.o,$(SOURCES))

CC=gcc
CFLAGS=-g -Wall -Werror -Wno-unused-function -O0 -fPIC -DDEBUG

build: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $^

$(BINDIR)/$(ARCH)/%.o: src/%.c | $(BINDIR)/$(ARCH)
	$(CC) $(CFLAGS) -c -o $@ $<
	
$(BINDIR)/$(ARCH):
	mkdir -p $@

clean:
	-rm -fr $(BINDIR)/$(ARCH)/*

.PHONY: clean build
