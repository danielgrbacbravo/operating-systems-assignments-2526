#include "vdisk.h"
#include <string.h>

int store_file_raid1(const char *data, int total_size) {
  int total_blocks;
  int block_index;
  char block_buffer[BLOCK_SIZE];

  total_blocks = (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

  for (block_index = 0; block_index < total_blocks; block_index++) {
    int data_offset;
    int bytes_in_this_block;

    data_offset = block_index * BLOCK_SIZE;
    bytes_in_this_block = total_size - data_offset;
    if (bytes_in_this_block > BLOCK_SIZE) {
      bytes_in_this_block = BLOCK_SIZE;
    }

    memset(block_buffer, 0, sizeof(block_buffer));
    if (bytes_in_this_block > 0) {
      memcpy(block_buffer, data + data_offset, (size_t)bytes_in_this_block);
    }

    if (disk_write(0, block_index, block_buffer) != 0) {
      return -1;
    }
    if (disk_write(1, block_index, block_buffer) != 0) {
      return -1;
    }
  }

  return 0;
}

int read_file_raid1(char *buffer, int total_size) {
  int total_blocks;
  int block_index;
  char block_buffer[BLOCK_SIZE];

  total_blocks = (total_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

  for (block_index = 0; block_index < total_blocks; block_index++) {
    int data_offset;
    int bytes_in_this_block;

    if (disk_read(0, block_index, block_buffer) != 0) {
      if (disk_read(1, block_index, block_buffer) != 0) {
        return -2;
      }
    }

    data_offset = block_index * BLOCK_SIZE;
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
