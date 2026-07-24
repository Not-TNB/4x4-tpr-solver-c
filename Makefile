CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra \
          -I 4x4-solver/ckociemba/include \
          -I 4x4-solver/include \
          -I cube/include
AR      = ar
ARFLAGS = rcs

SRCS    = cube/src/alg.c cube/src/cube4.c cube/src/util.c
OBJS    = $(SRCS:.c=.o)
DEPS    = $(OBJS:.o=.d)
LIB     = libsolver.a
BIN     = explorer

SOLVER_SRCS = \
    4x4-solver/src/tpr_util.c       \
    4x4-solver/src/moves.c          \
    4x4-solver/src/cubie.c          \
    4x4-solver/src/center1.c        \
    4x4-solver/src/center2.c        \
    4x4-solver/src/center3.c        \
    4x4-solver/src/edge3.c          \
    4x4-solver/src/search.c

CKOCIEMBA_SRCS = \
    4x4-solver/ckociemba/coordcube.c          \
    4x4-solver/ckociemba/cubiecube.c          \
    4x4-solver/ckociemba/facecube.c           \
    4x4-solver/ckociemba/prunetable_helpers.c \
    4x4-solver/ckociemba/search.c

.PHONY: all clean compdb

all: $(LIB) $(BIN)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(BIN): explorer.c $(LIB) $(SOLVER_SRCS) $(CKOCIEMBA_SRCS)
	$(CC) $(CFLAGS) -o $@ $^ -lm

%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

-include $(DEPS)

compdb:
	bear -- $(MAKE) clean all

clean:
	rm -f $(OBJS) $(DEPS) $(LIB) $(BIN)
