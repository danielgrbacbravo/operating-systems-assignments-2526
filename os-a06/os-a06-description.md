## Operating Systems

## Assignment VI – “The Redundancy Project (Software RAID)”

### Deadline: Wednesday, March 25, 2026, at 23:00

## Instructions

1. This assignment consists of two parts: a **practical** exercise and a set of **theoretical** questions.
2. **Practical submissions** are made via **Themis**. **Theoretical submissions** are made via **Brightspace**. Both deadlines are strict.
3. The practical exercise is worth **50 points** and is auto-graded by Themis. The theoretical questions are worth **30 points** and are submitted separately via Brightspace. The remaining **20 points** are assessed via lab attendance.


## Overview

| Component | Type | Points | Submit via |
|---|---|---:|---|
| Software RAID Controller (`raid_controller.c`) | Practical | 50 | Themis |
| Theoretical Questions (5 questions) | Written | 30 | Brightspace |
| Lab attendance | Attendance | 20 | TA assessed |

> **Note**
> For the theoretical questions, download the LaTeX template `os_students.tex` from the assignment page and submit your typeset answers as a PDF on Brightspace.

## Introduction

Hardware fails. As a systems engineer, you cannot prevent hard drives from dying, but you can prevent data loss. In this assignment you will implement the logic for a Redundant Array of Independent Disks (RAID) entirely in software.

Real-world RAID controllers sit between the operating system and a set of physical disks, transparently splitting data across drives for speed, replicating it for safety, or both. You will build that same logic: given a byte buffer and a virtual disk API, your controller must store and retrieve data correctly, even when disks fail mid-operation.

## Part I Practical Exercise

### The Virtual Disk API

You are provided with `vdisk.h` and `vdisk.c`, which simulate a 4-disk storage array. Every disk is divided into 4 KB blocks (4096 bytes each) and supports up to 256 blocks. The API exposes two functions your controller must use:

```c
/* Write exactly 4096 bytes to disk_id at block_num.
 * Returns 0 on success, -1 on hardware failure. */
int disk_write(int disk_id, int block_num, const char *buffer);
```

```c
/* Read exactly 4096 bytes from disk_id at block_num.
 * Returns 0 on success, -1 on hardware failure. */
int disk_read(int disk_id, int block_num, char *buffer);
```

> **Alert**
> Do not use standard file I/O (`fopen`, `fwrite`, etc.) to store actual data. All persistent storage must go through `disk_write` and `disk_read`. The autograder checks disk-usage statistics to enforce this.

Key constants defined in `vdisk.h`:

| Constant | Value |
|---|---:|
| `BLOCK_SIZE` | 4096 |
| `NUM_DISKS` | 4 |
| `MAX_BLOCKS` | 256 |

> **Alert**
> Your output must exactly match the expected format. Themis compares your program’s standard output against reference output, so watch spacing, punctuation, and capitalization.

> **Note**
> Your program reads input from standard input. Compile with:
>
> ```bash
> gcc -Wall -Wextra -std=c99 -o raid vdisk.c raid_controller.c main.c
> ```

### Exercise Overview

You will implement three RAID levels, each in its own exercise on Themis. For every level you write a `store_file` function that distributes data across the disks and a `read_file` function that reconstructs it. All six functions go in your `raid_controller.c`.

| # | RAID Level | Functions | Points |
|---:|---|---|---:|
| 1 | RAID 0: Striping | `store_file_raid0`, `read_file_raid0` | 10 |
| 2 | RAID 1: Mirroring | `store_file_raid1`, `read_file_raid1` | 15 |
| 3 | RAID 5: Distributed Parity | `store_file_raid5`, `read_file_raid5` | 25 |

> **Note**
> All three exercises share the same set of source files. On Themis, the provided files (`vdisk.h`, `vdisk.c`, `raid_controller.h`, `main.c`) are inserted automatically. You only upload `raid_controller.c`. Each exercise only tests the function pair for that RAID level, so you only need to implement the two functions required for the exercise you are submitting.

### General Rules

- If `total_size` is not a perfect multiple of `BLOCK_SIZE`, pad the final block with null bytes (`\0`).
- Your program must never segfault or call `exit()` when a disk fails. Return a negative error code instead.
- Your output must exactly match the expected format. Themis compares standard output byte-for-byte against reference output.
- Do not store data in global variables to bypass the virtual disks. The autograder verifies disk statistics.

### Exercise 1: RAID 0 (Striping) — 10 points, Themis

RAID 0 splits data across all disks in a round-robin fashion. This maximizes throughput but provides _zero_ redundancy: if any single disk dies, the entire array is lost.

`store_file_raid0`  
Divide the incoming data into 4 KB blocks and stripe them sequentially across all 4 disks:

```text
Block 0 -> Disk 0, position 0
Block 1 -> Disk 1, position 0
Block 2 -> Disk 2, position 0
Block 3 -> Disk 3, position 0
Block 4 -> Disk 0, position 1
Block 5 -> Disk 1, position 1
...
```

In general, for logical block number `b`:

- Disk: `b mod NUM_DISKS`
- Block: `floor(b / NUM_DISKS)`

Return `0` on success, or a negative value if any `disk_write` fails.

`read_file_raid0`  
Read the blocks back in the same stripe order and reconstruct the original data. If any `disk_read` returns `-1`, return `-2` immediately. Do not attempt recovery; RAID 0 has no redundancy.

Example (no disk failure):

```text
Input:
12
Hello World!
-1
```

```text
Output:
Store Result: 0
Read Result: 0
Data: Hello World!
=== DISK STATS ===
Disk 0: R=1 W=
Disk 1: R=0 W=
Disk 2: R=0 W=
Disk 3: R=0 W=
```

> **Note**
> 12 bytes fits within a single 4 KB block, so only Disk 0 is used (block 0 maps to Disk `0 mod 4 = 0`). Disks 1–3 show zero I/O.

Now fail Disk 0 before reading:

```text
Input:
12
Hello World!
0
```

```text
Output:
Store Result: 0
Read Result: -2
=== DISK STATS ===
Disk 0: R=0 W=
Disk 1: R=0 W=
Disk 2: R=0 W=
Disk 3: R=0 W=
```

The read fails with `-2` and no data is printed. Disk 0 shows `R=0` because the read was rejected by the failed disk.

### Exercise 2: RAID 1 (Mirroring) — 15 points, Themis

RAID 1 keeps an identical copy of every block on two disks. Capacity is halved, but the array survives the loss of one disk with no data loss.

`store_file_raid1`  
Write every 4 KB block to **both** Disk 0 and Disk 1 at the same block position. Leave Disks 2 and 3 unused. Return `0` on success.

`read_file_raid1`  
Attempt to read each block from Disk 0 first. If `disk_read` returns `-1` for a block (indicating a hardware failure), **seamlessly** fall back to reading that block from Disk 1. The caller should not know a failure occurred; return the correct data and result code `0` as usual.  
If _both_ disks fail for the same block, return `-2`.

Example (Disk 0 fails before reading):

```text
Input:
12
Hello World!
0
```

```text
Output:
Store Result: 0
Read Result: 0
Data: Hello World!
=== DISK STATS ===
Disk 0: R=0 W=
Disk 1: R=1 W=
Disk 2: R=0 W=
Disk 3: R=0 W=
```

Disk 0 has `R=0` (read failed, rejected), but Disk 1 served the read transparently. The data is intact.

### Exercise 3: RAID 5 (Distributed Parity) — 25 points, Themis

RAID 5 stripes data across all disks, like RAID 0, but dedicates one block per stripe to parity. The parity block is the bitwise XOR of all data blocks in its stripe. This allows the array to reconstruct any single missing block. The parity position rotates across disks to distribute the I/O load evenly.

#### Stripe Layout (Left-Asymmetric)

With 4 disks, each stripe holds 3 data blocks and 1 parity block. The parity disk for stripe `s` is:

```text
parity_disk(s) = (N - 1) - (s mod N), where N = NUM_DISKS = 4
```

The 3 data blocks are placed on the remaining disks in order, skipping the parity disk.

| Stripe | Disk 0 | Disk 1 | Disk 2 | Disk 3 |
|---:|---|---|---|---|
| 0 | D0 | D1 | D2 | P0 |
| 1 | D3 | D4 | P1 | D5 |
| 2 | D6 | P2 | D7 | D8 |
| 3 | P3 | D9 | D10 | D11 |
| 4 | D12 | D13 | D14 | P4 |

`store_file_raid5`

1. Divide the data into 4 KB blocks.
2. Group consecutive blocks into stripes of 3 (the last stripe may have fewer data blocks; pad missing ones with all-zero blocks before computing parity).
3. For each stripe, compute the parity block as the byte-wise XOR of all 3 data blocks.
4. Place the data blocks and the parity block on the correct disks according to the rotation table above.

Return `0` on success.

Parity computation (given three 4 KB data blocks `A`, `B`, `C`):

```text
P[i] = A[i] XOR B[i] XOR C[i], for i = 0, 1, ..., 4095
```

If a block is lost, XOR the survivors together to recover it. For example, if `B` is lost:

```text
B[i] = A[i] XOR C[i] XOR P[i]
```

`read_file_raid5`  
Read all data blocks and parity blocks for each stripe. If a `disk_read` returns `-1` for exactly one block in a stripe, reconstruct it by XOR-ing the other blocks (including parity). If more than one disk in the same stripe fails, return `-2`.

Data-disk mapping (for logical data block number `d`, starting from 0):

- Stripe: `s = floor(d / (N - 1))`
- Index within stripe: `i = d mod (N - 1)`
- Parity disk: `p = (N - 1) - (s mod N)`
- Actual disk for data block: skip the parity disk, then take the `i`-th remaining disk in ascending order.

The block row on disk is equal to the stripe number `s`.

Example (12 bytes, Disk 0 fails before read):

```text
Input:
12
Hello World!
0
```

```text
Output:
Store Result: 0
Read Result: 0
Data: Hello World!
=== DISK STATS ===
Disk 0: R=0 W=
Disk 1: R=1 W=
Disk 2: R=0 W=
Disk 3: R=1 W=
```

Stripe 0 has parity on Disk 3. 12 bytes is only 1 data block, placed on Disk 0 (the first non-parity disk). When Disk 0 fails, the controller reconstructs the data from Disk 1 (zero-padded second data block), Disk 2 (zero-padded third data block), and Disk 3 (parity).

> **Note**
> Since the second and third data blocks are implicit all-zero blocks, parity equals the first data block. Reconstruction becomes: `D0 = 0 XOR 0 XOR P0 = P0`. Disks 1 and 3 are read; Disk 2 has no stored data but is still read as zeros. The exact disk statistics depend on how many zero-padded blocks your implementation writes.

### Implementation Hints

- Use `calloc` for temporary block buffers and padding; it zero-initializes memory.
- Use `memcpy` and pointer arithmetic to slice the input data into 4 KB chunks.
- XOR is associative and commutative, so you can accumulate parity incrementally.
- Test locally by editing the `FAIL_DISK` value in test input files. A value of `-1` means no failure.
- `disk_print_stats()` is called automatically by the test harness; do not call it yourself.

### Submission Details

Upload your source file to each of the three Themis exercises:

| Exercise | File to upload | Functions required |
|---:|---|---|
| 1 | `raid_controller.c` | `store_file_raid0`, `read_file_raid0` |
| 2 | `raid_controller.c` | `store_file_raid1`, `read_file_raid1` |
| 3 | `raid_controller.c` | `store_file_raid5`, `read_file_raid5` |

> **Alert**
> The uploaded file must be named exactly `raid_controller.c`. Themis will not accept any other filename.

Each exercise only tests the function pair listed above. You do not need to implement the functions for the other RAID levels in that submission. You may define helper functions (e.g., for parity computation or disk mapping) in your file. Do not modify `vdisk.h`, `vdisk.c`, `raid_controller.h`, or `main.c`.

> **Alert**
> Your code must compile cleanly with `gcc -Wall -Wextra -std=c99`. Programs that do not compile receive zero marks.

### Local Testing

The download package includes a `Makefile` and three test harness files (`main_raid0.c`, `main_raid1.c`, `main_raid5.c`) so you can test your implementation on your own machine before submitting.

Compile a specific RAID level:

```bash
make raid0   # produces ./raid
make raid1   # produces ./raid
make raid5   # produces ./raid
make         # builds all three
make clean   # remove binaries
```

Run a test by piping input directly:

```bash
# No disk failure: store and read back 12 bytes
printf '12\nHello World!\n-1\n' | ./raid
```

```bash
# Fail disk 0 before the read
printf '12\nHello World!\n0\n' | ./raid
```

The input format (identical for all three harnesses) is:

1. An integer `DATA_SIZE` (number of bytes to store).
2. Exactly `DATA_SIZE` bytes of raw data.
3. An integer `FAIL_DISK`: the disk to kill before reading (`-1` for no failure, `0`–`3` to fail a specific disk).

You can also feed the provided test files directly:

```bash
./raid0 < tests/1.in
./raid5 < tests/7.in
```

> **Note**
> The expected output for each test is stored in the corresponding `.out` file (if provided). Use `diff` to compare:
>
> ```bash
> ./raid0 < tests/1.in | diff - tests/1.out
> ```

## Reference: RAID Levels at a Glance

This section provides a visual comparison of the three RAID levels you will implement. Use it as a quick reference while coding.

### RAID 0: Striping

| Disk 0 | Disk 1 | Disk 2 | Disk 3 |
|---|---|---|---|
| Block 0 | Block 1 | Block 2 | Block 3 |
| Block 4 | Block 5 | Block 6 | Block 7 |
| ... | ... | ... | ... |

Data is distributed round-robin across all disks. Maximum throughput, zero fault tolerance. If any disk fails, all data is lost.

### RAID 1: Mirroring

| Disk 0 | Disk 1 | Disk 2 | Disk 3 |
|---|---|---|---|
| Block 0 | Block 0 | unused | unused |
| Block 1 | Block 1 | unused | unused |
| Block 2 | Block 2 | unused | unused |

Every block is written to both Disk 0 and Disk 1. Capacity is halved, but if either disk fails the other can serve every read.

### RAID 5: Distributed Parity

| Disk 0 | Disk 1 | Disk 2 | Disk 3 |
|---|---|---|---|
| D0 | D1 | D2 | P0 |
| D3 | D4 | P1 | D5 |
| D6 | P2 | D7 | D8 |
| P3 | D9 | D10 | D11 |
| D12 | D13 | D14 | P4 |

Parity blocks (`P`) rotate across disks to distribute the I/O load. Any single failed disk can be reconstructed from the surviving data and parity blocks via XOR.

## Part II: Theoretical Questions — 30 points, Brightspace

Answer the following five questions. Submit your answers as a PDF typeset in LaTeX using the provided template (`os_students.tex`). You may not submit handwritten solutions.

### Question 1: Deadlock vs. Safe States (6 points)

Can a system be in a state that is neither deadlocked nor safe? If so, give a concrete example. If not, prove that every state is either deadlocked or safe.

Definitions to recall:

- A state is **safe** if there exists a sequence of process completions (a _safe sequence_) such that every process can eventually acquire all the resources it needs, finish, and release its resources.
- A state is **deadlocked** if there is a set of processes, each waiting for a resource held by another process in the set, and none can make progress.
- A state is **unsafe** if it is not safe—but unsafe does not necessarily mean deadlocked.

### Question 2: Multi-Unit Resource Deadlock (6 points)

Is it possible for a resource deadlock to involve multiple units of one resource type and a single unit of another? If so, give a concrete example showing the processes, resources, current allocations, and outstanding requests. If not, explain why.

**Hint.** Model your example with a resource-allocation graph. Recall that a deadlock requires a cycle of holds-and-waits among the involved processes and resource types. Your example should have at least two resource types and at least two processes. Annotate the graph with the number of units held and requested.

### Question 3: Full Virtualization vs. Paravirtualization (6 points)

a) **(3 points)** Explain the difference between full virtualization and paravirtualization. Your answer should address:

- How each approach handles privileged instructions executed by the guest OS.
- Whether the guest OS needs to be modified.
- A concrete example of a privileged instruction and what happens to it under each model.

b) **(3 points)** Which of the two do you think is harder to implement? Justify your answer by discussing at least two technical challenges specific to your chosen approach.

The table below summarizes the key trade-offs:

| Full virtualization | Paravirtualization |
|---|---|
| Guest OS modified? No | Guest OS modified? Yes |
| Privileged instructions trapped and emulated | Privileged instructions replaced with hypercalls |
| Performance overhead higher | Performance overhead lower |
| Guest OS portability: any unmodified OS | Guest OS portability: only modified guests |

### Question 4: Type 1 vs. Type 2 Hypervisors (6 points)

Type 1 hypervisors (“bare-metal”) run directly on the hardware. Type 2 hypervisors (“hosted”) run as a process on top of a conventional host OS.

**Type 1 stack**

```text
Hardware
  ↓
Hypervisor
  ↓
VM 1, VM 2
  ↓
Guest OS, Guest OS
```

**Type 2 stack**

```text
Hardware
  ↓
Host OS
  ↓
Hypervisor
  ↓
VM 1, VM 2
```

Type 1 hypervisors can do everything Type 2 hypervisors can, and are generally more efficient. So why do Type 2 hypervisors exist at all?

Give at least three concrete reasons why a user or organization might prefer a Type 2 hypervisor despite its lower efficiency. For each reason, explain why Type 1 hypervisors fall short in that scenario.

### Question 5: Key Establishment with a Trusted Third Party (6 points)

**Secret-key** (symmetric) cryptography is more efficient than **public-key** (asymmetric) cryptography, but requires the sender and receiver to agree on a shared key in advance. Suppose Alice and Bob have _never met_, but there exists a trusted third party, Trent, who:

- shares a secret key `K_AT` with Alice, and
- shares a different secret key `K_BT` with Bob.

Describe a protocol by which Alice and Bob can establish a new shared session key `K_S` using only symmetric cryptography and the help of Trent. Your protocol should:

1. Number each message (e.g., Message 1, Message 2, ...).
2. State the sender, receiver, and contents of each message.
3. Explain why an eavesdropper who observes all messages on the network _cannot_ recover `K_S`.

**Hint.** This is closely related to the Needham–Schroeder symmetric-key protocol and to the design of Kerberos. The general flow is:

```text
Alice -> Trent: Request session key
Trent -> Alice: K_S encrypted for Alice, plus ticket for Bob
Alice -> Bob: Ticket containing K_S encrypted for Bob
Alice <-> Bob: Secure communication using K_S
```

You should spell out the cryptographic contents of each message (e.g., `E_{KAT}(...)`) and justify the security of your protocol.

### Good luck! 🎉
