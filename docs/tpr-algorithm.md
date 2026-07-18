# TPR-4x4x4 Solver — Complete Implementation Reference

Source: `TPR-4x4x4-Solver/src/` (Java).  
Algorithm: Tsai's 8-step 4×4×4 method, merged into three reduction phases followed by an optimal 3×3×3 solve.

---

## Table of Contents

1. [High-Level Pipeline](#1-high-level-pipeline)
2. [Facelet & Piece Numbering](#2-facelet--piece-numbering)
3. [Cube State Representation](#3-cube-state-representation)
4. [Move System](#4-move-system)
5. [Utility Primitives](#5-utility-primitives)
6. [Phase 1 — Center1: Orient U/D Centers](#6-phase-1--center1-orient-ud-centers)
7. [Phase 2 — Center2: Solve R/L Centers](#7-phase-2--center2-solve-rl-centers)
8. [Phase 3 — Center3 + Edge3: Pair Edges & Finish Centers](#8-phase-3--center3--edge3-pair-edges--finish-centers)
9. [Final Step — 3×3×3 Solve (min2phase)](#9-final-step--333-solve-min2phase)
10. [Search Orchestration](#10-search-orchestration)
11. [Table Sizes and Memory Budget](#11-table-sizes-and-memory-budget)
12. [Initialization Order](#12-initialization-order)
13. [C Implementation Checklist](#13-c-implementation-checklist)

---

## 1. High-Level Pipeline

```
Scrambled 4×4×4
      │
      ▼
Phase 1 (Center1)
  Goal : bring 8 U/D-colored centers to the U/D axis
  Tool : IDA* on sym-reduced C(24,8) coordinate
  Moves: all 36 (U R F D L B Uw Rw Fw Dw Lw Bw × {1,2,3})
  Depth: typically 5–9 moves
      │  stores up to 10 000 candidate solutions
      ▼
Phase 2 (Center2)
  Goal : solve R/L centers (4 RL-pieces on R face, 4 on L face)
         while preserving U/D center axis
  Tool : IDA* on ct × rl coordinate
  Moves: 28 (all outer + Uw2 Fw2 Dw2 Bw2 + all Rw Lw)
  Depth: typically 7–11 moves
      │  stores up to 100 candidate solutions
      ▼
Phase 3 (Center3 + Edge3)
  Goal : finish all centers AND pair all 24 wing edges into
         12 matched pairs (reducing to a valid 3×3×3 edge state)
  Tool : IDA* on combined Center3 + Edge3 sym coordinate
  Moves: 20 (all outer, but R/L outer only as R2/L2;
              all wide moves only as half-turns)
  Depth: typically 8–12 moves
      │
      ▼
3×3×3 solve (min2phase)
  Goal : solve corners + (now-paired) edges optimally
  Input: to333Facelet() — convert cube to standard 54-facelet string
  Depth: up to 21 moves
      │
      ▼
Combined solution string (~44 moves average)
```

---

## 2. Facelet & Piece Numbering

### 2.1 Facelet slots (96 total)

The Java code uses hex naming. In C we use integer slots 0–95:

```
Face  Slots    Hex names    Face index (slot/16)
U     0–15     u0–uf        0
R     16–31    r0–rf        1
F     32–47    f0–ff        2
D     48–63    d0–df        3
L     64–79    l0–lf        4
B     80–95    b0–bf        5
```

Face order in the Java move enum: **U=0 R=1 F=2 D=3 L=4 B=5**.

Within each face, facelets are row-major 4×4:
```
 0  1  2  3
 4  5  6  7
 8  9 10 11
12 13 14 15
```

### 2.2 The 24 center pieces

Each face has 4 center stickers (the 2×2 inner block: local slots 5,6,9,10).

```java
static final byte[] centerFacelet = {
    u5, u6, ua, u9,   // positions 0–3:  U-face centers
    d5, d6, da, d9,   // positions 4–7:  D-face centers
    f5, f6, fa, f9,   // positions 8–11: F-face centers
    b5, b6, ba, b9,   // positions 12–15: B-face centers
    r5, r6, ra, r9,   // positions 16–19: R-face centers
    l5, l6, la, l9    // positions 20–23: L-face centers
};
```

In solved state, center position `i` holds a sticker of color `centerFacelet[i] / 16`:
- positions 0–3 → color 0 (U)
- positions 4–7 → color 3 (D)
- positions 8–11 → color 2 (F)
- positions 12–15 → color 5 (B)
- positions 16–19 → color 1 (R)
- positions 20–23 → color 4 (L)

Center positions in the unfolded net (used throughout the code):
```
           0  1
           3  2

20 21    8  9   16 17   12 13
23 22   11 10   19 18   15 14

           4  5
           7  6
```
Band left-to-right order: L (20–23), F (8–11), R (16–19), B (12–15).

### 2.3 The 24 wing-edge pieces

Each of the 12 3×3 edges has 2 wing stickers. Naming convention: `ep[i]` for i in 0–23.

```java
static int[][] EdgeColor = {
    {F,U},{L,U},{B,U},{R,U},  // 0–3:  U-layer wings (UF,UL,UB,UR)
    {B,D},{L,D},{F,D},{R,D},  // 4–7:  D-layer wings (DB,DL,DF,DR)
    {F,L},{B,L},{B,R},{F,R}   // 8–11: E-slice wings (FL,BL,BR,FR)
};
// positions 0–11: "A" wing of each edge pair
// positions 12–23: "B" wing of each edge pair (same edges, second wing)
```

In solved state `ep[i] = i`.  
A solved edge pair i has both `ep[i] = i` and `ep[i+12] = i+12` (or some consistent pairing).

```java
static int[] EdgeMap = {F2,L2,B2,R2,B8,L8,F8,R8,F4,B6,B4,F6,
                         U8,U4,U2,U6,D8,D4,D2,D6,L6,L4,R6,R4};
```
Maps edge positions 0–23 to 3×3 facelet slots (for `fill333Facelet`).

### 2.4 The 8 corner pieces

Standard 3×3 representation: 8 positions, each has a cubie (0–7) and orientation (0/1/2).

```java
static final byte[][] cornerFacelet = {
    {U9,R1,F3}, {U7,F1,L3}, {U1,L1,B3}, {U3,B1,R3},
    {D3,F9,R7}, {D1,L9,F7}, {D7,B9,L7}, {D9,R9,B7}
};
```
(These are 3×3 facelet slots — centers and edges only exist in the 3×3 context.)

---

## 3. Cube State Representation

### 3.1 `CenterCube`

```c
typedef struct {
    uint8_t ct[24];  // ct[i] = face color (0–5) of piece in center position i
} CenterCube;
```

**Solved state**: `ct[i] = centerFacelet[i] / 16` for all i.

**move(m)**: `m = axis*3 + key` where axis in 0–11 (U R F D L B u r f d l b) and key in 0–2 (CW, 180°, CCW).

Center move table (which positions swap for each axis):
```
axis 0  (U):  swap(ct, 0,1,2,3, key)
axis 1  (R):  swap(ct, 16,17,18,19, key)
axis 2  (F):  swap(ct, 8,9,10,11, key)
axis 3  (D):  swap(ct, 4,5,6,7, key)
axis 4  (L):  swap(ct, 20,21,22,23, key)
axis 5  (B):  swap(ct, 12,13,14,15, key)
axis 6  (u):  swap(ct, 0,1,2,3, key)
              swap(ct, 8,20,12,16, key)
              swap(ct, 9,21,13,17, key)
axis 7  (r):  swap(ct, 16,17,18,19, key)
              swap(ct, 1,15,5,9, key)
              swap(ct, 2,12,6,10, key)
axis 8  (f):  swap(ct, 8,9,10,11, key)
              swap(ct, 2,19,4,21, key)
              swap(ct, 3,16,5,22, key)
axis 9  (d):  swap(ct, 4,5,6,7, key)
              swap(ct, 10,18,14,22, key)
              swap(ct, 11,19,15,23, key)
axis 10 (l):  swap(ct, 20,21,22,23, key)
              swap(ct, 0,8,4,14, key)
              swap(ct, 3,11,7,13, key)
axis 11 (b):  swap(ct, 12,13,14,15, key)
              swap(ct, 1,20,7,18, key)
              swap(ct, 0,23,6,17, key)
```

The `swap(arr, a, b, c, d, key)` primitive (see §5) performs a 4-cycle CW (key=0), 2×2-swap (key=1), or 4-cycle CCW (key=2).

### 3.2 `EdgeCube`

```c
typedef struct {
    uint8_t ep[24];  // ep[i] = which wing piece (0–23) occupies position i
} EdgeCube;
```

**Solved state**: `ep[i] = i`.

**move(m)**: `m = axis*3 + key`, axes 0–11 same as CenterCube.

Edge move table:
```
axis 0 (U):  swap(ep, 0,1,2,3, key); swap(ep, 12,13,14,15, key)
axis 1 (R):  swap(ep, 11,15,10,19, key); swap(ep, 23,3,22,7, key)
axis 2 (F):  swap(ep, 0,11,6,8, key); swap(ep, 12,23,18,20, key)
axis 3 (D):  swap(ep, 4,5,6,7, key); swap(ep, 16,17,18,19, key)
axis 4 (L):  swap(ep, 1,20,5,21, key); swap(ep, 13,8,17,9, key)
axis 5 (B):  swap(ep, 2,9,4,10, key); swap(ep, 14,21,16,22, key)
axis 6 (u):  same as U, plus: swap(ep, 9,22,11,20, key)
axis 7 (r):  same as R, plus: swap(ep, 2,16,6,12, key)
axis 8 (f):  same as F, plus: swap(ep, 3,19,5,13, key)
axis 9 (d):  same as D, plus: swap(ep, 8,23,10,21, key)
axis 10 (l): same as L, plus: swap(ep, 14,0,18,4, key)
axis 11 (b): same as B, plus: swap(ep, 7,15,1,17, key)
```

**checkEdge()**: verifies that pieces 0–11 each appear exactly once in positions 0–11, with even parity. Used after Phase 2 to reject invalid states before Phase 3.

```java
boolean checkEdge() {
    int ck = 0;
    boolean parity = false;
    for (int i=0; i<12; i++) {
        ck |= 1 << ep[i];
        parity = parity != ep[i] >= 12;
    }
    ck &= ck >> 12;
    return ck == 0 && !parity;
}
```
`ck == 0`: each bit in 0–11 set exactly once in ep[0..11].  
`!parity`: even number of ep[i] >= 12 (no cross-pairing).

**getParity()**: permutation parity of `ep` (from `Util.parity()`).

### 3.3 `CornerCube`

```c
typedef struct {
    uint8_t cp[8];  // corner permutation: cp[i] = which corner piece is in position i
    uint8_t co[8];  // corner orientation: co[i] in {0,1,2}
} CornerCube;
```

**Solved**: `cp[i] = i`, `co[i] = 0`.

Corners only participate in outer face moves (not wide moves). Move table for 18 outer moves is precomputed from 6 base move cubes:

```java
moveCube[0]  = new CornerCube(15120, 0);   // U
moveCube[3]  = new CornerCube(21021, 1494); // R
moveCube[6]  = new CornerCube(8064, 1236);  // F
moveCube[9]  = new CornerCube(9, 0);        // D
moveCube[12] = new CornerCube(1230, 412);   // L
moveCube[15] = new CornerCube(224, 137);    // B
// moveCube[a+1] = moveCube[a] * moveCube[a]  (180° turn)
// moveCube[a+2] = moveCube[a+1] * moveCube[a] (CCW turn)
```

**CornMult(a, b, prod)**: compose two corner cubes. Corner multiplication:
```c
prod.cp[i] = a.cp[b.cp[i]];
oriA = a.co[b.cp[i]];
oriB = b.co[i];
// orientation arithmetic with mirroring support (ori >= 3 signals mirror)
ori = oriA + (oriA < 3 ? oriB : 6 - oriB);
ori %= 3;
if ((oriA >= 3) ^ (oriB >= 3)) ori += 3;
prod.co[i] = ori;
```

**getParity()**: permutation parity of `cp`.

### 3.4 `FullCube`

Combines all three sub-cubes with lazy-evaluation move buffering:

```c
typedef struct {
    EdgeCube   edge;
    CenterCube center;
    CornerCube corner;

    uint8_t moveBuffer[60];
    int     moveLength;
    int     edgeAvail, centerAvail, cornerAvail;  // how far each sub-cube is applied

    // search metadata
    int  value;    // priority = length1 + length2 + lower_bound
    bool add1;     // whether 2 normalization moves were appended after phase 1
    int  length1, length2, length3;
    int  sym;      // accumulated cube rotation symmetry index (0–47)
} FullCube;
```

`move(m)` appends to `moveBuffer` but does NOT apply.  
`getEdge()` / `getCenter()` / `getCorner()` lazily apply buffered moves.  
`doMove(m)` applies immediately to all three sub-cubes (used when building from a move sequence).  
Corners apply `m % 18` (wide moves have same corner effect as the corresponding outer move — actually no, wide moves do NOT move corners, but `m % 18` maps wide moves back to outer. Since outer move `m` and wide move `m+18` have the same outer-face corner effect, `% 18` is correct).

Actually: corner.move uses `m % 18`. This is correct because for m in 18..35 (wide moves), the corner movement is identical to the outer face move `m - 18` (the wide move rotates the outer face the same way as the outer-only move, for corner pieces).

**to333Facelet()**: convert to 54-char string for min2phase:
```java
String to333Facelet() {
    char[] ret = new char[54];
    getEdge().fill333Facelet(ret);
    getCenter().fill333Facelet(ret);
    getCorner().fill333Facelet(ret);
    return new String(ret);
}
```

---

## 4. Move System

### 4.1 Standard move indices (0–35)

```
Index  Move    Index  Move    Index  Move
  0    U        12    D        24    Fw
  1    U2       13    D2       25    Fw2
  2    U'       14    D'       26    Fw'
  3    R        15    L        27    Dw
  4    R2       16    L2       28    Dw2
  5    R'       17    L'       29    Dw'
  6    F        18    B        30    Lw
  7    F2       19    B2       31    Lw2
  8    F'       20    B'       32    Lw'
  9    Uw       21    Rw       33    Bw
 10    Uw2      22    Rw2      34    Bw2
 11    Uw'      23    Rw'      35    Bw'
```

Encoding: `m = face_axis * 3 + power` where power 0=CW, 1=180°, 2=CCW and face_axis 0..5 for outer (U R F D L B) and 6..11 for wide (Uw Rw Fw Dw Lw Bw).

### 4.2 Phase move sets

**Phase 1** — all 36 moves.

**Phase 2** — 28 moves, stored in `move2std[29]` (last entry = 36 = eom):
```
Ux1 Ux2 Ux3 Rx1 Rx2 Rx3 Fx1 Fx2 Fx3 Dx1 Dx2 Dx3 Lx1 Lx2 Lx3 Bx1 Bx2 Bx3
Uw2 Rw1 Rw2 Rw' Fw2 Dw2 Lw1 Lw2 Lw' Bw2
```
(All 18 outer moves + Uw2 Fw2 Dw2 Bw2 only as half-turns + all 3 powers of Rw and Lw.)

Rationale: after Phase 1, U/D centers are on the U/D axis. Uw/Dw and Fw/Bw as quarter-turns would displace U/D centers from the axis, so only half-turns are allowed for those wide moves. Rw/Lw do not touch U/D centers.

**Phase 3** — 20 moves, stored in `move3std[21]`:
```
Ux1 Ux2 Ux3 Rx2 Fx1 Fx2 Fx3 Dx1 Dx2 Dx3 Lx2 Bx1 Bx2 Bx3
Uw2 Rw2 Fw2 Dw2 Lw2 Bw2
```
R and L outer moves only as half-turns (to preserve edge pairing), all wide as half-turns only.

### 4.3 Move-redundancy tables

```c
bool ckmv[37][36];    // ckmv[i][j] = true means j is redundant after i
bool ckmv2[29][28];   // phase 2 version
bool ckmv3[21][20];   // phase 3 version
int  skipAxis[36];    // for move i, first j where !ckmv[i][j]
int  skipAxis2[28];
int  skipAxis3[20];
```

**Rule**: move j is redundant after move i if:
- `i/3 == j/3` — same face (e.g., U after U)
- `i/3 % 3 == j/3 % 3 && i > j` — opposite faces in non-canonical order (U and D are modulo-3-equal, same for R/L and F/B)

`ckmv2` and `ckmv3` are derived by mapping phase-specific move indices back through `move2std` / `move3std` and consulting `ckmv`.

**skipAxis optimization**: in the IDA* inner loop, after detecting `ckmv[lm][m]` is true, jump directly to `skipAxis[m]` to skip all further redundant moves on the same axis.

---

## 5. Utility Primitives

### 5.1 Pascal's triangle

```c
int Cnk[25][25];  // Cnk[n][k] = C(n,k)
int fact[13];     // fact[i] = i!
```

Build:
```c
for (int i=0; i<25; i++) { Cnk[i][0]=1; Cnk[i][i]=1; }
for (int i=1; i<25; i++)
    for (int j=1; j<i; j++)
        Cnk[i][j] = Cnk[i-1][j] + Cnk[i-1][j-1];
fact[0]=1;
for (int i=0; i<12; i++) fact[i+1]=fact[i]*(i+1);
```

### 5.2 `swap(arr, a, b, c, d, key)`

Cyclic 4-position rotate on an array:
```c
void swap4(T *arr, int a, int b, int c, int d, int key) {
    T t;
    switch (key) {
    case 0:  // CW 4-cycle: a←d←c←b←a
        t=arr[d]; arr[d]=arr[c]; arr[c]=arr[b]; arr[b]=arr[a]; arr[a]=t; break;
    case 1:  // 2×2 swap: a↔c, b↔d
        t=arr[a]; arr[a]=arr[c]; arr[c]=t;
        t=arr[b]; arr[b]=arr[d]; arr[d]=t; break;
    case 2:  // CCW 4-cycle: a→b→c→d→a
        t=arr[a]; arr[a]=arr[b]; arr[b]=arr[c]; arr[c]=arr[d]; arr[d]=t; break;
    }
}
```

### 5.3 `parity(arr)`

Permutation parity (number of inversions mod 2):
```c
int parity(uint8_t *arr, int len) {
    int p = 0;
    for (int i=0; i<len; i++)
        for (int j=i+1; j<len; j++)
            if (arr[i] > arr[j]) p ^= 1;
    return p;
}
```

### 5.4 `set8Perm(arr, idx)` / `getCPerm(arr)` (for corners)

Decode an integer 0–40319 into a permutation of 8 elements using a packed nibble trick:
```c
void set8Perm(uint8_t *arr, int idx) {
    int val = 0x76543210;
    for (int i=0; i<7; i++) {
        int p = fact[7-i];
        int v = idx / p; idx -= v*p;
        v <<= 2;
        arr[i] = (val >> v) & 0xf;
        int m = (1<<v)-1;
        val = (val & m) + ((val>>4) & ~m);
    }
    arr[7] = val;
}
```

### 5.5 Combinatorial rank/unrank (binomial indexing)

Used everywhere to encode "which k of n positions are marked":

**Rank** (get): iterate i from n-1 down to 0; if position i is marked, add `Cnk[i][r]` and decrement r (from k down to 0).

**Unrank** (set): iterate i from n-1 down to 0; if `idx >= Cnk[i][r]`, mark position i, subtract `Cnk[i][r]`, decrement r.

---

## 6. Phase 1 — Center1: Orient U/D Centers

### 6.1 Goal

After Phase 1, all 8 U/D-colored center stickers are on the U or D face (4+4). Centers are not yet sorted within the face.

### 6.2 State coordinate

**Binary representation**: `byte[] ct[24]` where `ct[i] = 1` if center position `i` holds a U-or-D colored piece, else 0.

Always exactly 8 ones → combinatorial rank via `C(24,8) = 735,471` raw states.

**get()**: 
```java
int idx = 0, r = 8;
for (int i=23; i>=0; i--)
    if (ct[i] == 1) idx += Cnk[i][r--];
return idx;
```

**set(idx)**:
```java
int r = 8;
for (int i=23; i>=0; i--) {
    ct[i] = 0;
    if (idx >= Cnk[i][r]) { idx -= Cnk[i][r--]; ct[i] = 1; }
}
```

### 6.3 Symmetry group (48 elements)

The cube has 48 rotational + reflective symmetries. They are generated by 4 primitive rotations (`rot 0–3`):

```
rot 0: move(ux2); move(dx2)                       — 180° around U/D axis
rot 1: move(rx1); move(lx3)                       — 90° CW around R/L axis
rot 2: direct swap of center positions (see below) — 90° around F/B axis (via center manipulation)
rot 3: move(ux1); move(dx3); move(fx1); move(bx3) — combined rotation
```

rot 2 acts directly on `ct[]`:
```java
swap(ct, 0,3,1,2, 1);  swap(ct, 8,11,9,10, 1);
swap(ct, 4,7,5,6, 1);  swap(ct, 12,15,13,14, 1);
swap(ct, 16,19,21,22, 1); swap(ct, 17,18,20,23, 1);
```

The full 48-element group is enumerated by iterating rot 0 → rot 1 → rot 2 → rot 3 in this nested pattern:
```
for j in 0..47:
    apply rot 0
    if j%2==1:  apply rot 1
    if j%8==7:  apply rot 2
    if j%16==15: apply rot 3
```

**Tables built by `initSym()`**:
```c
int symmult[48][48];  // symmult[i][j] = composition of symmetry i then j
int syminv[48];       // syminv[i] = inverse of symmetry i
int symmove[48][36];  // symmove[s][m] = S * m * S^{-1} (conjugate of move m by symmetry s)
int finish[48];       // finish[i] = raw coord of the solved center under symmetry i
```

`symmove[s][m]` is computed by:
- Start from identity center
- Apply inverse of symmetry s
- Apply move m
- Apply symmetry s
- Find which move produces the same result (brute-force over all 36 moves)

### 6.4 Sym→Raw table: 15,582 symmetry classes

**`initSym2Raw()`**:
- Allocate `raw2sym[735471]` (can be freed after table build)
- BFS-like enumeration: for each raw state not yet seen, iterate through 48 symmetries, mark all as seen, record `sym2raw[count++] = i` and `raw2sym[sym_orbit_member] = count-1 << 6 | syminv[j]`
- Result: `sym2raw[15582]` and `raw2sym[735471]`

**Encoding**: `raw2sym[raw] = (sym_class << 6) | sym_index` — sym_index is the symmetry that maps the canonical representative to this raw state.

### 6.5 Move table: `ctsmv[15582][36]`

```c
int ctsmv[15582][36];
// ctsmv[i][m] = resulting state after applying move m to sym class i
// encoded as: (new_sym_class << 6) | new_sym_index
```

Build:
```c
for (int i=0; i<15582; i++) {
    d.set(sym2raw[i]);          // start from canonical representative
    for (int m=0; m<36; m++) {
        c.set(d);
        c.move(m);
        ctsmv[i][m] = c.getsym();  // convert result to sym-coordinate
    }
}
```

`getsym()` converts a raw Center1 state to its `(sym_class << 6 | sym_index)` encoding.

### 6.6 Pruning table: `csprun[15582]`

BFS from the solved state (sym_class 0). Uses bidirectional BFS: forward for depth ≤ 4, backward (find predecessors of unseen nodes) for depth > 4.

```c
byte csprun[15582];
// csprun[i] = min moves to solve sym class i
// built by BFS using ctsmv (with only 27 moves: only the first 27 of 36)
```

Note: only 27 of the 36 moves are used during the pruning BFS (the loop `for m in 0..27`). The move table has 36 columns for use during search but pruning only needs 27.

### 6.7 `search1()` — IDA* for Phase 1

```c
bool search1(int ct, int sym, int maxl, int lm, int depth) {
    if (ct == 0 && maxl < 5)
        return maxl == 0 && init2(sym, lm);

    for (int axis=0; axis<27; axis+=3) {
        if (axis==lm || axis==lm-9 || axis==lm-18) continue;  // same face
        for (int power=0; power<3; power++) {
            int m = axis + power;
            int ctx = ctsmv[ct][symmove[sym][m]];
            int prun = csprun[ctx >>> 6];
            if (prun >= maxl) {
                if (prun > maxl) break;  // skip remaining powers of this face
                continue;
            }
            int symx = symmult[sym][ctx & 0x3f];
            ctx >>>= 6;
            move1[depth] = m;
            if (search1(ctx, symx, maxl-1, axis, depth+1)) return true;
        }
    }
    return false;
}
```

Key: `symmove[sym][m]` conjugates move m by the current accumulated symmetry. The `ctx & 0x3f` is the new symmetry index to compose with `sym`.

### 6.8 `init2()` — normalize and transition to Phase 2

After Phase 1 finds a solution, it needs the 8 U/D centers to be sorted so that 4 are specifically on U and 4 on D (not just "8 on the U/D axis").

Three possible endpoint states (by `Center1.finish[sym]`):
- `finish[sym] == 735470`: centers already 4 on U, 4 on D → no extra moves needed, `sym = 0`
- `finish[sym] == 0`: need to apply `F B'` → append `Fx1, Bx3` to move buffer, `sym = 19`
- `finish[sym] == 12869`: need to apply `U D'` → append `Ux1, Dx3`, `sym = 34`

After normalization, compute Center2 coordinates and add the phase1 candidate to the priority queue.

The `p1sols` priority queue holds up to 500 best Phase 1 solutions ranked by `length1 + ctprun[s2ct*70 + s2rl]` (Phase 1 length + Phase 2 lower bound).

---

## 7. Phase 2 — Center2: Solve R/L Centers

### 7.1 Goal

Bring all 4 R-colored centers to the R face and all 4 L-colored centers to the L face. The U/D center orientation from Phase 1 is preserved.

### 7.2 State coordinates

**Center2** tracks two independent sub-coordinates:

**`ct` coordinate** (6435 values = C(15,8)):
- `int[] ct[16]`: the color-class of each center in positions 0–15 (U/D/F/B-face centers), reduced modulo 3.
  - 0 → U or D colored (UD-axis)
  - 1 → F or B colored (FB-axis)
  - 2 → R or L colored (RL-axis)
- After Phase 1, positions 0–7 (U and D faces) all have value 0.
- `getct()`: binomial rank of positions 0–14 that differ from `ct[15]` → C(15,8) = 6435.

**`rl` coordinate** (70 values = C(8,4) × 2 = 35 × 2):
- `int[] rl[8]`: positions 0–7 in R/L center slots; `rl[i] = 1` if that position holds an RL-colored piece.
- Exactly 4 of the 8 R/L center positions hold RL pieces.
- `getrl()`: C(7,4) rank of positions 0–6 differing from `rl[7]`, multiplied by 2, plus `parity`.
- `parity`: edge permutation parity, tracked through moves that affect it (`pmv[]` array).

**Combined state size**: 6435 × 70 = 450,450.

### 7.3 Move table for Center2

```c
int  rlmv[70][28];   // rl coordinate transitions for 28 phase-2 moves
char ctmv[6435][28]; // ct coordinate transitions for 28 phase-2 moves
```

Built by `Center2.init()` via direct simulation.

**`rl` under rotation** (for sym-class initialization):
```c
int rlrot[70][16];   // rl coordinate under 16 rotations (subset of 48 sym group)
char ctrot[6435][16]; // ct coordinate under 16 rotations
```

### 7.4 Pruning table: `ctprun[6435 × 70]`

```c
byte ctprun[6435 * 70];
// ctprun[ct*70 + rl] = min moves in Phase 2 to reach ct=0 and ctprun[rl]=0
```

Solved states (all 6 by symmetry):
```java
ctprun[0]=ctprun[18]=ctprun[28]=ctprun[46]=ctprun[54]=ctprun[56]=0;
```
(These are the 6 symmetric solved RL configurations given that U/D centers are already placed.)

BFS forward from depth 0, using only 23 of the 28 phase-2 moves.

### 7.5 `search2()` — IDA* for Phase 2

```c
bool search2(int ct, int rl, int maxl, int lm, int depth) {
    if (ct==0 && ctprun[rl]==0 && maxl==0)
        return init3();

    for (int m=0; m<23; m++) {
        if (ckmv2[lm][m]) { m = skipAxis2[m]; continue; }
        int ctx = ctmv[ct][m];
        int rlx = rlmv[rl][m];
        int prun = ctprun[ctx * 70 + rlx];
        if (prun >= maxl) {
            if (prun > maxl) m = skipAxis2[m];
            continue;
        }
        move2[depth] = move2std[m];
        if (search2(ctx, rlx, maxl-1, m, depth+1)) return true;
    }
    return false;
}
```

### 7.6 `init3()` — transition to Phase 3

Verifies that the edge state is valid for Phase 3 (`checkEdge()` must pass), then computes:
- `Edge3` coordinate from `EdgeCube`
- `Center3` coordinate
- Combined lower bound `max(Edge3.getprun(), Center3.prun[ct])`

Stores the candidate in `arr2[]` (up to 100 candidates).

---

## 8. Phase 3 — Center3 + Edge3: Pair Edges & Finish Centers

### 8.1 Goal

- **Center3**: fully solve all center pieces (each face gets its 4 same-colored centers).
- **Edge3**: pair all 24 wing edges so that the two wings of each edge pair are together, reducing to a valid 3×3×3 edge state.

After Phase 3 the cube is a solved 3×3×3 with scrambled corners and edges (but the corners still need the 3×3 solver).

### 8.2 Center3

**Sub-coordinates**:
- `int[] ud[8]`: 0 or 1 for each U/D center position (which 4 of 8 hold U-colored vs D-colored)
- `int[] fb[8]`: same for F/B positions
- `int[] rl[8]`: same for R/L positions (with parity correction)
- `int parity`: combined parity

**`getct()`**:
```
ud_idx = C(7,3) rank of ud[0..6] differing from ud[7]    → 35 values
fb_idx = C(7,3) rank of fb[0..6] differing from fb[7]    → 35 values
rl_idx via rl2std lookup                                   → 12 values
total = parity + 2 * (ud_idx * 35 * 12 + fb_idx * 12 + std2rl[rl_idx])
```

**State size**: 35 × 35 × 12 × 2 = 29,400.

**Solved state**: `getct() == 0`.

The 12 valid rl configurations (not all C(8,4)=70 are reachable from legal center states):
```java
static int[] rl2std = {0, 9, 14, 23, 27, 28, 41, 42, 46, 55, 60, 69};
```

**Move table**: `ctmove[29400][20]` — transitions for 20 phase-3 moves.

**Pruning table**: `prun[29400]` — BFS from solved, using 17 of 20 moves.

**`set(CenterCube c, int eXc_parity)`**:
```java
int parity = ... // derived from ct[0], ct[8], ct[16] value ordering
for (int i=0; i<8; i++) {
    ud[i] = (c.ct[i]   / 3) ^ 1;
    fb[i] = (c.ct[i+8] / 3) ^ 1;
    rl[i] = (c.ct[i+16]/ 3) ^ 1 ^ parity;
}
this.parity = parity ^ eXc_parity;
```

### 8.3 Edge3

Represents the 12-element permutation of edge "pairs" — which merged edge-pair piece is in each of the 12 edge positions.

**Fields**:
```c
int edge[12];   // edge[i] = which edge pair (0–11) is in position i
int edgeo[12];  // orientation tracker (position bookkeeping after rotations)
bool isStd;     // whether std() normalization is applied
```

**`set(EdgeCube c)`** — convert 24-wing EdgeCube to 12-pair Edge3:
1. `edge[i] = c.ep[FullEdgeMap[i]+12] % 12` — what wing pair is in the "top" wing of each position
2. Use a sorting network (cycle sort on `edge[]`) to compute the rearrangement `temp[]`
3. `edge[i] = temp[c.ep[FullEdgeMap[i]] % 12]` — combine with bottom wing

```java
static int[] FullEdgeMap = {0, 2, 4, 6, 1, 3, 7, 5, 8, 9, 10, 11};
```

**`get(end)`** — Lehmer rank of first `end` elements:
```c
int get(int end) {
    // if !isStd: apply std() first
    long val = 0xba9876543210L;  // packed remaining values
    int idx = 0;
    for (int i=0; i<end; i++) {
        int v = edge[i] << 2;
        idx = idx * (12-i) + ((val >> v) & 0xf);
        val -= 0x111111111110L << v;  // remove used value
    }
    return idx;
}
```

`get(4)` → range [0, 11880) = P(12,4)  
`get(10) % 20160` → range [0, 20160) = 8! (the remaining 8-element permutation)

**State coordinate**: `symcord1 * N_RAW + cord2` where:
- `cord1 = get(4)` → raw2sym lookup → `symcord1 = cord1_sym >> 3`, `symx = cord1_sym & 7`
- `cord2 = get(10) % N_RAW` (after rotating by `symx`)
- `N_SYM = 1538`, `N_RAW = 20160`
- Total: 1538 × 20160 = 31,006,080 states

**Symmetry group for Edge3**: 8 elements (subset of the full 48).  
`syminv = {0,1,6,3,4,5,2,7}` (mapping index i to its inverse).

Generated by 3 generators (rot 0–2):
```
rot 0: move(u2); move(d2)   — 180° U/D
rot 1: circlex+swapx ops    — 90° R/L
rot 2: various swapx ops    — 90° F/B
```

**`initSym2Raw()`**: enumerate 11,880 raw cord1 values; for each unseen one, apply 8 symmetries → builds `sym2raw[1538]`, `raw2sym[11880]`, `symstate[1538]`.

**`symstate[i]`**: bitmask of which of the 8 symmetries map sym class i to itself (self-symmetries). Used during pruning table construction to propagate entries across equivalent states without re-applying moves.

### 8.4 `mvrot` and `mvroto` tables

For fast move+rotation computation during IDA* without modifying state:

```c
int mvrot[20*8][12];   // mvrot[m<<3|r][i] = where edge i goes after move m then rotate r
int mvroto[20*8][12];  // mvroto[m<<3|r][i] = orientation tracking version
```

Built in `initMvrot()`: for each of 20 moves and 8 rotations, simulate on a fresh Edge3.

**`getmvrot(edge[], mrIdx, end)`**: compute `get(end)` after applying a combined move+rotation, without modifying `edge[]`. Used as a read-only fast coordinate query.

### 8.5 Pruning table: `eprun[N_EPRUN/16]`

2 bits per entry, packed into int array:
- Entry value 0 → depth ≡ 0 (mod 3)
- Entry value 1 → depth ≡ 1 (mod 3)
- Entry value 2 → depth ≡ 2 (mod 3)
- Entry value 3 → unseen (init value)

**`setPruning(table, index, value)`**: `table[index>>4] ^= (0x3^value) << ((index&0xf)<<1)`  
**`getPruning(table, index)`**: `(table[index>>4] >> ((index&0xf)<<1)) & 0x3`

**`createPrun()`**: bidirectional BFS using `getmvrot()` to apply moves. For each state with known depth, apply all 17 phase-3 moves. Uses `symstate` to propagate to equivalent states without extra moves.

**`getprun(int sym_coord, int current_depth_mod3)`**: fast lookup with modular depth correction:
```java
int depm3 = getPruning(eprun, sym_coord);
if (depm3 == 3) return MAX_DEPTH;
return (depm3 - prun + 16) % 3 + prun - 1;
```
This extracts the exact depth from the mod-3 encoded value.

### 8.6 Edge3 moves

The 20 phase-3 moves in `move()`:
```
0  U,  1  U2, 2  U' : circle/swap edge[0,4,1,5] and edgeo[]
3  R2             : swap edge/edgeo [5,10,6,11]
4  F,  5  F2, 6  F': edge[0,11,3,8]
7  D,  8  D2, 9  D': edge[2,7,3,6]
10 L2             : edge[4,8,7,9]
11 B, 12 B2, 13 B': edge[1,9,2,10]
14 u2: U2 + swap[9,11] + swap[8,10]
15 r2: R2 + swap[1,3] + swap[0,2]
16 f2: F2 + swap[5,7] + swap[4,6]
17 d2: D2 + swap[8,10] + swap[9,11]
18 l2: L2 + swap[0,2] + swap[1,3]
19 b2: B2 + swap[4,6] + swap[5,7]
```

The `edgeo[]` array tracks "virtual position labels" that allow the Lehmer-code `get()` to account for rotations applied during the search (`std()` normalizes using edgeo).

**`std()`**: normalize using `edgeo[]`:
```java
void std() {
    for (int i=0; i<12; i++) temp[edgeo[i]] = i;
    for (int i=0; i<12; i++) { edge[i] = temp[edge[i]]; edgeo[i] = i; }
    isStd = true;
}
```

### 8.7 `search3()` — IDA* for Phase 3

```c
bool search3(int edge, int ct, int prun, int maxl, int lm, int depth) {
    if (maxl == 0) return edge==0 && ct==0;

    tempe[depth].set_from_int(edge);  // decode edge coordinate to Edge3 state

    for (int m=0; m<17; m++) {
        if (ckmv3[lm][m]) { m = skipAxis3[m]; continue; }

        int ctx = Center3.ctmove[ct][m];
        int prun1 = Center3.prun[ctx];
        if (prun1 >= maxl) {
            if (prun1 > maxl && m<14) m = skipAxis3[m];
            continue;
        }

        int edgex = Edge3.getmvrot(tempe[depth].edge, m<<3, 10);
        int cord1x = edgex / N_RAW;
        int symcord1x_raw = raw2sym[cord1x];
        int symx = symcord1x_raw & 7;
        int symcord1x = symcord1x_raw >> 3;
        int cord2x = Edge3.getmvrot(tempe[depth].edge, m<<3|symx, 10) % N_RAW;
        int idx = symcord1x * N_RAW + cord2x;

        int prunx = Edge3.getprun(idx, prun);
        if (prunx >= maxl) {
            if (prunx > maxl && m<14) m = skipAxis3[m];
            continue;
        }

        if (search3(edgex, ctx, prunx, maxl-1, m, depth+1)) {
            move3[depth] = m;
            return true;
        }
    }
    return false;
}
```

Note: `search3` uses `tempe[depth]` as a per-depth scratch Edge3 object to avoid recomputing the edge state from `edge` (which is just the integer coordinate, not the full array). The actual `edge[]` array is in `tempe[depth]`, decoded lazily.

---

## 9. Final Step — 3×3×3 Solve (min2phase)

After Phase 3, the cube is equivalent to a scrambled 3×3×3:
- All centers are solved (4 same-colored per face)
- All edge pairs are matched (24 wings → 12 pairs in correct positions, with 3×3 orientation)
- Corners are scrambled

**`to333Facelet()`** converts the FullCube to the 54-character facelet string consumed by min2phase:
- Centers: trivial (solved from Phase 3)
- Edges: `fill333Facelet()` — maps 24 wing positions to 12 3×3 edge slots via `EdgeMap` and `EdgeColor`
- Corners: `fill333Facelet()` — standard 3×3 corner mapping

**min2phase** (`cs.min2phase.Search`): standard two-phase 3×3×3 solver (Kociemba's algorithm). Called as:
```java
String sol = search333.solution(facelet, 21, 1000000, 500, 0);
```
Parameters: max 21 moves, 1,000,000ms timeout, 500ms probe limit, flags=0.

---

## 10. Search Orchestration

### 10.1 `doSearch()` — top-level control

```
1. Compute 3 Center1 sym-coordinates (for UD, FB, RL axis labeling):
   ud = new Center1(center, 0).getsym()   // 0 = mark cells with color%3==0
   fb = new Center1(center, 1).getsym()   // 1 = mark cells with color%3==1
   rl = new Center1(center, 2).getsym()   // 2 = mark cells with color%3==2

2. Phase 1 IDA* (loop over increasing length1):
   - Try search1 for rl, then ud, then fb (whatever has lowest pruning value)
   - Collect up to PHASE1_SOLUTIONS=10,000 candidates into p1sols (priority queue of 500 best)
   - Stop when 10,000 solutions found or all depths exhausted

3. Sort p1sols by value (ascending = best first)

4. Phase 2 IDA* (loop over increasing length1+length2):
   - For each p1 solution, if length12-length1 <= MAX_LENGTH2: run search2
   - Collect up to PHASE2_SOLUTIONS=100 candidates in arr2[]
   - Increase MAX_LENGTH2 if no solution found at all

5. Sort arr2 by value (length1+length2+max(edge_prun, center3_prun))

6. Phase 3 IDA* (loop over increasing length123):
   - For each of top PHASE3_ATTEMPTS=100 phase2 solutions, run search3
   - Take first complete solution

7. Reconstruct full move sequence through all 3 phases

8. Convert to 3×3×3 facelet string, run min2phase

9. Assemble and return final solution string (applying sym rotations to normalize notation)
```

### 10.2 Solution string assembly (`getMoveString`)

The `sym` field in FullCube tracks accumulated cube rotations. Phase 2 and 3 moves are stored in the "symmetry-rotated" frame, so when outputting the solution we must apply `symmove[sym][m]` to convert each move back to the original orientation.

If a move in the rotated frame is a rotation move (`>= dx1`), it's converted to the equivalent 2-layer move and the `sym` is updated.

`rot2str[48]` provides human-readable rotation names for optional cube-rotation output.

### 10.3 Priority queue and beam search

Phase 1 uses a max-heap of size 500 (`PHASE2_ATTEMPTS`), keeping only the 500 solutions with the lowest `value = length1 + ctprun[phase2_state]`. When the heap is full, a new candidate only replaces the worst entry if it's better.

This ensures Phase 2 runs only on the most promising Phase 1 endings.

---

## 11. Table Sizes and Memory Budget

| Table | Size | Type | Bytes |
|---|---|---|---|
| `Cnk[25][25]` | 625 | int | 2.5 KB |
| `fact[13]` | 13 | int | 52 B |
| Center1: `ctsmv[15582][36]` | 560,952 | int | 2.2 MB |
| Center1: `sym2raw[15582]` | 15,582 | int | 62 KB |
| Center1: `csprun[15582]` | 15,582 | byte | 15 KB |
| Center1: `symmult[48][48]` | 2,304 | int | 9 KB |
| Center1: `symmove[48][36]` | 1,728 | int | 7 KB |
| Center1: `syminv[48]` | 48 | int | 192 B |
| Center1: `finish[48]` | 48 | int | 192 B |
| Center2: `rlmv[70][28]` | 1,960 | int | 8 KB |
| Center2: `ctmv[6435][28]` | 180,180 | char(2B) | 360 KB |
| Center2: `ctprun[450450]` | 450,450 | byte | 450 KB |
| Center3: `ctmove[29400][20]` | 588,000 | char(2B) | 1.2 MB |
| Center3: `prun[29400]` | 29,400 | byte | 29 KB |
| Edge3: `eprun[N_EPRUN/16]` | 1,937,880 | int | 7.8 MB |
| Edge3: `sym2raw[1538]` | 1,538 | int | 6 KB |
| Edge3: `raw2sym[11880]` | 11,880 | int | 47 KB |
| Edge3: `symstate[1538]` | 1,538 | char(2B) | 3 KB |
| Edge3: `mvrot[160][12]` | 1,920 | int | 8 KB |
| Edge3: `mvroto[160][12]` | 1,920 | int | 8 KB |
| **Total (approx)** | | | **~12 MB** |

Plus `raw2sym[735471]` during Center1 init (~3 MB temporary, freed after table build).

---

## 12. Initialization Order

```
1.  Build Cnk[25][25] and fact[13]                         (Util static init)
2.  Build CornerCube moveCube[18]                          (CornerCube static init)
3.  cs.min2phase.Search.init()                             (3×3×3 tables)
4.  Center1.initSym()                                      → symmult, syminv, symmove, finish
5.  Center1.raw2sym = new int[735471]
6.  Center1.initSym2Raw()                                  → sym2raw[15582], raw2sym[735471]
7.  Center1.createMoveTable()                              → ctsmv[15582][36]
8.  Center1.raw2sym = null                                 (free 3MB)
9.  Center1.createPrun()                                   → csprun[15582]
10. Center2.init()                                         → rlmv, ctmv, ctprun
11. Center3.init()                                         → ctmove, prun
12. Edge3.initMvrot()                                      → mvrot, mvroto
13. Edge3.initRaw2Sym()                                    → sym2raw, raw2sym, symstate
14. Edge3.createPrun()                                     → eprun  (~6-7 seconds)
```

Tables can be serialized to disk (see `Tools.saveTo` / `Tools.initFrom`) to skip recomputation on subsequent runs. The saved tables are `ctsmv` and `eprun`.

---

## 13. C Implementation Checklist

### Data structures to define
- [ ] `CenterCube` — `uint8_t ct[24]`
- [ ] `EdgeCube` — `uint8_t ep[24]`
- [ ] `CornerCube` — `uint8_t cp[8], co[8]`
- [ ] `FullCube` — combines above + move buffer + metadata
- [ ] `Center1` — `uint8_t ct[24]` (binary, distinct from CenterCube)
- [ ] `Center2` — `int rl[8], ct[16], parity`
- [ ] `Center3` — `int ud[8], fb[8], rl[8], parity`
- [ ] `Edge3` — `int edge[12], edgeo[12]`

### Tables to compute
- [ ] `Cnk[25][25]`, `fact[13]`
- [ ] Corner move cubes (6 base + 12 derived by composition)
- [ ] Center1: `symmult`, `syminv`, `symmove`, `finish`
- [ ] Center1: `sym2raw[15582]`, `ctsmv[15582][36]`, `csprun[15582]`
- [ ] Center2: `rlmv[70][28]`, `ctmv[6435][28]`, `ctprun[450450]`
- [ ] Center3: `ctmove[29400][20]`, `prun[29400]`
- [ ] Edge3: `mvrot[160][12]`, `mvroto[160][12]`
- [ ] Edge3: `sym2raw[1538]`, `raw2sym[11880]`, `symstate[1538]`
- [ ] Edge3: `eprun[1937880]` (2-bit packed)

### Move system
- [ ] 36-move `move()` for CenterCube, EdgeCube (12 axes × 3 powers)
- [ ] 18-move `move()` for CornerCube (with composition via `CornMult`)
- [ ] `ckmv[37][36]`, `ckmv2[29][28]`, `ckmv3[21][20]`
- [ ] `skipAxis[36]`, `skipAxis2[28]`, `skipAxis3[20]`
- [ ] `move2std[29]`, `move3std[21]`, `std2move[37]`, `std3move[37]`

### Coordinate functions
- [ ] Center1: `get()`, `set(idx)`, `getsym()`, `rot(0–3)`, `rotate(n)`, `move(m)`
- [ ] Center2: `getct()`, `setct(idx)`, `getrl()`, `setrl(idx)`, `rot(0–2)`, `move(m)`
- [ ] Center3: `getct()`, `setct(idx)`, `set(CenterCube, parity)`, `move(m)`
- [ ] Edge3: `get(end)`, `set(idx)`, `set(EdgeCube)`, `getsym()`, `std()`, `move(m)`, `rot(0–2)`, `rotate(n)`
- [ ] Edge3: `getmvrot(edge[], mrIdx, end)` — read-only coordinate evaluation
- [ ] Edge3: `getPruning()`, `setPruning()`, `getprun(coord)`, `getprun(coord, depth_mod3)`

### Search functions
- [ ] `search1(ct, sym, maxl, lm, depth)` + `init2(sym, lm)`
- [ ] `search2(ct, rl, maxl, lm, depth)` + `init3()`
- [ ] `search3(edge, ct, prun, maxl, lm, depth)`
- [ ] `doSearch()` — beam orchestration across all 3 phases
- [ ] `checkEdge()` — validate edge state before Phase 3
- [ ] `to333Facelet()` + `fill333Facelet()` for edges, centers, corners
- [ ] Priority queue for phase1 solutions (min-heap by value, capped at 500)
- [ ] `getMoveString()` — apply accumulated `sym` rotation to normalize output notation

### 3×3 solver
- [ ] Integrate or reimplement min2phase (Kociemba two-phase) for the final step.
  Input: 54-char facelet string in order U(9) R(9) F(9) D(9) L(9) B(9).
  Output: solution string in SiGN notation, ≤21 moves.
