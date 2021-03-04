default: all

CFLAGS=-I/usr/local/opt/readline/include -O3 -Wall
LIBS=-lreadline

ifeq ($(shell uname), Darwin)
	LDFLAGS=-L/usr/local/opt/readline/lib
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
