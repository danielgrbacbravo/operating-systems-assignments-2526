#include "vdisk.h"
#include <stdio.h>
#include <string.h>

static char  disk_data[NUM_DISKS][MAX_BLOCKS][BLOCK_SIZE];
static int disk_failed[NUM_DISKS];
static int disk_reads_cnt[NUM_DISKS];
static int disk_writes_cnt[NUM_DISKS];


int disk_write(int disk_id, int block_num, const char *buffer)
{
    if (disk_id < 0 || disk_id >= NUM_DISKS)  return -1;
    if (block_num < 0 || block_num >= MAX_BLOCKS) return -1;
    if (disk_failed[disk_id]) return -1;

    memcpy(disk_data[disk_id][block_num], buffer, BLOCK_SIZE);
    disk_writes_cnt[disk_id]++;
    return 0;
}

int disk_read(int disk_id, int block_num, char *buffer)
{
    if (disk_id < 0 || disk_id >= NUM_DISKS)  return -1;
    if (block_num < 0 || block_num >= MAX_BLOCKS) return -1;
    if (disk_failed[disk_id]) return -1;

    memcpy(buffer, disk_data[disk_id][block_num], BLOCK_SIZE);
    disk_reads_cnt[disk_id]++;
    return 0;
}


void disk_reset(void)
{
    memset(disk_data, 0, sizeof(disk_data));
    memset(disk_failed, 0, sizeof(disk_failed));
    memset(disk_reads_cnt, 0, sizeof(disk_reads_cnt));
    memset(disk_writes_cnt, 0, sizeof(disk_writes_cnt));
}

void disk_fail(int disk_id)
{
    if (disk_id >= 0 && disk_id < NUM_DISKS)
        disk_failed[disk_id] = 1;
}

void disk_print_stats(void)
{
    printf("=== DISK STATS ===\n");
    for (int i = 0; i < NUM_DISKS; i++)
        printf("Disk %d: R=%d W=%d\n", i, disk_reads_cnt[i], disk_writes_cnt[i]);
}
