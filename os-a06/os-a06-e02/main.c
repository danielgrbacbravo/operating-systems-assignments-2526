/*
 * main.c,  Test harness for Exercise 2: RAID 1
 *
 * Input format (stdin):
 *   Line 1: DATA_SIZE  (integer, bytes)
 *   Next DATA_SIZE bytes: raw data (may contain newlines)
 *   Next line: FAIL_DISK (-1 = none, 0 or 1 = disk to kill before read)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vdisk.h"
#include "raid_controller.h"

int main(void)
{
    int data_size;
    if (scanf("%d", &data_size) != 1) {
        fprintf(stderr, "Error: could not read DATA_SIZE\n");
        return 1;
    }
    fgetc(stdin);

    char *data = calloc(data_size + 1, 1);
    if (!data) { perror("calloc"); return 1; }

    if (data_size > 0) {
        if ((int)fread(data, 1, data_size, stdin) != data_size) {
            fprintf(stderr, "Error: could not read DATA bytes\n");
            free(data);
            return 1;
        }
    }

    int fail_disk;
    if (scanf("%d", &fail_disk) != 1) fail_disk = -1;

    disk_reset();
    int rc = store_file_raid1(data, data_size);
    printf("Store Result: %d\n", rc);

    if (fail_disk >= 0 && fail_disk < NUM_DISKS)
        disk_fail(fail_disk);

    char *buf = calloc(data_size + 1, 1);
    if (!buf) { perror("calloc"); free(data); return 1; }

    rc = read_file_raid1(buf, data_size);
    printf("Read Result: %d\n", rc);

    if (rc == 0) {
        printf("Data: ");
        fwrite(buf, 1, data_size, stdout);
        printf("\n");
    }

    disk_print_stats();

    free(data);
    free(buf);
    return 0;
}
