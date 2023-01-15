CFLAGS=-O3 -Wall -Werror -Wimplicit-fallthrough
SRCS=$(wildcard src/*.c)
HDRS=$(wildcard src/*.h)
OBJS=$(SRCS:.c=.o)
CC=clang

rvemu: $(OBJS)
	$(CC) $(CFLAGS) -lm -o $@ $^ $(LDFLAGS)

$(OBJS): $(HDRS)

clean:
	rm -f rvemu src/*.o

.PHONY: clean
