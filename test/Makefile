CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -g \
         -I../4x4-solver/include

SOLVER_SRCS = \
    ../4x4-solver/src/tpr_util.c \
    ../4x4-solver/src/moves.c    \
    ../4x4-solver/src/cubie.c    \
    ../4x4-solver/src/center1.c  \
    ../4x4-solver/src/center2.c  \
    ../4x4-solver/src/center3.c  \
    ../4x4-solver/src/edge3.c    \
    ../4x4-solver/src/search.c

TARGET = test_tpr

all: $(TARGET)

$(TARGET): test_tpr.c $(SOLVER_SRCS)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	./$(TARGET)

runv: $(TARGET)
	./$(TARGET) -v

# Run with a specific seed: make seed SEED=0x1234
seed: $(TARGET)
	TPR_SEED=$(SEED) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run runv seed clean
