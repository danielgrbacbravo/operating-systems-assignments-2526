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

static void build_name(const DirEntry *entry, char *output_name) {
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

static void decode_date(uint16_t date_value, int *year, int *month, int *day) {
  *year = FAT_YEAR_BASE +
          ((date_value >> FAT_DATE_YEAR_SHIFT) & FAT_DATE_YEAR_MASK);
  *month = (date_value >> FAT_DATE_MONTH_SHIFT) & FAT_DATE_MONTH_MASK;
  *day = date_value & FAT_DATE_DAY_MASK;
}

static void decode_time(uint16_t time_value, int *hour, int *minute,
                        int *second) {
  *hour = (time_value >> FAT_TIME_HOUR_SHIFT) & FAT_TIME_HOUR_MASK;
  *minute = (time_value >> FAT_TIME_MINUTE_SHIFT) & FAT_TIME_MINUTE_MASK;
  *second = (time_value & FAT_TIME_SECOND_MASK) * FAT_TIME_SECOND_MULTIPLIER;
}

static void print_attrs(uint8_t attr, char *attrs) {
  attrs[0] = (attr & ATTR_DIRECTORY) ? 'd' : '-';
  attrs[1] = (attr & ATTR_READ_ONLY) ? 'r' : '-';
  attrs[2] = (attr & ATTR_HIDDEN) ? 'h' : '-';
  attrs[3] = (attr & ATTR_SYSTEM) ? 's' : '-';
  attrs[4] = (attr & ATTR_ARCHIVE) ? 'a' : '-';
  attrs[5] = '\0';
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

static uint32_t cluster_offset(const FAT16 *fs, uint16_t cluster) {
  return fs->data_start + (uint32_t)(cluster - 2) * fs->cluster_size;
}

uint8_t *read_cluster_chain(const FAT16 *fs, uint16_t start, size_t *out_size) {
  uint16_t current_cluster;
  uint32_t fat_entry_count;
  uint32_t visited_clusters;
  uint8_t *buffer;
  size_t buffer_size;

  *out_size = 0;
  if (start < 2) {
    return NULL;
  }

  fat_entry_count =
      ((uint32_t)fs->bpb.fat_size_16 * fs->bpb.bytes_per_sector) / 2;
  current_cluster = start;
  visited_clusters = 0;
  buffer = NULL;
  buffer_size = 0;

  while (current_cluster < FAT16_EOC) {
    uint8_t *new_buffer;
    uint32_t offset;

    if (current_cluster >= fat_entry_count) {
      break;
    }

    new_buffer = (uint8_t *)realloc(buffer, buffer_size + fs->cluster_size);
    if (new_buffer == NULL) {
      free(buffer);
      return NULL;
    }
    buffer = new_buffer;

    offset = cluster_offset(fs, current_cluster);
    if (fseek(fs->fp, (long)offset, SEEK_SET) != 0 ||
        fread(buffer + buffer_size, 1, fs->cluster_size, fs->fp) !=
            fs->cluster_size) {
      free(buffer);
      return NULL;
    }

    buffer_size += fs->cluster_size;
    current_cluster = fs->fat[current_cluster];
    visited_clusters++;
    if (visited_clusters > fat_entry_count) {
      break;
    }
  }

  *out_size = buffer_size;
  return buffer;
}

const DirEntry *find_in_dir(const DirEntry *entries, size_t count,
                            const char *component) {
  size_t index;

  for (index = 0; index < count; index++) {
    char built_name[13];

    if ((uint8_t)entries[index].name[0] == 0x00) {
      break;
    }
    if (should_skip_entry(&entries[index])) {
      continue;
    }

    build_name(&entries[index], built_name);
    if (equals_ignore_case_ascii(built_name, component)) {
      return &entries[index];
    }
  }

  return NULL;
}

static size_t collect_visible_entries(const DirEntry *entries, size_t count,
                                      size_t *visible_indices) {
  size_t index;
  size_t visible_count;

  visible_count = 0;
  for (index = 0; index < count; index++) {
    if ((uint8_t)entries[index].name[0] == 0x00) {
      break;
    }
    if (should_skip_entry(&entries[index])) {
      continue;
    }
    visible_indices[visible_count] = index;
    visible_count++;
  }

  return visible_count;
}

void list_directory(FAT16 *fs, const DirEntry *entries, size_t count,
                    const char *prefix, int is_root) {
  size_t *visible_indices;
  size_t visible_count;
  size_t visible_position;

  visible_indices = (size_t *)malloc(count * sizeof(size_t));
  if (visible_indices == NULL) {
    return;
  }

  visible_count = collect_visible_entries(entries, count, visible_indices);

  if (is_root) {
    printf("\n/\n");
  }

  for (visible_position = 0; visible_position < visible_count;
       visible_position++) {
    const DirEntry *entry;
    char name[13];
    char attrs[6];
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int is_last_entry;
    int is_directory;

    entry = &entries[visible_indices[visible_position]];
    is_last_entry = (visible_position + 1 == visible_count);
    is_directory = (entry->attr & ATTR_DIRECTORY) != 0;

    build_name(entry, name);
    print_attrs(entry->attr, attrs);
    decode_date(entry->wrt_date, &year, &month, &day);
    decode_time(entry->wrt_time, &hour, &minute, &second);

    printf("%s", prefix);
    if (is_last_entry) {
      printf("\\-- ");
    } else {
      printf("|-- ");
    }

    if (is_directory) {
      printf("[%s] %s/  (%04d-%02d-%02d %02d:%02d:%02d)\n", attrs, name, year,
             month, day, hour, minute, second);
    } else {
      fs->total_files++;
      fs->total_bytes += entry->file_size;
      printf("[%s] %s  size=%-11u (%04d-%02d-%02d %02d:%02d:%02d)\n", attrs,
             name, entry->file_size, year, month, day, hour, minute, second);
    }

    if (is_directory) {
      uint8_t *subdir_buffer;
      size_t subdir_size;
      size_t subdir_count;
      char child_prefix[512];

      subdir_buffer =
          read_cluster_chain(fs, entry->first_clus_lo, &subdir_size);
      if (subdir_buffer == NULL) {
        continue;
      }

      subdir_count = subdir_size / sizeof(DirEntry);

      snprintf(child_prefix, sizeof(child_prefix), "%s%s", prefix,
               is_last_entry ? "    " : "|   ");
      list_directory(fs, (const DirEntry *)subdir_buffer, subdir_count,
                     child_prefix, 0);
      free(subdir_buffer);
    }
  }

  if (is_root) {
    printf("\nTotal: %u file(s),  %llu byte(s)\n", fs->total_files,
           (unsigned long long)fs->total_bytes);
  }

  free(visible_indices);
}

static int resolve_next_component(const char **cursor, char *component,
                                  size_t component_size) {
  size_t length;

  while (**cursor == '/') {
    (*cursor)++;
  }

  if (**cursor == '\0') {
    return 0;
  }

  length = 0;
  while (**cursor != '\0' && **cursor != '/') {
    if (length + 1 < component_size) {
      component[length] = **cursor;
      length++;
    }
    (*cursor)++;
  }
  component[length] = '\0';

  return 1;
}

int resolve_path(FAT16 *fs, const DirEntry *root_entries, size_t root_count,
                 const char *path, DirEntry *resolved_entry,
                 char *missing_component, size_t missing_size) {
  const DirEntry *current_entries;
  size_t current_count;
  uint8_t *owned_entries;
  const char *cursor;
  char component[128];

  current_entries = root_entries;
  current_count = root_count;
  owned_entries = NULL;
  cursor = path;

  while (resolve_next_component(&cursor, component, sizeof(component))) {
    const DirEntry *found_entry;
    int has_more;

    found_entry = find_in_dir(current_entries, current_count, component);
    if (found_entry == NULL) {
      snprintf(missing_component, missing_size, "%s", component);
      free(owned_entries);
      return 0;
    }

    while (*cursor == '/') {
      cursor++;
    }
    has_more = (*cursor != '\0');

    if (!has_more) {
      *resolved_entry = *found_entry;
      free(owned_entries);
      return 1;
    }

    if ((found_entry->attr & ATTR_DIRECTORY) == 0) {
      snprintf(missing_component, missing_size, "%s", component);
      free(owned_entries);
      return 0;
    }

    {
      uint8_t *next_entries;
      size_t next_size;

      next_entries =
          read_cluster_chain(fs, found_entry->first_clus_lo, &next_size);
      if (next_entries == NULL) {
        snprintf(missing_component, missing_size, "%s", component);
        free(owned_entries);
        return 0;
      }

      free(owned_entries);
      owned_entries = next_entries;
      current_entries = (const DirEntry *)owned_entries;
      current_count = next_size / sizeof(DirEntry);
    }
  }

  snprintf(missing_component, missing_size, "%s", path);
  free(owned_entries);
  return 0;
}

static void cat_file(FAT16 *fs, const DirEntry *entry) {
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
  size_t root_entry_count;
  const char *image_path;
  const char *target_path;

  if (argc < 2) {
    printf("No input received\n");
    return 1;
  }

  image_path = argv[1];
  fat16_open(&filesystem, image_path);
  if (filesystem.fp == NULL) {
    return 1;
  }

  root_entries = read_root_directory(&filesystem, &root_entry_count);
  if (root_entries == NULL) {
    fat16_close(&filesystem);
    return 1;
  }

  if (argc == 2) {
    print_volume_info(&filesystem, image_path);
    list_directory(&filesystem, root_entries, root_entry_count, "", 1);
  } else {
    DirEntry target_entry;
    char missing_component[128];
    int resolved;

    target_path = argv[2];
    resolved = resolve_path(&filesystem, root_entries, root_entry_count,
                            target_path, &target_entry, missing_component,
                            sizeof(missing_component));
    if (!resolved) {
      printf("cat: '%s': no such file or directory\n", missing_component);
      free(root_entries);
      fat16_close(&filesystem);
      return 0;
    }

    if ((target_entry.attr & ATTR_DIRECTORY) != 0) {
      printf("cat: '%s': is a directory\n", target_path);
      free(root_entries);
      fat16_close(&filesystem);
      return 0;
    }

    cat_file(&filesystem, &target_entry);
  }

  free(root_entries);
  fat16_close(&filesystem);
  return 0;
}
