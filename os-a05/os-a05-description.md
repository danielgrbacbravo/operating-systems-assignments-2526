## Operating Systems

## Assignment V – “FAT16 Filesystem”

#### Deadline: Wednesday, March 18, 2026, at 23:59

### Instructions

This assignment consists of two parts: a **practical** exercise and a set of **theoretical** questions.


### Introduction

Every program you have written so far (diagnostic scripts, concurrent processes, threaded simulations, virtual memory managers) relied on the operating system to handle one thing they all took for granted: files. Every `fopen()`, every `fread()`, every `printf()` ultimately asks the OS to translate a human-readable path like `/home/you/notes.txt` into raw bytes sitting somewhere on a storage device. But what does that translation actually look like? In this assignment, you become the filesystem and find out first-hand.

**What is FAT16?**

Think of a hard disk (or a USB stick, or an SD card) as one long strip of bytes, billions of them, numbered starting from zero. By themselves these bytes are meaningless. A filesystem is the set of conventions that gives them structure: “bytes 0-61 describe the geometry of this disk,” “bytes 512- are a table that tracks which pieces belong to which file,” and so on.

FAT16 (File Allocation Table, 16-bit) is one of the oldest and simplest such conventions. Microsoft introduced it in the 1980s for MS-DOS, and it is still the default format on many SD cards, USB drives, digital cameras, and embedded systems. When you plug a fresh SD card into your laptop, chances are it is formatted as FAT.

The “16” refers to the width of the table entries: each one is a 16-bit integer, which limits the number of data clusters the filesystem can track and, therefore, the maximum disk size. The simplicity that makes FAT16 easy to implement on tiny microcontrollers is the same simplicity that makes it a perfect learning exercise: the entire on-disk format can be described by two small C structs and a handful of offset calculations.

**What you will build**

You will work with a real FAT16 disk image, a regular file that contains an exact byte-for-byte copy of what would be on a physical disk. Instead of calling library functions that hide the details, you will open this image as a flat binary file and interpret every byte yourself: the boot sector that describes the disk geometry, the File Allocation Table that chains clusters together, and the 32-byte directory entries that give files their names, sizes, and timestamps.

The five exercises build on each other progressively:

1. **Parse** the BIOS Parameter Block and print the volume geometry.
2. **List** the files in the root directory.
3. **Read** a file by following its cluster chain.
4. **Traverse** the full directory tree recursively.
5. **Write** a new file to the image.

By the end, you will have built a miniature filesystem driver entirely in user-space C, with both reading *and* writing capabilities. Along the way you will get comfortable with a skill that comes up constantly in systems programming: pointing a C struct at a chunk of raw bytes and letting the field layout do the parsing for you.

### 1. Part I: Practical Exercises (Themis)

#### 1.1 Overview

All five exercises operate on a FAT16 **disk image** file (a flat binary file). A shared header `fat16_types.h` is provided as part of the template; it contains all on-disk structure definitions, attribute constants, and the runtime context struct. **Do not modify** this header.

Each exercise builds on the previous one. You are expected to copy and reuse your implementations from earlier exercises. Upload a single `.c` file per exercise.

> **Alert!**
> Your output must exactly match the expected format. Themis compares your program’s `stdout` against reference output, so watch spacing, punctuation, and capitalisation.

> **Note.**
> A test FAT16 image will be provided on Themis. You can also create your own test images using `mkfs.fat -F 16` from the `dosfstools` package. See the appendix for instructions.

#### 1.2 Background

Before diving into the exercises, let us build up a mental model of how FAT16 works. If you already know all of this, feel free to skim ahead to Exercise 1 (§1.3).

**A disk is just a big array of bytes**

At the hardware level, every storage device (hard disk, SSD, USB stick, SD card) exposes a flat sequence of bytes, numbered from 0 up to the device’s capacity. The device itself has no concept of files, folders, or names. All of that structure is imposed by the **filesystem**: a set of conventions about which byte ranges mean what.

In this assignment you will work with a **disk image**: a regular file on your own computer that is that flat byte sequence. If the image is 32 MB, then byte 0 of the file corresponds to byte 0 of the (virtual) disk, byte 1 to byte 1, and so on. You can open it with `fopen()`, seek to any offset, and read or write any byte.

**FAT16 Disk Layout**

A FAT16-formatted disk is divided into four regions, laid out sequentially one after the other:

| Boot Sector (BPB) | FAT Region | Root Directory | Data Region |
| :--- | :--- | :--- | :--- |
| byte 0 | `fat_start` | `root_start` | `data_start` |

Let us walk through each region:

1. **Boot Sector / BPB** (byte offset 0). The very first bytes of the disk contain the *BIOS Parameter Block* (BPB), a small header that describes everything you need to know about the disk’s geometry: how big a sector is, how many sectors make up a cluster, where the FAT lives, how large the root directory is, and so on. Think of it as the “table of contents” for the entire filesystem. It is stored as a tightly packed structure, and in Exercise 1 you will read it in one shot with `fread()`.
2. **FAT Region** (starts at `fat_start`). The *File Allocation Table* is the heart of FAT16. It is a flat array of 16-bit integers, with one entry per cluster on the disk. Each entry tells you what comes *next* in a file’s chain of clusters:
    * `0x0000` - the cluster is free (nobody is using it).
    * `0x0002`-`0xFFEF` - the next cluster number in the chain.
    * `0xFFF7` - bad cluster (physically damaged, do not use).
    * `0xFFF8`-`0xFFFF` - end of chain (this is the file’s last cluster).
    Entries 0 and 1 are reserved; data clusters start at index 2. The disk usually stores two identical copies of the FAT for redundancy (the BPB field `num_fats` tells you how many).
3. **Root Directory** (starts at `root_start`). A fixed-size array of 32-byte *directory entries*. Each entry records a file’s name (in 8.3 format), its attributes, timestamps, the number of its first cluster, and its size in bytes. The maximum number of entries in the root is given by `root_entry_count` in the BPB (commonly 512).
4. **Data Region** (starts at `data_start`). The actual file and subdirectory contents, organised in *clusters*. A cluster is a group of consecutive sectors (for example, 4 sectors × 512 bytes = 2048 bytes per cluster). Clusters are numbered starting from 2, so cluster *C* lives at byte offset:
    `data_start + (C - 2) * cluster_size`

**How files are stored: cluster chains**

A file’s data might not fit in a single cluster, and the clusters allocated to it need not be adjacent. FAT16 handles this with a **linked list stored in the FAT table**.

Suppose a file’s directory entry says its first cluster is 5. To read the whole file, you follow the chain:

| Step | What you do |
| :--- | :--- |
| 1 | Read cluster 5. Look up `FAT[5]` → 8. |
| 2 | Read cluster 8. Look up `FAT[8]` → 12. |
| 3 | Read cluster 12. Look up `FAT[12]` → `0xFFF8` (end of chain). |

The file occupies clusters 5 → 8 → 12, in that order. Here is what the relevant FAT entries look like:

| [0] | [1] | [2] | [3] | [4] | [5] | [6] | [7] | [8] | [9] | [10] | [11] | [12] |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| -- | -- | 0000 | 0000 | 0000 | **0008** | 0000 | 0000 | **000C** | 0000 | 0000 | 0000 | **FFF8** |

Entries 0 and 1 (grey) are reserved. The highlighted entries form the file’s cluster chain: cluster 5 points to 8, cluster 8 points to 12, and cluster 12 holds the end-of-chain marker (FFF8). Every other entry is zero (free).

**Key Offset Formulas**

All offsets are in **bytes** from the start of the image:

```c
fat_start = reserved_sectors * bytes_per_sector
root_start = fat_start + num_fats * fat_size_16 * bytes_per_sector
data_start = root_start + root_entry_count * 32
cluster_size = bytes_per_sector * sectors_per_clus
```

**Seeing the raw bytes: using a hex dump**

When debugging a filesystem implementation, the most useful tool in your toolbox is a **hex dumper**. It shows you the raw bytes of a file in hexadecimal, side by side with their ASCII interpretation. On Linux, `xxd` (bundled with vim) and `hexdump -C` both work well:

```bash
# Show the first 64 bytes of a FAT16 image
xxd -l 64 test.img
```

Output (for the test image from the appendix):
```text
00000000: eb3c 906d 6b66 732e 6661 7400 0204 0100 .<.mkfs.fat .....
00000010: 0200 0200 0080 f800 8000 2000 0000 0000 .......... .....
00000020: 0000 0100 8000 29ab cdef 0154 4553 5444 ......) .... TESTD
00000030: 4953 4b20 2020 4641 5431 3620 2020 0000 ISK FAT ..
```

How to read this: the leftmost column is the **byte offset** (in hex). The middle columns are the **raw byte values** (in hex, grouped in pairs). The rightmost column is the **ASCII decoding** (non-printable bytes show as dots).

Let us pick out a few BPB fields from this dump:

| Offset | Hex bytes | Field | Decoded value |
| :--- | :--- | :--- | :--- |
| 0x03 | `6d 6b 66 73 2e 66 61 74` | `oem_name[8]` | "mkfs.fat" |
| 0x0B | `00 02` | `bytes_per_sector` | `0x0200` = 512 |
| 0x0D | `04` | `sectors_per_clus` | 4 |
| 0x0E | `01 00` | `reserved_sectors` | 1 |
| 0x10 | `02` | `num_fats` | 2 |
| 0x11 | `00 02` | `root_entry_count` | `0x0200` = 512 |
| 0x2B | `54 45 53 54 44 49 53 ...` | `volume_label[11]` | "TESTDISK " |
| 0x36 | `46 41 54 31 36 20 20 20` | `fs_type[8]` | "FAT16 " |

> **Note.**
> FAT16 stores multi-byte integers in little-endian byte order. That means the least-significant byte comes first. For example, the hex bytes `00 02` at offset `0x0B` represent the 16-bit value `0x0200` = 512, not `0x0002` = 2. On x86 and ARM (which are both little-endian), C reads these correctly without any special conversion.

> **Note.**
> Get in the habit of hex-dumping your images whenever something looks wrong. You can dump a specific region like this:
> ```bash
> # Dump 32 bytes starting at offset 0x4200 (the root directory)
> xxd -s 0x4200 -l 32 test.img
> ```

**Laying a C struct on top of raw bytes**

Here is the key C technique that makes this entire assignment work. Look at the first few fields of the BPB struct in `fat16_types.h`:

```c
#pragma pack(push, 1) /* no padding between fields */
typedef struct {
    uint8_t jump[3];             /* offset 0x00, 3 bytes */
    char oem_name[8];            /* offset 0x03, 8 bytes */
    uint16_t bytes_per_sector;   /* offset 0x0B, 2 bytes */
    uint8_t sectors_per_clus;    /* offset 0x0D, 1 byte */
    uint16_t reserved_sectors;   /* offset 0x0E, 2 bytes */
    uint8_t num_fats;            /* offset 0x10, 1 byte */
    /* ... more fields ... */
} BPB;
#pragma pack(pop)
```

Normally, the C compiler is free to insert padding bytes between struct fields for alignment reasons. The `#pragma pack(push, 1)` directive tells the compiler: “pack these fields tightly, with no gaps: lay them out in memory exactly in the order I declared them, byte after byte.”

This means the struct’s in-memory layout matches the on-disk layout exactly. You can read the entire boot sector in one call:

```c
BPB bpb;
fread(&bpb, sizeof(BPB), 1, fp); /* read 62 raw bytes into bpb */
printf("Sector size: %u\n", bpb.bytes_per_sector); /* 512 */
```

Here is what happens in memory. The raw bytes from the disk land directly in the struct, and each field “overlays” the right portion:

| jump[3] | oem_name[8] | bps | spc | rsv | nfats |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `eb 3c 90` | `6d 6b 66 73 2e 66 61 74` | `00 02` | `04` | `01 00` | `02` |
| 0x00 | 0x03 | 0x0B | 0x0D | 0x0E | 0x10 |

`bps` = `bytes_per_sector`, `spc` = `sectors_per_clus`, `rsv` = `reserved_sectors`, `nfats` = `num_fats`

After the `fread()`, `bpb.bytes_per_sector` contains the two bytes at offsets 11-12 (`00 02`), interpreted as a 16-bit little-endian integer: 512. `bpb.sectors_per_clus` is the single byte at offset 13 (`04`): 4. And so on.

The same trick works for directory entries: each `DirEntry` is exactly 32 bytes on disk, and the packed struct maps each field to the right offset.

**8.3 Filenames**

FAT16 stores file names as 8 bytes (name, space-padded) followed by 3 bytes (extension, space-padded), with no dot on disk. For example, the file HELLO.TXT is stored as:
`H E L L O _ _ _ T X T` (where `_` is space)

When displaying the name, strip trailing spaces and insert a dot between the name and extension (if the extension is non-empty): `HELLO.TXT`, `MAKEFILE`, `README.MD`.

**Directory Entry Attributes**

The `attr` byte in each directory entry is a bitmask. Several bits can be set at once:

| Flag | Value | Meaning |
| :--- | :--- | :--- |
| `ATTR_READ_ONLY` | 0x01 | File is read-only |
| `ATTR_HIDDEN` | 0x02 | File is hidden |
| `ATTR_SYSTEM` | 0x04 | Operating-system file |
| `ATTR_VOLUME_ID` | 0x08 | Volume label (not a real file) |
| `ATTR_DIRECTORY` | 0x10 | Entry is a subdirectory |
| `ATTR_ARCHIVE` | 0x20 | Archive (modified since backup) |
| `ATTR_LFN` | 0x0F | Long filename fragment |

Skip entries where `attr` has all four low bits set (`0x0F` - long-filename fragments), and skip volume-label entries (`ATTR_VOLUME_ID`).

**FAT16 Timestamps**

Dates and times are packed into 16-bit integers:
* **Date:** bits [15:9] = year − 1980, bits [8:5] = month (1-12), bits [4:0] = day (1-31).
* **Time:** bits [15:11] = hour (0-23), bits [10:5] = minute (0-59), bits [4:0] = seconds÷2 (0-29).

#### 1.3 Exercise 1: Volume Inspector

**Task**

Write `fat16_volume.c`. Open a FAT16 disk image, parse the BPB, and print a summary of the volume geometry.

**What to implement**
* `fat16_open(image_path)` - open the image, read the BPB, compute the four byte offsets (`fat_start`, `root_start`, `data_start`, `cluster_size`), and load the entire FAT into a heap-allocated `uint16_t` array.
* `fat16_close(fs)` - free the FAT buffer, close the file, free the struct.
* `print_volume_info(fs, image_path)` - print the formatted summary.

**Output format**
```text
Image      : <filename>
Volume     : <label>
FS type    : <type>
Bytes/sec  : <N>
Sec/cluster: <N> (<N> bytes/cluster)
FATs       : <N> (<N> sectors each)
Root cap   : <N> entries
Total sec  : <N>
Clusters   : <N>
```

**Notes:**
* **Volume label**: read from `bpb.volume_label` (11 space-padded chars), with trailing spaces trimmed; if all spaces, print `(none)`.
* **FS type**: read from `bpb.fs_type` (8 space-padded chars), trimmed.
* **Total sectors**: use `total_sectors_16` if non-zero, else `total_sectors_32`.
* **Clusters**: `data_sectors ÷ sectors_per_clus`, where `data_sectors = total_sectors - (data_start ÷ bytes_per_sector)`.

**Example**
Input: `test.img`
Output:
```text
Image      : test.img
Volume     : TESTDISK
FS type    : FAT
Bytes/sec  : 512
Sec/cluster: 4 (2048 bytes/cluster)
FATs       : 2 (128 sectors each)
Root cap   : 512 entries
Total sec  : 65536
Clusters   : 7933
```

#### 1.4 Exercise 2: Root Directory Listing

**Task**

Write `fat16_ls.c`. Building on Exercise 1, read the root directory from the image and print a listing of all files and subdirectories it contains.

**What to implement (new)**
* `read_root_directory(fs, &count)` - seek to `root_start`, read `root_entry_count` entries into a heap buffer.
* `build_name(entry, out)` - reconstruct "NAME.EXT" from the raw 8.3 fields.
* `decode_date(d, &yr, &mo, &dy)` and `decode_time(t, &hr, &mn, &sc)` - unpack 16-bit FAT timestamps.
* `print_attrs(attr, buf)` - format attribute byte as a 5-character string: `d` for directory, `r` read-only, `h` hidden, `s` system, `a` archive; dash if unset.
* `list_root(fs, entries, count)` - iterate entries and print each one.

**Skipping entries**
The following directory entries must be skipped during listing:
* First byte `0x00` - end of directory (stop iterating entirely).
* First byte `0xE5` - deleted entry.
* Attribute byte equals `0x0F` - long filename (LFN) fragment.
* `ATTR_VOLUME_ID` is set - volume label, not a real file.
* Name starts with `'.'` - dot and dotdot entries.

**Output format**

After the volume summary, print a tree-style listing. The output uses ASCII tree connectors: `|--` for non-last entries and `\--` for the last entry at each level.

```text
/
|-- [attrs] NAME size=FILESIZE (YYYY-MM-DD HH:MM:SS)
\-- [attrs] NAME/ (YYYY-MM-DD HH:MM:SS)

Total: N file(s), B byte(s)
```
* The root is printed as `/` on the first line.
* Directories get a trailing `/` after the name.
* Files show their size as `size=N`.
* Use `|--` for all entries except the last in each directory, which uses `\--`.
* The total line counts only regular files (not directories).

**Example**
Given root entries `HELLO.TXT` (12 bytes), `README.TXT` (11 bytes), `SUBDIR/` (directory):
```text
/
|-- [----a] HELLO.TXT size=12 (2025-06-15 14:30:00)
|-- [----a] README.TXT size=11 (2025-06-15 14:30:00)
\-- [d----] SUBDIR/ (2025-06-15 14:30:00)

Total: 2 file(s), 23 byte(s)
```

#### 1.5 Exercise 3: File Reader

**Task**

Write `fat16_cat.c`. Given a filename in the root directory, follow its cluster chain through the FAT and print the file’s contents to `stdout`.

**Usage**
`./fat16_cat <image.img> <filename>`

**What to implement (new)**
* `cluster_offset(fs, cluster)` - convert a cluster number to its byte offset: `data_start + (cluster - 2) * cluster_size`.
* `find_in_root(entries, count, filename)` - search root for matching name case-insensitively.
* `cat_file(fs, entry)` - follow FAT chain and print exactly `file_size` bytes to `stdout`.

**Cluster chain traversal**
Start at the first cluster number. For each cluster:
1. Compute the byte offset with `cluster_offset()`.
2. Seek to that offset.
3. Read `min(cluster_size, remaining_bytes)` into a buffer.
4. Write those bytes to `stdout`.
5. Look up the next cluster: `next = fs->fat[current]`.
6. Stop when `next >= FAT16_EOC` (value `0xFFF8` or higher).

> **Alert!**
> The last cluster may contain padding bytes beyond the actual file size. You must print exactly `file_size` bytes.

**Error handling**
* File not found: print `Error: '<name>' not found` to stdout, exit 0.
* Target is a directory: print `Error: '<name>' is a directory` to stdout, exit 0.
* Empty file (size 0): print nothing, exit 0.

**Example**
```bash
$ ./fat16_cat test.img HELLO.TXT
hello world
```

#### 1.6 Exercise 4: Recursive Directory Traversal

**Task**

Write `fat16_tree.c`. Extend your reader to navigate subdirectories and either (a) list the full tree recursively, or (b) resolve a path like `/SUBDIR/DEEP/FILE.TXT` and print its contents.

**Usage**
```bash
./fat16_tree <image.img>              # list entire tree
./fat16_tree <image.img> /path/to/file # cat a nested file
```

**What to implement (new)**
* `read_cluster_chain(fs, start, &out_size)` - load all data from a cluster chain into a single heap buffer.
* `find_in_dir(entries, count, component)` - version of `find_in_root()` for any directory array.
* `list_directory(fs, entries, count, prefix, depth)` - recursively print the directory tree.
* `resolve_path(fs, root, count, path)` - tokenize the path by `/` and walk to target.

**Subdirectory contents**
Subdirectories store entries in the data region as cluster chains.
1. Get first cluster from `entry->first_clus_lo`.
2. Call `read_cluster_chain()` to load clusters into a buffer.
3. Cast to `(DirEntry *)` and compute count: `chain_size / sizeof(DirEntry)`.
4. Process entries.
5. Free the buffer.

**Tree-style output**
Use `|--` for non-last and `\--` for the last entry. For deeper levels, use `|   ` (pipe + 3 spaces) to continue a vertical line, or `    ` (4 spaces) if the parent was the last entry.

**Example (ls mode)**
```text
/
|-- [----a] HELLO.TXT size=12 (2025-06-15 14:30:00)
|-- [----a] README.TXT size=11 (2025-06-15 14:30:00)
\-- [d----] SUBDIR/ (2025-06-15 14:30:00)
|   |-- [----a] NESTED.TXT size=12 (2025-06-15 14:30:00)
|   \-- [d----] DEEP/ (2025-06-15 14:30:00)
|       \-- [----a] DEEP.TXT size=10 (2025-06-15 14:30:00)

Total: 4 file(s), 45 byte(s)
```

#### 1.7 Exercise 5: File Writer

**Task**

Write `fat16_write.c`. Create a new file in the root directory. This requires modifying the FAT, writing data, creating a directory entry, and flushing to the image.

**Usage**
```bash
echo "hello world" | ./fat16_write test.img write HELLO.TXT
./fat16_write test.img ls
./fat16_write test.img cat HELLO.TXT
```

**What to implement (new)**
* `find_free_cluster(fs, total_clusters)` - scan FAT for `FAT16_FREE` (0).
* `allocate_clusters(fs, file_size, total_clusters, &first)` - update in-memory FAT and link chain with `FAT16_EOC`.
* `write_data(fs, first_cluster, data, size)` - write bytes to clusters on disk.
* `create_dir_entry(root, count, filename, first_clus, size)` - find free slot (`0x00` or `0xE5`) and fill metadata.
* `flush_fat(fs)` - write modified FAT back to all copies.
* `flush_root(fs, root, count)` - write root back to image.
* `fat16_write_file(fs, filename, data, size)` - orchestrate the steps.

> **Alert!**
> For this exercise, open the image in `"r+b"` mode (read and write).

**Error handling**
* File already exists: `Error: '<name>' already exists`
* Disk full: `Error: disk full`
* Root full: `Error: root directory full`
* Invalid filename: `Error: invalid filename '<name>'`

**Write procedure summary**
1. Read content from stdin.
2. Read root directory.
3. Verify filename doesn't exist.
4. Allocate clusters in FAT.
5. Write data to clusters.
6. Create entry in root.
7. Flush FAT and root to disk.

**Provided helpers**
* `make_83_name(filename, name8, ext3)`
* `encode_fat_date(year, month, day)`
* `encode_fat_time(hour, min, sec)`

#### 1.8 Appendix: Creating Test Images

Using `dosfstools`:
```bash
# Create a 32 MB image
dd if=/dev/zero of=test.img bs=512 count=65536
# Format as FAT16
mkfs.fat -F 16 -n TESTDISK test.img
# Mount (requires root)
mkdir -p mnt
sudo mount -o loop test.img mnt
# Add files
echo "hello world" | sudo tee mnt/HELLO.TXT > /dev/null
sudo umount mnt
```

Using `mtools` (no root needed):
```bash
dd if=/dev/zero of=test.img bs=512 count=65536
mkfs.fat -F 16 -n TESTDISK test.img
echo "hello world" | mcopy -i test.img - ::HELLO.TXT
```

### 2. Part II: Theoretical Exercises (Brightspace)


**Question 1 (4 points).** A computer uses a programmable clock in square-wave mode. If a 500 MHz crystal is used, what should be the value of the holding register to achieve a clock resolution of:
(a) a millisecond?
(b) 100 microseconds?

**Question 2 (4 points).** In the discussion on journaling, it was shown that the disk can be recovered to a consistent state if a CPU crash occurs during a write. Does this property hold if the CPU crashes again during a recovery procedure? Explain.

**Question 3 (4 points).** Compare RAID level 0 through 5 with respect to read performance, write performance, space overhead, and reliability.

**Question 4 (4 points).** CPU architects know that OS writers hate imprecise interrupts. One way to please them is to stop issuing new instructions when an interrupt is signaled but allow current ones to finish. Does this approach have disadvantages? Explain.

**Question 5 (4 points).** One mode that some DMA controllers use is to have the device controller send the word to the DMA controller, which then issues a second bus request to write to memory. How can this be used for memory to memory copy? Discuss advantages/disadvantages vs using the CPU.
