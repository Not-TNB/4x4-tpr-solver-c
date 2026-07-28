# 4x4-tpr-solver-c

C port of [cs0x7f's TPR algorithm](https://github.com/cs0x7f/TPR-4x4x4-Solver) for solving the 4×4×4 Rubik's cube.

## Algorithm

Three IDA\* phases reduce the cube to an equivalent 3×3, then [ckociemba](https://github.com/muodov/kociemba) finishes it optimally.

- **Phase 1** — orient the U/D centre axis (C(24,8) sym-reduced coordinate)
- **Phase 2** — solve R/L centres while preserving Phase 1 (C(7,4)×C(15,8) coordinate)
- **Phase 3** — pair all 12 dedges and finish remaining centres jointly (29,400 × 31M state space)
- **Phase 4** — ckociemba 3×3 finish

## Performance

Benchmarked on 2000 random 60-move scrambles (Apple M-series, `-O3 -march=native -flto`):

```
> TPR_SEED=0x12345678 ./test_bench 2000

  Time  (ms):   min      3.7   max    635.0   mean     69.5   std     52.2
  Moves:        min       42   max       51   mean     47.2

  Solved: 2000 / 2000  (100%)

  Time distribution:
    0-25ms     | ##########                     | 225   11%
    25-50ms    | ############################## | 617   30%
    50-75ms    | #######################        | 493   24%
    75-100ms   | ##############                 | 308   15%
    100-125ms  | #######                        | 156    7%
    125-150ms  | ###                            |  81    4%
    150-175ms  | #                              |  39    1%
    175-200ms  | #                              |  32    1%
    200-250ms  | #                              |  21    1%
    250-300ms  |                                |  15    0%
    300-500ms  |                                |  11    0%
    500-1000ms |                                |   2    0%
    >1000ms    |                                |   0    0%

  Moves distribution:
     42     |                                |   4    0%
     43     |                                |  11    0%
     44     | ##                             |  57    2%
     45     | ######                         | 134    6%
     46     | ################               | 338   16%
     47     | #############################  | 598   29%
     48     | ############################## | 601   30%
     49     | ##########                     | 217   10%
     50     | #                              |  38    1%
     51     |                                |   2    0%
```

God's number for the 4×4 is within the range of 35–55 moves (OBTM),
so the solution move range is satisfactory.

## Build & Run

```bash
cd test
make run          # unit + integration tests
make bench        # benchmark 10 scrambles
./test_bench N    # benchmark N scrambles (run after make bench)
```

## Explorer

An interactive 4×4 cube explorer is included. Build and run from the project root:

```bash
make
./explorer
```

**Move syntax** — standard WCA outer and wide moves:

| Notation | Meaning |
|----------|---------|
| `U R F D L B` | outer face, clockwise |
| `U' R' …` | outer face, counter-clockwise |
| `U2 R2 …` | outer face, 180° |
| `Uw Rw Fw Dw Lw Bw` | wide (2-layer), same suffixes apply |
| `(R U R')3` | repeat a sequence N times |

**Commands:**

| Command | Action |
|---------|--------|
| `solve` / `s` | solve the current cube state |
| `reset` / `r` | return to solved state |
| `facelet` / `f` | print the 3×3 facelet string for the current state |
| `q` | quit |

The solve command prints per-phase timing (P1/P2/P3/P4) and the full solution string, then applies the solution so the final state is shown.

## Using `tpr_solve` in your own project

The public API is in `4x4-solver/include/search.h`. Two calls are needed:

```c
#include "search.h"   // tpr_init, tpr_solve, tpr_set_kok_path

// Once at startup — builds all phase tables (~7 s first run, instant on
// subsequent runs once ckociemba has written its cache to disk).
tpr_set_kok_path("/absolute/path/to/4x4-solver/ckociemba/cprunetables");
tpr_init();

// Per solve — facelet96 is a 96-character string, one character per sticker,
// row-major within each face, faces in order U R F D L B.
// Valid characters: U R F D L B (one per face colour).
char solution[512];
int n = tpr_solve(facelet96, solution, sizeof(solution));
// n  >= 0: success; solution[] holds the full move string, e.g. "U Rw2 F' … R U'"
// n  == -1: no solution found (invalid or unsolvable cube)
```

**Facelet string format** — 96 characters, faces in order U R F D L B, each face
read row-major (top-left → bottom-right across a 4×4 grid).  The character for
each sticker is the face letter of its home colour: `U`=white, `R`=red, `F`=green,
`D`=yellow, `L`=orange, `B`=blue (or whichever colour scheme you map).

**ckociemba cache** — `tpr_set_kok_path` sets the directory where ckociemba reads
and writes its pruning tables (~64 MB on disk). If not called, the default path is
`../4x4-solver/ckociemba/cprunetables` (correct when CWD is `test/`). Call it with
an absolute path when the working directory is not predictable.

**Compile** — include all sources listed in the root `Makefile` under `SOLVER_SRCS`
and `CKOCIEMBA_SRCS`, add `-I 4x4-solver/ckociemba/include -I 4x4-solver/include`,
and link with `-lm`. The `-flto -O3 -march=native` flags are recommended for
performance.