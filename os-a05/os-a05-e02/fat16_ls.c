#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fat16_types.h"

static void copy_trimmed(char *destination, size_t destination_size,
                         const char *source, size_t source_length) {
  size_t trimmed_length;

  if (destination_size == 0) {
    return;
  }

  trimmed_length = source_length;
  while (trimmed_length > 0 && source[trimmed_length - 1] == ' ') {
    trimmed_length--;
  }

  if (trimmed_length >= destination_size) {
    trimmed_length = destination_size - 1;
  }

  memcpy(destination, source, trimmed_length);
  destination[trimmed_length] = '\0';
}

void fat16_open(FAT16 *fat, const char *image_path) {
  BPB bpb;
  FILE *file_pointer;
  uint32_t fat_size_bytes;

  memset(&bpb, 0, sizeof(BPB));
  memset(fat, 0, sizeof(FAT16));

  file_pointer = fopen(image_path, "rb");
  if (file_pointer == NULL) {
    printf("Error: Unable to open file %s\n", image_path);
    return;
  }

  if (fread(&bpb, sizeof(BPB), 1, file_pointer) != 1) {
    printf("Error: Unable to read BPB from %s\n", image_path);
    fclose(file_pointer);
    return;
  }

  fat->fp = file_pointer;
  fat->bpb = bpb;
  fat->fat_start = (uint32_t)bpb.reserved_sectors * bpb.bytes_per_sector;
  fat->root_start = fat->fat_start + (uint32_t)bpb.num_fats * bpb.fat_size_16 *
                                         bpb.bytes_per_sector;
  fat->data_start = fat->root_start + bpb.root_entry_count * sizeof(DirEntry);
  fat->cluster_size = (uint32_t)bpb.bytes_per_sector * bpb.sectors_per_clus;

  fat_size_bytes = (uint32_t)bpb.fat_size_16 * bpb.bytes_per_sector;
  fat->fat = malloc(fat_size_bytes);
  if (fat->fat == NULL) {
    printf("Error: Unable to allocate FAT buffer\n");
    fclose(file_pointer);
    memset(fat, 0, sizeof(FAT16));
    return;
  }

  if (fseek(file_pointer, (long)fat->fat_start, SEEK_SET) != 0 ||
      fread(fat->fat, 1, fat_size_bytes, file_pointer) != fat_size_bytes) {
    printf("Error: Unable to read FAT from %s\n", image_path);
    free(fat->fat);
    fclose(file_pointer);
    memset(fat, 0, sizeof(FAT16));
    return;
  }
}

int fat16_close(FAT16 *fs) {
  if (fs == NULL) {
    return 1;
  }

  if (fs->fat != NULL) {
    free(fs->fat);
    fs->fat = NULL;
  }

  if (fs->fp != NULL) {
    fclose(fs->fp);
    fs->fp = NULL;
  }

  return 0;
}

void print_volume_info(FAT16 *fs, const char *image_path) {
  uint32_t total_sectors;
  uint32_t root_dir_sectors;
  uint32_t data_sectors;
  uint32_t total_clusters;
  char volume_label[12];
  char fs_type[9];

  copy_trimmed(volume_label, sizeof(volume_label), fs->bpb.volume_label,
               sizeof(fs->bpb.volume_label));
  copy_trimmed(fs_type, sizeof(fs_type), fs->bpb.fs_type,
               sizeof(fs->bpb.fs_type));

  if (volume_label[0] == '\0') {
    strcpy(volume_label, "(none)");
  }

  total_sectors = fs->bpb.total_sectors_16 != 0 ? fs->bpb.total_sectors_16
                                                : fs->bpb.total_sectors_32;

  root_dir_sectors = ((uint32_t)fs->bpb.root_entry_count * sizeof(DirEntry) +
                      fs->bpb.bytes_per_sector - 1) /
                     fs->bpb.bytes_per_sector;

  data_sectors =
      total_sectors -
      ((uint32_t)fs->bpb.reserved_sectors +
       (uint32_t)fs->bpb.num_fats * fs->bpb.fat_size_16 + root_dir_sectors);

  total_clusters = data_sectors / fs->bpb.sectors_per_clus;

  printf(" Image      : %s\n", image_path);
  printf(" Volume     : %s\n", volume_label);
  printf(" FS type    : %s\n", fs_type);
  printf(" Bytes/sec  : %u\n", fs->bpb.bytes_per_sector);
  printf(" Sec/cluster: %u  (%u bytes/cluster)\n", fs->bpb.sectors_per_clus,
         fs->cluster_size);
  printf(" FATs       : %u  (%u sectors each)\n", fs->bpb.num_fats,
         fs->bpb.fat_size_16);
  printf(" Root cap   : %u entries\n", fs->bpb.root_entry_count);
  printf(" Total sec  : %u\n", total_sectors);
  printf(" Clusters   : %u\n", total_clusters);
}

int main(int argc, char **argv) {
  FAT16 file_allocation_table;
  const char *image_path;

  if (argc < 2) {
    printf("No input received\n");
    return 1;
  }

  image_path = argv[1];

  fat16_open(&file_allocation_table, image_path);
  if (file_allocation_table.fp == NULL) {
    return 1;
  }

  print_volume_info(&file_allocation_table, image_path);
  fat16_close(&file_allocation_table);

  return 0;
}
