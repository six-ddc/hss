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
	$(CC) $(CFLAGS) -c main.c -o main.o

sstring.o: sstring.c
	$(CC) $(CFLAGS) -c sstring.c -o sstring.o

slot.o: slot.c
	$(CC) $(CFLAGS) -c slot.c -o slot.o

completion.o: completion.c
	$(CC) $(CFLAGS) -c completion.c -o completion.o

executor.o: executor.c
	$(CC) $(CFLAGS) -c executor.c -o executor.o

command/help.o: command/help.c
	$(CC) $(CFLAGS) -c command/help.c -o command/help.o

command/host.o: command/host.c
	$(CC) $(CFLAGS) -c command/host.c -o command/host.o

command/config.o: command/config.c
	$(CC) $(CFLAGS) -c command/config.c -o command/config.o

all: main.o sstring.o slot.o completion.o executor.o command/help.o command/host.o command/config.o
	$(CC) $(CFLAGS) $(LIBS) main.o sstring.o slot.o completion.o executor.o command/*.o -o $(HSS_BIN) $(LDFLAGS)

install:
	@mkdir -p $(INSTALL_BIN)
	$(INSTALL) $(HSS_BIN) $(INSTALL_BIN)

clean:
	rm -rf $(HSS_BIN) *.o command/*.o

.PHONY: install all clean
