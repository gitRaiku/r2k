all: client server

.PHONY: build client server log ""

# I know this makefile is pretty messy but it's old and i don't wanna rewrite it

CC = gcc
DATE := $(shell date "+%Y-%m-%d")
COMPILE_FLAGS = -Og -g -ggdb3 -march=native -mtune=native -Wall -D_FORTIFY_SOURCE=2 -fmodulo-sched
# COMPILE_FLAGS = -Ofast -ggdb3 -march=native -mtune=native -Wall -D_FORTIFY_SOURCE=2 -fmodulo-sched
INCLUDE_FLAGS = -I/usr/X11R6/include -I/usr/include/freetype2 
KB_PROTOCOL = virtual-keyboard-unstable-v1.xml
LIBRARY_FLAGS := -L/usr/X11R6/lib -lXft -lX11 -lfontconfig -lxkbcommon

REQ_XORG = $(shell grep 'TYPE_XORG' src/config.h | tail -c 2)
ifeq ($(REQ_XORG),1)
REQ_XORG = src/type-x.o
else
REQ_XORG =
endif

REQ_WL = $(shell grep 'TYPE_WL' src/config.h | tail -c 2)
ifeq ($(REQ_WL),1)
	LIBRARY_FLAGS := $(LIBRARY_FLAGS) -lwayland-client
REQ_WL = src/type-wl.o src/protocol/kb-protocol.o
else
REQ_WL = 
endif


src/protocol/kb-protocol.c: src/protocol/virtual-keyboard-unstable-v1.xml
	wayland-scanner private-code < src/protocol/virtual-keyboard-unstable-v1.xml > src/protocol/kb-protocol.c

src/protocol/kb-protocol.h: src/protocol/virtual-keyboard-unstable-v1.xml
	wayland-scanner client-header < src/protocol/virtual-keyboard-unstable-v1.xml > src/protocol/kb-protocol.h

src/protocol/kb-protocol.o: src/protocol/kb-protocol.c src/protocol/kb-protocol.h
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/protocol/kb-protocol.o src/protocol/kb-protocol.c

src/log.o: src/log.c
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/log.o src/log.c 

src/r2k.o: src/r2k.c src/protocol/kb-protocol.h
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/r2k.o src/r2k.c

src/type-x.o: src/type-x.c
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/type-x.o src/type-x.c 

src/type-wl.o: src/type-wl.c src/protocol/kb-protocol.h
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/type-wl.o src/type-wl.c

client: src/log.o src/r2k.o bin $(REQ_WL) $(REQ_XORG) 
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -o bin/r2k src/r2k.o $(REQ_WL) $(REQ_XORG) src/log.o 

src/daemon.o: src/daemon.c
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/daemon.o src/daemon.c 

src/dict.o: src/dict.c
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/dict.o src/dict.c

server: src/log.o src/daemon.o src/dict.o bin
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -o bin/r2kd src/daemon.o src/dict.o src/log.o

bin:
	mkdir -p bin

debug: client
	gdb -q r2k

install: client server
	mkdir -p /usr/local/share/r2k/
	cp -f resources/RaikuDict /usr/local/share/r2k/r2kdict
	cp -f bin/r2k /usr/local/bin/r2k
	cp -f bin/r2kd /usr/local/bin/r2kd

run: client
	./bin/r2k

uninstall:
	rm -f /usr/local/bin/r2k
	rm -f /usr/local/bin/r2kd

clean:
	rm -f src/*.o
	rm -f src/protocol/*.c src/protocol/*.h src/protocol/*.o
	rm -rf bin/

