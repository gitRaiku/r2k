all: client server

.PHONY: build client server log

CC = gcc
DATE := $(shell date "+%Y-%m-%d")
# COMPILE_FLAGS = -Og -g -ggdb3 -march=native -mtune=native -Wall -D_FORTIFY_SOURCE=2 -fmodulo-sched
COMPILE_FLAGS = -Ofast -ggdb3 -march=native -mtune=native -Wall -D_FORTIFY_SOURCE=2 -fmodulo-sched
INCLUDE_FLAGS = -I/usr/X11R6/include -I/usr/include/freetype2 
LIBRARY_FLAGS = -L/usr/X11R6/lib -lXft -lX11 -lfontconfig -lxkbcommon -lwayland-client

KB_PROTOCOL = virtual-keyboard-unstable-v1.xml

protocol/kb-protocol.c: protocol/virtual-keyboard-unstable-v1.xml
	wayland-scanner private-code < protocol/virtual-keyboard-unstable-v1.xml > protocol/kb-protocol.c

protocol/kb-protocol.h: protocol/virtual-keyboard-unstable-v1.xml
	wayland-scanner client-header < protocol/virtual-keyboard-unstable-v1.xml > protocol/kb-protocol.h

protocol/kb-protocol.o: protocol/kb-protocol.c protocol/kb-protocol.h
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o protocol/kb-protocol.o protocol/kb-protocol.c

src/log.o: src/log.c
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/log.o src/log.c 

src/r2k.o: src/r2k.c
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/r2k.o src/r2k.c 

src/type.o: src/type.c
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/type.o src/type.c 
	
client: src/log.o src/r2k.o src/type.o protocol/kb-protocol.o
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -o r2k protocol/kb-protocol.o src/r2k.o src/type.o src/log.o 

src/daemon.o: src/daemon.c
	./update_defs
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/daemon.o src/daemon.c 

src/dict.o: src/dict.c
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/dict.o src/dict.c

server: src/log.o src/daemon.o src/dict.o
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -o r2kd src/daemon.o src/dict.o src/log.o

debug: client
	gdb -q r2k

install: client server
	cp -f r2k /usr/local/bin/r2k
	cp -f r2kd /usr/local/bin/r2kd

run: client
	./r2k

uninstall:
	rm -f /usr/local/bin/r2k
	rm -f /usr/local/bin/r2kd

clean:
	rm -f src/*.o
	rm -f protocol/*.c protocol/*.h protocol/*.o

