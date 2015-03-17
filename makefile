CFLAGS ?= -O2 -pipe
CPPFLAGS += -pedantic -std=c99 -Wall -Werror -Wextra
DESTDIR ?= /usr/local
LDADD := -lcurses

sources := chart.c
objects := $(sources:.c=.o)
bin := chart

.PHONY: all clean install
all: $(bin)

clean:
	$(RM) $(objects) $(bin)

install: $(bin)
	install $(bin) "$(DESTDIR)/bin/$(bin)"


.SUFFIXES:
.SUFFIXES: .o .c
.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(bin): $(objects)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(objects) $(LDADD)

$(objects): makefile
