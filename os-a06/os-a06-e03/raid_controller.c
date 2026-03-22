#include "vdisk.h"
#include <string.h>

static int parity_disk_for_stripe(int stripe_index) {
  return (NUM_DISKS - 1) - (stripe_index % NUM_DISKS);
}

int store_file_raid5(const char *data, int total_size) {
  int data_blocks_per_stripe;
  int total_data_blocks;
  int total_stripes;
  int stripe_index;
  int logical_data_block_index;

  data_blocks_per_stripe = NUM_DISKS - 1;
  total_data_blocks = (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  total_stripes =
      (total_data_blocks + data_blocks_per_stripe - 1) / data_blocks_per_stripe;
  logical_data_block_index = 0;

  for (stripe_index = 0; stripe_index < total_stripes; stripe_index++) {
    int parity_disk;
    char stripe_blocks[NUM_DISKS][BLOCK_SIZE];
    int disk_id;

    parity_disk = parity_disk_for_stripe(stripe_index);
    memset(stripe_blocks, 0, sizeof(stripe_blocks));

    for (disk_id = 0; disk_id < NUM_DISKS; disk_id++) {
      if (disk_id == parity_disk) {
        continue;
      }

      if (logical_data_block_index < total_data_blocks) {
        int data_offset;
        int bytes_in_this_block;

        data_offset = logical_data_block_index * BLOCK_SIZE;
        bytes_in_this_block = total_size - data_offset;
        if (bytes_in_this_block > BLOCK_SIZE) {
          bytes_in_this_block = BLOCK_SIZE;
        }

        if (bytes_in_this_block > 0) {
          memcpy(stripe_blocks[disk_id], data + data_offset,
                 (size_t)bytes_in_this_block);
        }
      }

      logical_data_block_index++;
    }

    for (disk_id = 0; disk_id < NUM_DISKS; disk_id++) {
      int byte_index;

      if (disk_id == parity_disk) {
        continue;
      }

      for (byte_index = 0; byte_index < BLOCK_SIZE; byte_index++) {
        stripe_blocks[parity_disk][byte_index] ^=
            stripe_blocks[disk_id][byte_index];
      }
    }

    for (disk_id = 0; disk_id < NUM_DISKS; disk_id++) {
      if (disk_write(disk_id, stripe_index, stripe_blocks[disk_id]) != 0) {
        return -1;
      }
    }
  }

  return 0;
}

int read_file_raid5(char *buffer, int total_size) {
  int data_blocks_per_stripe;
  int total_data_blocks;
  int total_stripes;
  int stripe_index;
  int logical_data_block_index;

  data_blocks_per_stripe = NUM_DISKS - 1;
  total_data_blocks = (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;
  total_stripes =
      (total_data_blocks + data_blocks_per_stripe - 1) / data_blocks_per_stripe;
  logical_data_block_index = 0;

  for (stripe_index = 0; stripe_index < total_stripes; stripe_index++) {
    int parity_disk;
    char stripe_blocks[NUM_DISKS][BLOCK_SIZE];
    int disk_id;
    int failed_disk_id;
    int failed_count;

    parity_disk = parity_disk_for_stripe(stripe_index);
    failed_disk_id = -1;
    failed_count = 0;

    for (disk_id = 0; disk_id < NUM_DISKS; disk_id++) {
      if (disk_read(disk_id, stripe_index, stripe_blocks[disk_id]) != 0) {
        failed_count++;
        failed_disk_id = disk_id;
        memset(stripe_blocks[disk_id], 0, BLOCK_SIZE);
      }
    }

    if (failed_count > 1) {
      return -2;
    }

    if (failed_count == 1) {
      int recover_disk_id;
      int byte_index;

      recover_disk_id = failed_disk_id;
      memset(stripe_blocks[recover_disk_id], 0, BLOCK_SIZE);
      for (disk_id = 0; disk_id < NUM_DISKS; disk_id++) {
        if (disk_id == recover_disk_id) {
          continue;
        }
        for (byte_index = 0; byte_index < BLOCK_SIZE; byte_index++) {
          stripe_blocks[recover_disk_id][byte_index] ^=
              stripe_blocks[disk_id][byte_index];
        }
      }
    }

    for (disk_id = 0; disk_id < NUM_DISKS; disk_id++) {
      int data_offset;
      int bytes_in_this_block;

      if (disk_id == parity_disk) {
        continue;
      }
      if (logical_data_block_index >= total_data_blocks) {
        break;
      }

      data_offset = logical_data_block_index * BLOCK_SIZE;
      bytes_in_this_block = total_size - data_offset;
      if (bytes_in_this_block > BLOCK_SIZE) {
        bytes_in_this_block = BLOCK_SIZE;
      }

      if (bytes_in_this_block > 0) {
        memcpy(buffer + data_offset, stripe_blocks[disk_id],
               (size_t)bytes_in_this_block);
      }

      logical_data_block_index++;
    }
  }

  return 0;
}
