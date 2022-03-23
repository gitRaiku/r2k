all: client server

.PHONY: build client server log

CC = gcc
DATE := $(shell date "+%Y-%m-%d")
COMPILE_FLAGS = -Og -g -ggdb3 -march=native -mtune=native -Wall -D_FORTIFY_SOURCE=2 # -fmodulo-sched
INCLUDE_FLAGS = -I/usr/X11R6/include -I/usr/include/freetype2 
LIBRARY_FLAGS = -L/usr/X11R6/lib -lXft -lX11 -lfontconfig

log:
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/log.o src/log.c 

client: log
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/r2k.o src/r2k.c 
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -o r2k src/r2k.o src/log.o

server: log
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/daemon.o src/daemon.c 
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -c -o src/dict.o src/dict.c
	$(CC) $(COMPILE_FLAGS) $(INCLUDE_FLAGS) $(LIBRARY_FLAGS) -o r2kd src/daemon.o src/dict.o src/log.o

install: client server
	cp -f r2k /usr/local/bin/r2k
	cp -f r2kd /usr/local/bin/r2kd
	cp -f argToKp /usr/local/bin/argToKp

uninstall:
	rm -f /usr/local/bin/r2k
	rm -f /usr/local/bin/r2kd
	rm -f /usr/local/bin/argToKp
