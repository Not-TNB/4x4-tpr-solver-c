# 4x4-tpr-solver-c

C port of [cs0x7f's TPR algorithm](https://github.com/cs0x7f/TPR-4x4x4-Solver) for solving the 4×4×4 Rubik's cube.

## Algorithm

Three IDA\* phases reduce the cube to an equivalent 3×3, then [ckociemba](https://github.com/muodov/kociemba) finishes it optimally.

- **Phase 1** — orient the U/D centre axis (C(24,8) sym-reduced coordinate)
- **Phase 2** — solve R/L centres while preserving Phase 1 (C(7,4)×C(15,8) coordinate)
- **Phase 3** — pair all 12 dedges and finish remaining centres jointly (29,400 × 31M state space)
- **Phase 4** — ckociemba 3×3 finish

## Build & run

```bash
cd test
make run          # unit + integration tests
make bench        # benchmark 10 scrambles
./test_bench N    # benchmark N scrambles (run after make bench)
```

## Performance

Benchmarked on 1000 random 60-move scrambles (Apple M-series, `-O3 -march=native -flto`):

```
Time  (ms):   min      3.4   max  32334.8   mean    403.9   std   1546.5
Moves:        min       38   max       49   mean     46.4

Time distribution:
  <10ms     | #                              |  12    1%
  10-50ms   | ############################## | 299   29%
  50-100ms  | ##########################     | 266   26%
  100-200ms | ##################             | 180   18%
  200-300ms | ####                           |  44    4%
  300-500ms | #####                          |  59    5%
  0.5-1s    | #######                        |  77    7%
  1-2s      | ##                             |  27    2%
  2-5s      | ##                             |  22    2%
  5-10s     |                                |   9    0%
  10-30s    |                                |   4    0%
  30-60s    |                                |   1    0%
  >60s      |                                |   0    0%

Moves distribution:
   38     |                                |   1    0%
   39     |                                |   1    0%
   40     |                                |   0    0%
   41     |                                |   0    0%
   42     |                                |   0    0%
   43     | #                              |  17    1%
   44     | #####                          |  55    5%
   45     | #############                  | 138   13%
   46     | ############################   | 299   29%
   47     | ############################## | 316   31%
   48     | #############                  | 144   14%
   49     | ##                             |  29    2%
```

God's number for the 4×4 is within the range of 35–55 moves (OBTM),
so the solution move range is satisfactory.
