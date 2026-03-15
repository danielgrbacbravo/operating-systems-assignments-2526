#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fat16_types.h"

#define FAT_YEAR_BASE 1980

#define FAT_DATE_YEAR_SHIFT 9
#define FAT_DATE_YEAR_MASK 0x7F
#define FAT_DATE_MONTH_SHIFT 5
#define FAT_DATE_MONTH_MASK 0x0F
#define FAT_DATE_DAY_MASK 0x1F

#define FAT_TIME_HOUR_SHIFT 11
#define FAT_TIME_HOUR_MASK 0x1F
#define FAT_TIME_MINUTE_SHIFT 5
#define FAT_TIME_MINUTE_MASK 0x3F
#define FAT_TIME_SECOND_MASK 0x1F
#define FAT_TIME_SECOND_MULTIPLIER 2

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

void build_name(const DirEntry *entry, char *output_name) {
  char name_part[9];
  char extension_part[4];

  copy_trimmed(name_part, sizeof(name_part), entry->name, sizeof(entry->name));
  copy_trimmed(extension_part, sizeof(extension_part), entry->ext,
               sizeof(entry->ext));

  if (extension_part[0] != '\0') {
    snprintf(output_name, 13, "%s.%s", name_part, extension_part);
  } else {
    snprintf(output_name, 13, "%s", name_part);
  }
}

void decode_date(uint16_t date_value, int *year, int *month, int *day) {
  *year = FAT_YEAR_BASE +
          ((date_value >> FAT_DATE_YEAR_SHIFT) & FAT_DATE_YEAR_MASK);
  *month = (date_value >> FAT_DATE_MONTH_SHIFT) & FAT_DATE_MONTH_MASK;
  *day = date_value & FAT_DATE_DAY_MASK;
}

void decode_time(uint16_t time_value, int *hour, int *minute, int *second) {
  *hour = (time_value >> FAT_TIME_HOUR_SHIFT) & FAT_TIME_HOUR_MASK;
  *minute = (time_value >> FAT_TIME_MINUTE_SHIFT) & FAT_TIME_MINUTE_MASK;
  *second = (time_value & FAT_TIME_SECOND_MASK) * FAT_TIME_SECOND_MULTIPLIER;
}

void print_attrs(uint8_t attr, char *attrs) {
  attrs[0] = (attr & ATTR_DIRECTORY) ? 'd' : '-';
  attrs[1] = (attr & ATTR_READ_ONLY) ? 'r' : '-';
  attrs[2] = (attr & ATTR_HIDDEN) ? 'h' : '-';
  attrs[3] = (attr & ATTR_SYSTEM) ? 's' : '-';
  attrs[4] = (attr & ATTR_ARCHIVE) ? 'a' : '-';
  attrs[5] = '\0';
}

int should_skip_entry(const DirEntry *entry) {
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

  if (entry->name[0] == '.') {
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
  const char *display_name;
  const char *last_slash;
  const char *last_backslash;
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

  display_name = image_path;
  last_slash = strrchr(image_path, '/');
  last_backslash = strrchr(image_path, '\\');
  if (last_slash != NULL && last_slash[1] != '\0') {
    display_name = last_slash + 1;
  }
  if (last_backslash != NULL && last_backslash[1] != '\0' &&
      last_backslash > display_name) {
    display_name = last_backslash + 1;
  }

  printf(" Image      : %s\n", display_name);
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

DirEntry *read_root_directory(const FAT16 *fs, size_t *entry_count) {
  size_t entries_to_read;
  DirEntry *entries;

  entries_to_read = fs->bpb.root_entry_count;
  entries = calloc(entries_to_read, sizeof(DirEntry));
  if (entries == NULL) {
    return NULL;
  }

  if (fseek(fs->fp, (long)fs->root_start, SEEK_SET) != 0) {
    printf("Error: unable to seek to root_start\n");
    free(entries);
    return NULL;
  }

  if (fread(entries, sizeof(DirEntry), entries_to_read, fs->fp) !=
      entries_to_read) {
    printf("Error: unable to read root directory\n");
    free(entries);
    return NULL;
  }

  *entry_count = entries_to_read;
  return entries;
}

void list_root(FAT16 *fs, const DirEntry *entries, size_t count) {
  size_t index;
  size_t visible_count;
  size_t visible_index;

  printf("\n/\n");

  visible_count = 0;
  for (index = 0; index < count; index++) {
    if ((uint8_t)entries[index].name[0] == 0x00) {
      break;
    }
    if (should_skip_entry(&entries[index])) {
      continue;
    }
    visible_count++;
  }

  visible_index = 0;
  for (index = 0; index < count; index++) {
    char name[13];
    char attrs[6];
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int is_directory;

    if ((uint8_t)entries[index].name[0] == 0x00) {
      break;
    }

    if (should_skip_entry(&entries[index])) {
      continue;
    }

    build_name(&entries[index], name);
    print_attrs(entries[index].attr, attrs);
    decode_date(entries[index].wrt_date, &year, &month, &day);
    decode_time(entries[index].wrt_time, &hour, &minute, &second);

    is_directory = (entries[index].attr & ATTR_DIRECTORY) != 0;

    if (visible_index + 1 == visible_count) {
      printf("\\-- ");
    } else {
      printf("|-- ");
    }

    if (is_directory) {
      printf("[%s] %s/  (%04d-%02d-%02d %02d:%02d:%02d)\n", attrs, name, year,
             month, day, hour, minute, second);
    } else {
      fs->total_files++;
      fs->total_bytes += entries[index].file_size;
      printf("[%s] %s  size=%-11u (%04d-%02d-%02d %02d:%02d:%02d)\n", attrs,
             name, entries[index].file_size, year, month, day, hour, minute,
             second);
    }

    visible_index++;
  }

  printf("\nTotal: %u file(s),  %llu byte(s)\n", fs->total_files,
         (unsigned long long)fs->total_bytes);
}

int main(int argc, char **argv) {
  DirEntry *root_entries;
  FAT16 file_allocation_table;
  size_t root_entry_count;

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

  root_entries = read_root_directory(&file_allocation_table, &root_entry_count);
  if (root_entries == NULL) {
    fat16_close(&file_allocation_table);
    return 1;
  }

  list_root(&file_allocation_table, root_entries, root_entry_count);

  free(root_entries);
  fat16_close(&file_allocation_table);

  return 0;
}
