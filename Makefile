CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -I include
AR      = ar
ARFLAGS = rcs

SRCS    = src/alg.c src/cube4.c src/util.c
OBJS    = $(SRCS:.c=.o)
DEPS    = $(OBJS:.o=.d)
LIB     = libsolver.a
BIN     = explorer

.PHONY: all clean compdb

all: $(LIB) $(BIN)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(BIN): explorer.c $(LIB)
	$(CC) $(CFLAGS) -o $@ $< -L. -lsolver

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEPS)

compdb:
	bear -- $(MAKE) clean all

clean:
	rm -f $(OBJS) $(DEPS) $(LIB) $(BIN)
