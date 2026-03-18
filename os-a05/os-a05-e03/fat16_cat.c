#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fat16_types.h"

static char to_upper_ascii(char character) {
  if (character >= 'a' && character <= 'z') {
    return (char)(character - ('a' - 'A'));
  }
  return character;
}

static int equals_ignore_case_ascii(const char *left, const char *right) {
  size_t index;

  for (index = 0;; index++) {
    char left_char;
    char right_char;

    left_char = to_upper_ascii(left[index]);
    right_char = to_upper_ascii(right[index]);

    if (left_char != right_char) {
      return 0;
    }

    if (left[index] == '\0' && right[index] == '\0') {
      return 1;
    }
  }
}

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

static void build_name(const DirEntry *entry, char *output_name,
                       size_t output_size) {
  char name_part[9];
  char extension_part[4];

  copy_trimmed(name_part, sizeof(name_part), entry->name, sizeof(entry->name));
  copy_trimmed(extension_part, sizeof(extension_part), entry->ext,
               sizeof(entry->ext));

  if (extension_part[0] != '\0') {
    snprintf(output_name, output_size, "%s.%s", name_part, extension_part);
  } else {
    snprintf(output_name, output_size, "%s", name_part);
  }
}

static int should_skip_entry(const DirEntry *entry) {
  uint8_t first_char;

  first_char = (uint8_t)entry->name[0];
  if (first_char == 0xE5) {
    return 1;
  }
  if ((entry->attr & ATTR_LFN) == ATTR_LFN) {
    return 1;
  }
  if ((entry->attr & ATTR_VOLUME_ID) != 0) {
    return 1;
  }

  return 0;
}

void fat16_open(FAT16 *fat, const char *image_path) {
  BPB bpb;
  FILE *file_pointer;
  uint32_t fat_size_bytes;

  memset(&bpb, 0, sizeof(BPB));
  memset(fat, 0, sizeof(FAT16));

  file_pointer = fopen(image_path, "rb");
  if (file_pointer == NULL) {
    char fallback_path[512];
    snprintf(fallback_path, sizeof(fallback_path), "imgs/%s", image_path);
    file_pointer = fopen(fallback_path, "rb");
  }
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
  fat->fat = (uint16_t *)malloc(fat_size_bytes);
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

static DirEntry *read_root_directory(const FAT16 *fs, size_t *entry_count) {
  size_t entries_to_read;
  DirEntry *entries;

  entries_to_read = fs->bpb.root_entry_count;
  entries = (DirEntry *)calloc(entries_to_read, sizeof(DirEntry));
  if (entries == NULL) {
    return NULL;
  }

  if (fseek(fs->fp, (long)fs->root_start, SEEK_SET) != 0) {
    free(entries);
    return NULL;
  }

  if (fread(entries, sizeof(DirEntry), entries_to_read, fs->fp) !=
      entries_to_read) {
    free(entries);
    return NULL;
  }

  *entry_count = entries_to_read;
  return entries;
}

uint32_t cluster_offset(const FAT16 *fs, uint16_t cluster) {
  return fs->data_start + (uint32_t)(cluster - 2) * fs->cluster_size;
}

const DirEntry *find_in_root(const DirEntry *entries, size_t count,
                             const char *filename) {
  size_t index;

  for (index = 0; index < count; index++) {
    char built_name[13];

    if ((uint8_t)entries[index].name[0] == 0x00) {
      break;
    }
    if (should_skip_entry(&entries[index])) {
      continue;
    }

    build_name(&entries[index], built_name, sizeof(built_name));
    if (equals_ignore_case_ascii(built_name, filename)) {
      return &entries[index];
    }
  }

  return NULL;
}

void cat_file(FAT16 *fs, const DirEntry *entry) {
  uint32_t remaining_bytes;
  uint16_t current_cluster;
  uint32_t fat_entry_count;
  uint32_t visited_clusters;
  uint8_t *buffer;

  remaining_bytes = entry->file_size;
  if (remaining_bytes == 0) {
    return;
  }

  current_cluster = entry->first_clus_lo;
  if (current_cluster < 2) {
    return;
  }

  fat_entry_count =
      ((uint32_t)fs->bpb.fat_size_16 * fs->bpb.bytes_per_sector) / 2;
  visited_clusters = 0;

  buffer = (uint8_t *)malloc(fs->cluster_size);
  if (buffer == NULL) {
    return;
  }

  while (remaining_bytes > 0 && current_cluster < FAT16_EOC) {
    uint32_t offset;
    uint32_t bytes_this_cluster;

    if (current_cluster >= fat_entry_count) {
      break;
    }

    offset = cluster_offset(fs, current_cluster);
    if (fseek(fs->fp, (long)offset, SEEK_SET) != 0) {
      break;
    }

    bytes_this_cluster = fs->cluster_size;
    if (remaining_bytes < bytes_this_cluster) {
      bytes_this_cluster = remaining_bytes;
    }

    if (fread(buffer, 1, bytes_this_cluster, fs->fp) != bytes_this_cluster) {
      break;
    }

    fwrite(buffer, 1, bytes_this_cluster, stdout);
    remaining_bytes -= bytes_this_cluster;

    if (remaining_bytes == 0) {
      break;
    }

    current_cluster = fs->fat[current_cluster];
    visited_clusters++;
    if (visited_clusters > fat_entry_count) {
      break;
    }
  }

  free(buffer);
}

int main(int argc, char **argv) {
  FAT16 filesystem;
  DirEntry *root_entries;
  const DirEntry *target_entry;
  size_t root_entry_count;
  const char *image_path;
  const char *filename;

  if (argc < 3) {
    printf("No input received\n");
    return 1;
  }

  image_path = argv[1];
  filename = argv[2];

  fat16_open(&filesystem, image_path);
  if (filesystem.fp == NULL) {
    return 1;
  }

  root_entries = read_root_directory(&filesystem, &root_entry_count);
  if (root_entries == NULL) {
    fat16_close(&filesystem);
    return 1;
  }

  target_entry = find_in_root(root_entries, root_entry_count, filename);
  if (target_entry == NULL) {
    printf("Error: '%s' not found\n", filename);
    free(root_entries);
    fat16_close(&filesystem);
    return 0;
  }

  if ((target_entry->attr & ATTR_DIRECTORY) != 0) {
    printf("Error: '%s' is a directory\n", filename);
    free(root_entries);
    fat16_close(&filesystem);
    return 0;
  }

  cat_file(&filesystem, target_entry);

  free(root_entries);
  fat16_close(&filesystem);
  return 0;
}
