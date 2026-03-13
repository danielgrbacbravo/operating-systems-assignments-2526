# Operating Systems

# Assignment IV – “Page Replacement Algorithms”

### Deadline: Wednesday, March 11, 2026, at 23:00

## 1. Part I: Practical Exercises (Themis)

### 1.1 Overview

This assignment explores **page replacement algorithms**, a core topic in virtual memory management.  
You will implement four different algorithms, each as a standalone C program, and compare their behaviour on identical workloads.


All four exercises share the same input format and read from **standard input**. Each program must write its output to **standard output**, matching the reference format exactly (including spacing and capitalisation). Upload a single `.c` file per exercise.

> **Important:** For Exercises 2–4, your program must also internally compute the Optimal replacement result and print it as a comparison line at the end of the output. This means you can (and should) reuse your Optimal implementation from Exercise 1.

---

### 1.2 Background

When a process accesses a virtual page that is not currently mapped to a physical frame, the hardware raises a **page fault**. The operating system must then choose a physical frame to hold the requested page. If all frames are occupied, one existing page must be evicted (replaced).

The choice of which page to evict is made by the **page replacement algorithm**. Different algorithms trade off between implementation complexity and the number of faults they produce. In this assignment you implement four:

1. **Optimal**: Always evicts the page whose next use is furthest in the future. Requires knowledge of future accesses, so it cannot be implemented in practice, but it establishes the theoretical lower bound on page faults.
2. **LRU**: Evicts the page that has not been accessed for the longest time. A good approximation of Optimal in many workloads, but true LRU is expensive to implement in hardware.
3. **MRU**: Evicts the *most* recently used page. Although counter-intuitive, MRU can outperform LRU on sequential or cyclic scan patterns where the most recently used page is unlikely to be referenced again immediately.
4. **LRU-Second Chance (Clock)**: A practical FIFO-based approximation using reference (R) and dirty (D) bits, scanning in two cycles before evicting.

Each page access also has a *type*: **R**ead or **W**rite. A Write sets the page’s *dirty bit*. When a dirty page is evicted, it must be written back to swap, which we count as a *write to swap*.

---

### 1.3 Common Input/Output Format

#### **Input**

```
F P
N
page process R/W
page process R/W
...
```

- **F**: Number of physical frames (1 ≤ F ≤ 100)
- **P**: Number of processes (1 ≤ P ≤ 10)
- **N**: Number of memory accesses that follow
- Each of the N subsequent lines contains:
    - **page**: Virtual page number (non-negative integer)
    - **process**: Process ID (1 to P)
    - **R/W**: Access type, a single character: R (read) or W (write)

> Note: Page *X* of process 1 and page *X* of process 2 are *different* pages occupying separate frames.

**Handling multiple processes:** When P > 1, all P processes compete for the same shared pool of F frames; there is no per-process reservation. At any moment a frame may hold a page belonging to any of the P processes.

**Eviction is global:** When a page fault requires eviction, the algorithm selects its victim from *all* currently loaded pages, regardless of which process owns them. The per-algorithm selection rules apply across process boundaries:

- **Optimal**: The look-ahead scans the entire remaining interleaved access sequence to find the page (from any process) whose next use is furthest in the future.
- **LRU / MRU**: Last-access timestamps come from the global step counter, so a page belonging to P1 and a page belonging to P2 are directly compared on the same time axis.
- **Clock**: The FIFO queue contains pages from all processes interleaved in load order; the two-cycle scan treats every slot the same regardless of ownership.

Per-process fault counts are tallied separately and printed in the summary line:  
`Page faults: N (P1: a, P2: b, ...)`

#### **Output**

Every exercise prints a header, a per-step trace, and a summary:

```
=== ALGORITHM_NAME ===
Frames: F, Processes: P
P1 7 R: FAULT P1:7, -, -
P1 0 R: FAULT P1:7, P1:0, -
...
P1 0 R: HIT P1:2, P1:0, P1:
...
Page faults: N (P1: a, P2: b)
Writes to swap: W
```

- **ALGORITHM_NAME** is one of: OPTIMAL, LRU, MRU, CLOCK.
- Each trace line shows `Pp page type: STATUS frame_contents`, where STATUS is either `FAULT` or `HIT`.
- `frame_contents` lists all F frames in order, comma-separated. An empty frame is shown as `-`; an occupied frame as `Pp:page`.
- For Exercises 2–4, append one extra line:  
  `Optimal faults: M`

---

### 1.4 Exercise 1: Optimal (Bélády’s Algorithm)

#### **Algorithm**

When a page fault occurs and all frames are occupied, the Optimal algorithm looks *ahead* in the remaining access sequence and evicts the page whose next access is **furthest in the future**. If a page is never accessed again, it is the ideal eviction candidate. Ties are broken by choosing the **lowest frame index**.

#### **Example**

Given 3 frames, 1 process, and the reference string ⟨7, 0, 1, 2, 0, 3, 0, 4, 2, 3⟩ (all reads):

| Access | 7 | 0 | 1 | 2 | 0 | 3 | 0 | 4 | 2 | 3 |
|--------|---|---|---|---|---|---|---|---|---|---|
| Frame 0| 7 | 7 | 7 | 2 | 2 | 2 | 2 | 2 | 2 | 2 |
| Frame 1| - | 0 | 0 | 0 | 0 | 0 | 0 | 4 | 4 | 4 |
| Frame 2| - | - | 1 | 1 | 1 | 3 | 3 | 3 | 3 | 3 |
| Status | F | F | F | F | H | F | H | F | H | H |

- Step 4 (page 2): pages 7 and 1 are never used again, so we evict 7 (lowest frame index).
- Step 8 (page 4): page 0 is never used again from this point, so we evict it.
- **Total:** 6 page faults, 0 writes to swap.

#### **Input**

```
3 1
10
7 1 R
0 1 R
1 1 R
2 1 R
0 1 R
3 1 R
0 1 R
4 1 R
2 1 R
3 1 R
```

#### **Output**

```
=== OPTIMAL ===
Frames: 3, Processes: 1
P1 7 R: FAULT P1:7, -, -
P1 0 R: FAULT P1:7, P1:0, -
P1 1 R: FAULT P1:7, P1:0, P1:1
P1 2 R: FAULT P1:2, P1:0, P1:1
P1 0 R: HIT P1:2, P1:0, P1:1
P1 3 R: FAULT P1:2, P1:0, P1:3
P1 0 R: HIT P1:2, P1:0, P1:3
P1 4 R: FAULT P1:2, P1:4, P1:3
P1 2 R: HIT P1:2, P1:4, P1:3
P1 3 R: HIT P1:2, P1:4, P1:3
Page faults: 6 (P1: 6)
Writes to swap: 0
```

---

### 1.5 Exercise 2: Least Recently Used (LRU)

#### **Algorithm**

When eviction is required, LRU selects the page whose last access is the **oldest** (i.e. the page with the smallest “last used” timestamp). On a hit, the page’s timestamp is updated to the current step. Ties are broken by choosing the **lowest frame index**.

#### **Example**

Using the same reference string ⟨7, 0, 1, 2, 0, 3, 0, 4, 2, 3⟩ with 3 frames:

| Access | 7 | 0 | 1 | 2 | 0 | 3 | 0 | 4 | 2 | 3 |
|--------|---|---|---|---|---|---|---|---|---|---|
| Frame 0| 7 | 7 | 7 | 2 | 2 | 2 | 2 | 4 | 4 | 4 |
| Frame 1| - | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 3 |
| Frame 2| - | - | 1 | 1 | 1 | 3 | 3 | 3 | 2 | 2 |
| Status | F | F | F | F | H | F | H | F | F | F |

- Step 8 (page 4): pages have last-access times {2:3, 0:4, 3:5}. LRU evicts page 2 (oldest, time 3).
- Step 9 (page 2, fault): LRU evicts page 3 (time 5, oldest now).
- Step 10 (page 3, fault): LRU evicts page 0 (time 6).
- **Total:** 8 faults vs. Optimal’s 6.

> **Remember:** Your program must also internally run the Optimal algorithm and print the comparison line at the end.

#### **Input**

```
3 1
10
7 1 R
0 1 R
1 1 R
2 1 R
0 1 R
3 1 R
0 1 R
4 1 R
2 1 R
3 1 R
```

#### **Output**

```
=== LRU ===
Frames: 3, Processes: 1
P1 7 R: FAULT P1:7, -, -
P1 0 R: FAULT P1:7, P1:0, -
P1 1 R: FAULT P1:7, P1:0, P1:1
P1 2 R: FAULT P1:2, P1:0, P1:1
P1 0 R: HIT P1:2, P1:0, P1:1
P1 3 R: FAULT P1:2, P1:0, P1:3
P1 0 R: HIT P1:2, P1:0, P1:3
P1 4 R: FAULT P1:4, P1:0, P1:3
P1 2 R: FAULT P1:4, P1:0, P1:2
P1 3 R: FAULT P1:4, P1:3, P1:2
Page faults: 8 (P1: 8)
Writes to swap: 0
Optimal faults: 6
```

---

### 1.6 Exercise 3: Most Recently Used (MRU)

#### **Algorithm**

MRU is the opposite of LRU: on eviction, it selects the page whose last access is the **most recent** (largest timestamp). Ties are broken by choosing the **lowest frame index**.

> MRU may seem counter-intuitive, but it can outperform LRU on workloads with cyclic or sequential scan patterns, where the most recently used page is unlikely to be needed at the very next step.

#### **Example**

Same reference string, 3 frames:

| Access | 7 | 0 | 1 | 2 | 0 | 3 | 0 | 4 | 2 | 3 |
|--------|---|---|---|---|---|---|---|---|---|---|
| Frame 0| 7 | 7 | 7 | 7 | 7 | 7 | 7 | 7 | 7 | 7 |
| Frame 1| - | 0 | 0 | 0 | 0 | 3 | 0 | 4 | 4 | 4 |
| Frame 2| - | - | 1 | 2 | 2 | 2 | 2 | 2 | 2 | 3 |
| Status | F | F | F | F | H | F | F | F | H | F |

- Step 6 (page 3): MRU evicts page 0 (most recently used at step 5).
- Step 7 (page 0): MRU evicts page 3 (most recently used at step 6).
- Notice that frame 0 (page 7) is never evicted because it is always the least recently used.
- **Total:** 8 faults vs. Optimal’s 6.

#### **Input**

```
3 1
10
7 1 R
0 1 R
1 1 R
2 1 R
0 1 R
3 1 R
0 1 R
4 1 R
2 1 R
3 1 R
```

#### **Output**

```
=== MRU ===
Frames: 3, Processes: 1
P1 7 R: FAULT P1:7, -, -
P1 0 R: FAULT P1:7, P1:0, -
P1 1 R: FAULT P1:7, P1:0, P1:1
P1 2 R: FAULT P1:7, P1:0, P1:2
P1 0 R: HIT P1:7, P1:0, P1:2
P1 3 R: FAULT P1:7, P1:3, P1:2
P1 0 R: FAULT P1:7, P1:0, P1:2
P1 4 R: FAULT P1:7, P1:4, P1:2
P1 2 R: HIT P1:7, P1:4, P1:2
P1 3 R: FAULT P1:7, P1:4, P1:3
Page faults: 8 (P1: 8)
Writes to swap: 0
Optimal faults: 6
```

---

### 1.7 Exercise 4: LRU-Second Chance (Clock)

#### **Algorithm**

This algorithm maintains a FIFO queue of loaded pages (ordered by *load time*, *not* last access time). Each page has two bits:

- **R (Referenced)**: Set to 1 whenever the page is accessed (on a hit or initial load). Not updated based on access time.
- **D (Dirty)**: Set to 1 when the page is written to (W access).

On a page **hit**: set R=1 (and D=1 if the access is a write). The page’s position in the FIFO queue does *not* change.

On a page fault with no free frames, the algorithm scans the FIFO queue in two cycles:

1. **Cycle 1**: Scan the queue from front to back:
    - If R=0 and D=0: **replace immediately** (clean, unreferenced page — ideal victim).
    - If R=1: **clear R** to 0 and skip (give the page a “second chance”).
    - If R=0 and D=1: **skip** (prefer not to write back a dirty page).
    - Scanned pages that are not evicted are re-enqueued at the back.
2. **Cycle 2**: Scan again from the front. All R bits were cleared in Cycle 1, so evict the **first page** encountered. If D=1, count a write to swap before evicting.

After eviction, the newly loaded page is enqueued at the **back** of the FIFO queue with R=1 (and D=1 if the access is a write).

#### **Example**

Using the same reference string ⟨7, 0, 1, 2, 0, 3, 0, 4, 2, 3⟩ with 3 frames:

| Access | 7 | 0 | 1 | 2 | 0 | 3 | 0 | 4 | 2 | 3 |
|--------|---|---|---|---|---|---|---|---|---|---|
| Frame 0| 7 | 7 | 7 | 2 | 2 | 2 | 2 | 4 | 4 | 4 |
| Frame 1| - | 0 | 0 | 0 | 0 | 0 | 0 | 0 | 2 | 2 |
| Frame 2| - | - | 1 | 1 | 1 | 3 | 3 | 3 | 3 | 3 |
| Status | F | F | F | F | H | F | H | F | F | H |

- Step 4 (page 2): All three pages have R=1 (recently loaded). Cycle 1 clears all R bits but finds no R=0, D=0 victim (all were R=1, now cleared). Cycle 2 evicts the front of the queue: frame 0 (page 7, loaded first).
- Step 6 (page 3): Page 0 has R=1 (hit at step 5). Page 1 has R=0 (cleared earlier), D=0. Cycle 1 finds it immediately. Evict frame 2 (page 1).
- Step 8 (page 4): All R bits are 1 again. Cycle 1 clears them. Cycle 2 evicts front of queue: frame 0 (page 2).
- Step 9 (page 2): Page 0 has R=0, D=0. Cycle 1 evicts frame 1 (page 0) immediately.
- **Total:** 7 faults vs. Optimal’s 6, 0 writes to swap.

#### **Input**

```
3 1
10
7 1 R
0 1 R
1 1 R
2 1 R
0 1 R
3 1 R
0 1 R
4 1 R
2 1 R
3 1 R
```

#### **Output**

```
=== CLOCK ===
Frames: 3, Processes: 1
P1 7 R: FAULT P1:7, -, -
P1 0 R: FAULT P1:7, P1:0, -
P1 1 R: FAULT P1:7, P1:0, P1:1
P1 2 R: FAULT P1:2, P1:0, P1:1
P1 0 R: HIT P1:2, P1:0, P1:1
P1 3 R: FAULT P1:2, P1:0, P1:3
P1 0 R: HIT P1:2, P1:0, P1:3
P1 4 R: FAULT P1:4, P1:0, P1:3
P1 2 R: FAULT P1:4, P1:2, P1:3
P1 3 R: HIT P1:4, P1:2, P1:3
Page faults: 7 (P1: 7)
Writes to swap: 0
Optimal faults: 6
```

---

## 2. Part II: Theoretical Exercises (Brightspace)

Submit your answers as a PDF via Brightspace. A LaTeX template (`os_students.tex`) is provided in the downloadable archive.


### **Question 1 (6 points)**

Explain how hard links and symbolic links (soft links) differ with respect to i-node allocation.

- (a) Describe the difference in i-node allocation between a hard link and a symbolic link.
- (b) Name one advantage of hard links over symbolic links.
- (c) Name one advantage of symbolic links over hard links.

---

### **Question 2 (8 points)**

Free disk space can be tracked using either a free list or a bitmap. Disk addresses require D bits. For a disk with B blocks, F of which are free:

- (a) State the condition (in terms of D, B, and F) under which the free list occupies less space than the bitmap.
- (b) For D = 16 bits, express your answer as a percentage of disk blocks that must be free for the free list to be more compact than the bitmap.

---

### **Question 3 (10 points)**

A disk has 10 data blocks numbered 14 through 23. Two files, `f1` and `f2`, are stored on the disk. The directory records that the first data block of `f1` is block 22 and the first data block of `f2` is block 16. The FAT entries are listed below, where `(x, y)` means that entry x points to block y, and `-1` marks the end of a chain:

```
(14, 18), (15, 17), (16, 23), (17, 21), (18, 20), (19, 15), (20, -1), (21, -1), (22, 19), (23, 14)
```

- (a) List all data blocks allocated to `f1`, in order.
- (b) List all data blocks allocated to `f2`, in order.

---

### **Question 4 (6 points)**

A university stores student records in a file. The records are randomly accessed and updated; each record has a fixed size. Of the three file allocation schemes (contiguous, linked, and table-based/indexed), which is most appropriate for this use case? Justify your choice and briefly explain why each of the other two schemes is less suitable.
