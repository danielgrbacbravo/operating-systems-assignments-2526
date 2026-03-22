#ifndef RAID_CONTROLLER_H
#define RAID_CONTROLLER_H


/*
 * Store `total_size` bytes by striping 4KB blocks round-robin
 * across all 4 disks.  Returns 0 on success, negative on error.
 */
int store_file_raid0(const char *data, int total_size);

/*
 * Read back a RAID-0 stored file into `buffer`.
 * Returns 0 on success.  If any disk fails, return -2 immediately.
 */
int read_file_raid0(char *buffer, int total_size);

/*
 * Store `total_size` bytes by writing every block to BOTH
 * Disk 0 and Disk 1.  Returns 0 on success, negative on error.
 */
int store_file_raid1(const char *data, int total_size);

/*
 * Read back a RAID-1 stored file.  Try Disk 0 first; on failure
 * fall back to Disk 1 transparently.  Returns 0 on success,
 * negative if both disks fail.
 */
int read_file_raid1(char *buffer, int total_size);

/*
 * Store `total_size` bytes using RAID-5 with left-asymmetric
 * parity rotation across 4 disks.  Returns 0 on success,
 * negative on error.
 */
int store_file_raid5(const char *data, int total_size);

/*
 * Read back a RAID-5 stored file.  If one disk has failed,
 * reconstruct the missing block(s) using XOR parity.
 * Returns 0 on success, negative on error.
 */
int read_file_raid5(char *buffer, int total_size);

#endif
