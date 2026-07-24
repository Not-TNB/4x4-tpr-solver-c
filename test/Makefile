CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -O3 -march=native \
         -I../4x4-solver/ckociemba/include \
         -I../4x4-solver/include \
         -I../include

SOLVER_SRCS = \
    ../4x4-solver/src/tpr_util.c       \
    ../4x4-solver/src/moves.c          \
    ../4x4-solver/src/cubie.c          \
    ../4x4-solver/src/center1.c        \
    ../4x4-solver/src/center2.c        \
    ../4x4-solver/src/center3.c        \
    ../4x4-solver/src/edge3.c          \
    ../4x4-solver/src/search.c         \
    ../src/alg.c                       \
    ../src/util.c

CKOCIEMBA_SRCS = \
    ../4x4-solver/ckociemba/coordcube.c          \
    ../4x4-solver/ckociemba/cubiecube.c          \
    ../4x4-solver/ckociemba/facecube.c           \
    ../4x4-solver/ckociemba/prunetable_helpers.c \
    ../4x4-solver/ckociemba/search.c

all: test_basic test_bench

test_basic: test_basic.c $(SOLVER_SRCS) $(CKOCIEMBA_SRCS)
	$(CC) $(CFLAGS) -o $@ $^

test_bench: test_bench.c $(SOLVER_SRCS) $(CKOCIEMBA_SRCS)
	$(CC) $(CFLAGS) -lm -o $@ $^

run: test_basic
	./test_basic

runv: test_basic
	./test_basic -v

# Run with a specific seed: make seed SEED=0x1234
seed: test_basic
	TPR_SEED=$(SEED) ./test_basic

bench: test_bench
	./test_bench

benchseed: test_bench
	TPR_SEED=$(SEED) ./test_bench

clean:
	rm -f test_basic test_bench

.PHONY: all run runv seed bench benchseed clean
