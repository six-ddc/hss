default: all

uname_M := $(shell sh -c 'uname -m 2>/dev/null || echo not')
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

CFLAGS=-O3 -Wall
LIBS=-lreadline

ifeq ($(uname_S),Darwin)
ifeq ($(uname_M),arm64)
	# Homebrew arm64 uses /opt/homebrew as HOMEBREW_PREFIX
	CFLAGS+=-I/opt/homebrew/opt/readline/include
	LDFLAGS+=-L/opt/homebrew/opt/readline/lib
else
	CFLAGS+=-I/usr/local/opt/readline/include
	LDFLAGS+=-L/usr/local/opt/readline/lib
endif
endif

INSTALL=install
INSTALL_BIN?=/usr/local/bin
HSS_BIN=hss

main.o: main.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c main.c -o main.o

sstring.o: sstring.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c sstring.c -o sstring.o

slot.o: slot.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c slot.c -o slot.o

completion.o: completion.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c completion.c -o completion.o

executor.o: executor.c
	$(CC) $(CFLAGS) $(CPPFLAGS) -c executor.c -o executor.o

all: main.o sstring.o slot.o completion.o executor.o
	$(CC) $(CFLAGS) $(CPPFLAGS) main.o sstring.o slot.o completion.o executor.o $(LIBS) -o $(HSS_BIN) $(LDFLAGS)

install:
	@mkdir -p $(INSTALL_BIN)
	$(INSTALL) $(HSS_BIN) $(INSTALL_BIN)

clean:
	rm -rf $(HSS_BIN) *.o

.PHONY: install all clean
