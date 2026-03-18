
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fat16_types.h"
#include "helpers.h"

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

static uint32_t cluster_offset(const FAT16 *fs, uint16_t cluster) {
  return fs->data_start + (uint32_t)(cluster - 2) * fs->cluster_size;
}

static uint32_t get_total_sectors(const FAT16 *fs) {
  if (fs->bpb.total_sectors_16 != 0) {
    return fs->bpb.total_sectors_16;
  }
  return fs->bpb.total_sectors_32;
}

static uint32_t get_total_clusters(const FAT16 *fs) {
  uint32_t total_sectors;
  uint32_t data_start_sector;
  uint32_t data_sectors;

  total_sectors = get_total_sectors(fs);
  data_start_sector = fs->data_start / fs->bpb.bytes_per_sector;
  if (total_sectors <= data_start_sector) {
    return 0;
  }

  data_sectors = total_sectors - data_start_sector;
  return data_sectors / fs->bpb.sectors_per_clus;
}

static uint32_t get_fat_entry_count(const FAT16 *fs) {
  return ((uint32_t)fs->bpb.fat_size_16 * fs->bpb.bytes_per_sector) /
         sizeof(uint16_t);
}

static int is_leap_year(int year) {
  if (year % 400 == 0) {
    return 1;
  }
  if (year % 100 == 0) {
    return 0;
  }
  return (year % 4 == 0);
}

static int parse_date_arg(const char *date_arg, int *year, int *month,
                          int *day) {
  int parsed_year;
  int parsed_month;
  int parsed_day;
  int days_in_month;

  if (sscanf(date_arg, "%d-%d-%d", &parsed_year, &parsed_month, &parsed_day) !=
      3) {
    return 0;
  }

  if (parsed_year < 1980 || parsed_year > 2107) {
    return 0;
  }
  if (parsed_month < 1 || parsed_month > 12) {
    return 0;
  }

  days_in_month = 31;
  if (parsed_month == 4 || parsed_month == 6 || parsed_month == 9 ||
      parsed_month == 11) {
    days_in_month = 30;
  } else if (parsed_month == 2) {
    days_in_month = is_leap_year(parsed_year) ? 29 : 28;
  }

  if (parsed_day < 1 || parsed_day > days_in_month) {
    return 0;
  }

  *year = parsed_year;
  *month = parsed_month;
  *day = parsed_day;
  return 1;
}

static int parse_time_arg(const char *time_arg, int *hour, int *minute,
                          int *second) {
  int parsed_hour;
  int parsed_minute;
  int parsed_second;

  if (sscanf(time_arg, "%d:%d:%d", &parsed_hour, &parsed_minute,
             &parsed_second) != 3) {
    return 0;
  }

  if (parsed_hour < 0 || parsed_hour > 23) {
    return 0;
  }
  if (parsed_minute < 0 || parsed_minute > 59) {
    return 0;
  }
  if (parsed_second < 0 || parsed_second > 59) {
    return 0;
  }

  *hour = parsed_hour;
  *minute = parsed_minute;
  *second = parsed_second;
  return 1;
}

void fat16_open(FAT16 *fat, const char *image_path) {
  BPB bpb;
  FILE *file_pointer;
  uint32_t fat_size_bytes;

  memset(&bpb, 0, sizeof(BPB));
  memset(fat, 0, sizeof(FAT16));

  file_pointer = fopen(image_path, "r+b");
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

  total_clusters = get_total_clusters(fs);

  printf(" Image      : %s\n", image_path);
  printf(" Volume     : %s\n", volume_label);
  printf(" FS type    : %s\n", fs_type);
  printf(" Bytes/sec  : %u\n", fs->bpb.bytes_per_sector);
  printf(" Sec/cluster: %u  (%u bytes/cluster)\n", fs->bpb.sectors_per_clus,
         fs->cluster_size);
  printf(" FATs       : %u  (%u sectors each)\n", fs->bpb.num_fats,
         fs->bpb.fat_size_16);
  printf(" Root cap   : %u entries\n", fs->bpb.root_entry_count);
  printf(" Total sec  : %u\n", get_total_sectors(fs));
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

static void list_root(FAT16 *fs, const DirEntry *entries, size_t count) {
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

    build_name(&entries[index], name, sizeof(name));
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

static const DirEntry *find_in_root(const DirEntry *entries, size_t count,
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

  fat_entry_count = get_fat_entry_count(fs);
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

int find_free_cluster(const FAT16 *fs, uint32_t total_clusters) {
  uint32_t fat_entry_count;
  uint32_t max_cluster_number;
  uint32_t cluster;

  fat_entry_count = get_fat_entry_count(fs);
  max_cluster_number = total_clusters + 1;

  if (max_cluster_number >= fat_entry_count) {
    max_cluster_number = fat_entry_count - 1;
  }

  for (cluster = 2; cluster <= max_cluster_number; cluster++) {
    if (fs->fat[cluster] == FAT16_FREE) {
      return (int)cluster;
    }
  }

  return -1;
}

int allocate_clusters(FAT16 *fs, uint32_t file_size, uint32_t total_clusters,
                      uint16_t *first) {
  uint32_t required_clusters;
  uint16_t *allocated_clusters;
  uint32_t index;

  *first = 0;
  if (file_size == 0) {
    return 1;
  }

  required_clusters = (file_size + fs->cluster_size - 1) / fs->cluster_size;
  allocated_clusters = (uint16_t *)malloc(required_clusters * sizeof(uint16_t));
  if (allocated_clusters == NULL) {
    return 0;
  }

  for (index = 0; index < required_clusters; index++) {
    int free_cluster;

    free_cluster = find_free_cluster(fs, total_clusters);
    if (free_cluster < 0) {
      free(allocated_clusters);
      return 0;
    }

    allocated_clusters[index] = (uint16_t)free_cluster;
    fs->fat[free_cluster] = FAT16_EOC;
  }

  for (index = 0; index < required_clusters; index++) {
    if (index + 1 < required_clusters) {
      fs->fat[allocated_clusters[index]] = allocated_clusters[index + 1];
    } else {
      fs->fat[allocated_clusters[index]] = FAT16_EOC;
    }
  }

  *first = allocated_clusters[0];
  free(allocated_clusters);
  return 1;
}

int write_data(FAT16 *fs, uint16_t first_cluster, const uint8_t *data,
               uint32_t size) {
  uint16_t current_cluster;
  uint32_t remaining_bytes;
  uint32_t fat_entry_count;
  uint32_t visited_clusters;

  if (size == 0) {
    return 1;
  }

  current_cluster = first_cluster;
  remaining_bytes = size;
  fat_entry_count = get_fat_entry_count(fs);
  visited_clusters = 0;

  while (remaining_bytes > 0 && current_cluster < FAT16_EOC) {
    uint32_t offset;
    uint32_t bytes_this_cluster;

    if (current_cluster < 2 || current_cluster >= fat_entry_count) {
      return 0;
    }

    offset = cluster_offset(fs, current_cluster);
    if (fseek(fs->fp, (long)offset, SEEK_SET) != 0) {
      return 0;
    }

    bytes_this_cluster = fs->cluster_size;
    if (remaining_bytes < bytes_this_cluster) {
      bytes_this_cluster = remaining_bytes;
    }

    if (fwrite(data + (size - remaining_bytes), 1, bytes_this_cluster,
               fs->fp) != bytes_this_cluster) {
      return 0;
    }

    remaining_bytes -= bytes_this_cluster;
    if (remaining_bytes == 0) {
      return 1;
    }

    current_cluster = fs->fat[current_cluster];
    visited_clusters++;
    if (visited_clusters > fat_entry_count) {
      return 0;
    }
  }

  return (remaining_bytes == 0);
}

int create_dir_entry(DirEntry *root, size_t count, const char *filename,
                     uint16_t first_clus, uint32_t size, int year, int month,
                     int day, int hour, int minute, int second) {
  size_t index;
  char name8[8];
  char ext3[3];

  if (make_83_name(filename, name8, ext3) != 0) {
    return -1;
  }

  for (index = 0; index < count; index++) {
    uint8_t first_byte;

    first_byte = (uint8_t)root[index].name[0];
    if (first_byte == 0x00 || first_byte == 0xE5) {
      memset(&root[index], 0, sizeof(DirEntry));
      memcpy(root[index].name, name8, sizeof(name8));
      memcpy(root[index].ext, ext3, sizeof(ext3));

      root[index].attr = ATTR_ARCHIVE;
      root[index].first_clus_lo = first_clus;
      root[index].file_size = size;
      root[index].wrt_date = encode_fat_date(year, month, day);
      root[index].wrt_time = encode_fat_time(hour, minute, second);
      root[index].crt_date = root[index].wrt_date;
      root[index].crt_time = root[index].wrt_time;
      root[index].acc_date = root[index].wrt_date;

      return 0;
    }
  }

  return -2;
}

int flush_fat(FAT16 *fs) {
  uint32_t fat_size_bytes;
  uint32_t fat_index;

  fat_size_bytes = (uint32_t)fs->bpb.fat_size_16 * fs->bpb.bytes_per_sector;

  for (fat_index = 0; fat_index < fs->bpb.num_fats; fat_index++) {
    uint32_t fat_copy_offset;

    fat_copy_offset = fs->fat_start + fat_index * fat_size_bytes;
    if (fseek(fs->fp, (long)fat_copy_offset, SEEK_SET) != 0) {
      return 0;
    }

    if (fwrite(fs->fat, 1, fat_size_bytes, fs->fp) != fat_size_bytes) {
      return 0;
    }
  }

  return 1;
}

int flush_root(FAT16 *fs, const DirEntry *root, size_t count) {
  if (fseek(fs->fp, (long)fs->root_start, SEEK_SET) != 0) {
    return 0;
  }

  if (fwrite(root, sizeof(DirEntry), count, fs->fp) != count) {
    return 0;
  }

  return 1;
}

int fat16_write_file(FAT16 *fs, const char *filename, const uint8_t *data,
                     uint32_t size, int year, int month, int day, int hour,
                     int minute, int second) {
  DirEntry *root_entries;
  size_t root_count;
  size_t index;
  char name8[8];
  char ext3[3];
  uint16_t first_cluster;
  uint32_t total_clusters;
  uint32_t fat_size_bytes;
  uint16_t *fat_backup;
  int created_result;

  if (make_83_name(filename, name8, ext3) != 0) {
    printf("Error: invalid filename '%s'\n", filename);
    return 0;
  }

  root_entries = read_root_directory(fs, &root_count);
  if (root_entries == NULL) {
    return 0;
  }

  for (index = 0; index < root_count; index++) {
    if ((uint8_t)root_entries[index].name[0] == 0x00) {
      break;
    }
    if ((uint8_t)root_entries[index].name[0] == 0xE5) {
      continue;
    }
    if ((root_entries[index].attr & ATTR_LFN) == ATTR_LFN) {
      continue;
    }

    if (memcmp(root_entries[index].name, name8, sizeof(name8)) == 0 &&
        memcmp(root_entries[index].ext, ext3, sizeof(ext3)) == 0) {
      printf("Error: '%s' already exists\n", filename);
      free(root_entries);
      return 0;
    }
  }

  fat_size_bytes = (uint32_t)fs->bpb.fat_size_16 * fs->bpb.bytes_per_sector;
  fat_backup = (uint16_t *)malloc(fat_size_bytes);
  if (fat_backup == NULL) {
    free(root_entries);
    return 0;
  }
  memcpy(fat_backup, fs->fat, fat_size_bytes);

  total_clusters = get_total_clusters(fs);
  if (!allocate_clusters(fs, size, total_clusters, &first_cluster)) {
    printf("Error: disk full\n");
    free(fat_backup);
    free(root_entries);
    return 0;
  }

  if (!write_data(fs, first_cluster, data, size)) {
    memcpy(fs->fat, fat_backup, fat_size_bytes);
    free(fat_backup);
    free(root_entries);
    return 0;
  }

  created_result =
      create_dir_entry(root_entries, root_count, filename, first_cluster, size,
                       year, month, day, hour, minute, second);
  if (created_result == -1) {
    printf("Error: invalid filename '%s'\n", filename);
    memcpy(fs->fat, fat_backup, fat_size_bytes);
    free(fat_backup);
    free(root_entries);
    return 0;
  }
  if (created_result == -2) {
    printf("Error: root directory full\n");
    memcpy(fs->fat, fat_backup, fat_size_bytes);
    free(fat_backup);
    free(root_entries);
    return 0;
  }

  if (!flush_fat(fs) || !flush_root(fs, root_entries, root_count)) {
    memcpy(fs->fat, fat_backup, fat_size_bytes);
    free(fat_backup);
    free(root_entries);
    return 0;
  }

  fflush(fs->fp);

  free(fat_backup);
  free(root_entries);
  return 1;
}

static uint8_t *read_stdin_all(uint32_t *out_size) {
  uint8_t *buffer;
  size_t buffer_capacity;
  size_t buffer_size;

  buffer = NULL;
  buffer_capacity = 0;
  buffer_size = 0;

  for (;;) {
    uint8_t chunk[4096];
    size_t bytes_read;

    bytes_read = fread(chunk, 1, sizeof(chunk), stdin);
    if (bytes_read == 0) {
      break;
    }

    if (buffer_size + bytes_read > buffer_capacity) {
      size_t new_capacity;
      uint8_t *new_buffer;

      new_capacity = buffer_capacity == 0 ? 4096 : buffer_capacity;
      while (new_capacity < buffer_size + bytes_read) {
        new_capacity *= 2;
      }

      new_buffer = (uint8_t *)realloc(buffer, new_capacity);
      if (new_buffer == NULL) {
        free(buffer);
        return NULL;
      }
      buffer = new_buffer;
      buffer_capacity = new_capacity;
    }

    memcpy(buffer + buffer_size, chunk, bytes_read);
    buffer_size += bytes_read;
  }

  *out_size = (uint32_t)buffer_size;
  return buffer;
}

int main(int argc, char **argv) {
  FAT16 filesystem;
  const char *image_path;
  const char *command;

  if (argc < 3) {
    printf("No input received\n");
    return 1;
  }

  image_path = argv[1];
  command = argv[2];

  fat16_open(&filesystem, image_path);
  if (filesystem.fp == NULL) {
    return 1;
  }

  if (strcmp(command, "ls") == 0) {
    DirEntry *root_entries;
    size_t root_entry_count;

    root_entries = read_root_directory(&filesystem, &root_entry_count);
    if (root_entries == NULL) {
      fat16_close(&filesystem);
      return 1;
    }

    print_volume_info(&filesystem, image_path);
    list_root(&filesystem, root_entries, root_entry_count);

    free(root_entries);
  } else if (strcmp(command, "cat") == 0 && argc >= 4) {
    DirEntry *root_entries;
    size_t root_entry_count;
    const DirEntry *target_entry;
    const char *filename;

    filename = argv[3];
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
  } else if (strcmp(command, "write") == 0 && argc >= 6) {
    const char *filename;
    const char *date_arg;
    const char *time_arg;
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    uint8_t *data;
    uint32_t data_size;

    filename = argv[3];
    date_arg = argv[4];
    time_arg = argv[5];

    if (!parse_date_arg(date_arg, &year, &month, &day) ||
        !parse_time_arg(time_arg, &hour, &minute, &second)) {
      fat16_close(&filesystem);
      return 1;
    }

    data = read_stdin_all(&data_size);
    if (data == NULL && ferror(stdin)) {
      fat16_close(&filesystem);
      return 1;
    }

    fat16_write_file(&filesystem, filename, data, data_size, year, month, day,
                     hour, minute, second);
    free(data);
  }

  fat16_close(&filesystem);
  return 0;
}
