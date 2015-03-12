CFLAGS ?= -O2 -pipe
CPPFLAGS += -pedantic -std=c99 -Wall -Werror -Wextra
LDADD := -lcurses

sources := chart.c
objects := $(sources:.c=.o)
bin := chart

.PHONY: all
all: $(bin)

.PHONY: clean
clean:
	$(RM) $(objects) $(bin)

.SUFFIXES:
.SUFFIXES: .o .c
.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

$(bin): $(objects)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(objects) $(LDADD)

$(objects): makefile
