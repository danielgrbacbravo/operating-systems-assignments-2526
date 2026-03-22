#ifndef VDISK_H
#define VDISK_H

#define BLOCK_SIZE 4096
#define NUM_DISKS  4
#define MAX_BLOCKS 256

/*
 * Writes a 4KB buffer to a specific block on a specific disk.
 * Returns: 0 on success, -1 if the disk has suffered a hardware failure.
 */
int disk_write(int disk_id, int block_num, const char *buffer);

/*
 * Reads a 4KB block from a specific disk into the provided buffer.
 * Returns: 0 on success, -1 if the disk has suffered a hardware failure.
 */
int disk_read(int disk_id, int block_num, char *buffer);

/*
 * --- Controller functions (used by the test harness only) ---
 * Do NOT call these from your raid_controller.c implementation.
 */

/* Reset all disks: clear data, clear failures, reset stats. */
void disk_reset(void);

/* Simulate a hardware failure on the given disk. */
void disk_fail(int disk_id);

/* Print per-disk read/write statistics to stdout. */
void disk_print_stats(void);

#endif
