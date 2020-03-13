prefix ?= /usr/local
bindir ?= $(prefix)/bin

CFLAGS = -Wall -Wextra

all: iotrack
%: %.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	$(RM) iotrack

install:
	install -m 755 iotrack $(bindir)/iotrack

uninstall:
	-rm $(bindir)/iotrack
