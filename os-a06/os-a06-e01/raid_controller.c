#include "vdisk.h"
#include <string.h>

int store_file_raid0(const char *data, int total_size) {
  int total_blocks;
  int logical_block_index;
  char block_buffer[BLOCK_SIZE];

  total_blocks = (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

  for (logical_block_index = 0; logical_block_index < total_blocks;
       logical_block_index++) {
    int disk_id;
    int disk_block_index;
    int data_offset;
    int bytes_in_this_block;

    disk_id = logical_block_index % NUM_DISKS;
    disk_block_index = logical_block_index / NUM_DISKS;
    data_offset = logical_block_index * BLOCK_SIZE;
    bytes_in_this_block = total_size - data_offset;
    if (bytes_in_this_block > BLOCK_SIZE) {
      bytes_in_this_block = BLOCK_SIZE;
    }

    memset(block_buffer, 0, sizeof(block_buffer));
    if (bytes_in_this_block > 0) {
      memcpy(block_buffer, data + data_offset, (size_t)bytes_in_this_block);
    }

    if (disk_write(disk_id, disk_block_index, block_buffer) != 0) {
      return -1;
    }
  }

  return 0;
}

int read_file_raid0(char *buffer, int total_size) {
  int total_blocks;
  int logical_block_index;
  char block_buffer[BLOCK_SIZE];

  total_blocks = (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

  for (logical_block_index = 0; logical_block_index < total_blocks;
       logical_block_index++) {
    int disk_id;
    int disk_block_index;
    int data_offset;
    int bytes_in_this_block;

    disk_id = logical_block_index % NUM_DISKS;
    disk_block_index = logical_block_index / NUM_DISKS;

    if (disk_read(disk_id, disk_block_index, block_buffer) != 0) {
      return -2;
    }

    data_offset = logical_block_index * BLOCK_SIZE;
    bytes_in_this_block = total_size - data_offset;
    if (bytes_in_this_block > BLOCK_SIZE) {
      bytes_in_this_block = BLOCK_SIZE;
    }

    if (bytes_in_this_block > 0) {
      memcpy(buffer + data_offset, block_buffer, (size_t)bytes_in_this_block);
    }
  }

  return 0;
}
