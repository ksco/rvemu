CFLAGS=-O3 -Wall -Werror -Wimplicit-fallthrough
SRCS=$(wildcard src/*.c)
OBJS=$(patsubst src/%.c, obj/%.o, $(SRCS))
CC=clang

rvemu: $(OBJS)
	$(CC) $(CFLAGS) -lm -o $@ $^ $(LDFLAGS)

$(OBJS): obj/%.o: src/%.c
	@mkdir -p $$(dirname $@)
	$(CC) $(CFLAGS) -c -o $@ $^

clean:
	rm -rf rvemu obj/

.PHONY: clean
