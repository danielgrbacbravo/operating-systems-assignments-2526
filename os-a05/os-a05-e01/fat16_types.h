#ifndef FAT16_TYPES_H
#define FAT16_TYPES_H

#include <stdint.h>
#include <stdio.h>

#pragma pack(push, 1)

/**
 * BPB – BIOS Parameter Block.
 *
 * This is the first 62 bytes of the boot sector (byte offset 0 in the
 * image). It describes the entire geometry of the filesystem: sector
 * sizes, cluster sizes, FAT location, root directory size, etc.
 *
 * All multi-byte fields are little-endian.
 */
typedef struct {
  uint8_t jump[3];            /* 0x00  boot jump instruction           */
  char oem_name[8];           /* 0x03  OEM name, e.g. "MSDOS5.0"      */
  uint16_t bytes_per_sector;  /* 0x0B  almost always 512               */
  uint8_t sectors_per_clus;   /* 0x0D  cluster size in sectors         */
  uint16_t reserved_sectors;  /* 0x0E  sectors before the first FAT    */
  uint8_t num_fats;           /* 0x10  number of FAT copies (usually 2)*/
  uint16_t root_entry_count;  /* 0x11  max entries in root directory   */
  uint16_t total_sectors_16;  /* 0x13  total sectors (0 if > 65535)    */
  uint8_t media_type;         /* 0x15  media descriptor byte           */
  uint16_t fat_size_16;       /* 0x16  sectors per FAT copy            */
  uint16_t sectors_per_track; /* 0x18  CHS geometry                    */
  uint16_t num_heads;         /* 0x1A  CHS geometry                    */
  uint32_t hidden_sectors;    /* 0x1C  sectors before this volume      */
  uint32_t total_sectors_32;  /* 0x20  total sectors if > 65535        */
  /* Extended BPB (FAT16) */
  uint8_t drive_number;  /* 0x24  BIOS drive number               */
  uint8_t reserved1;     /* 0x25  reserved                        */
  uint8_t boot_sig;      /* 0x26  0x29 = extended BPB is valid    */
  uint32_t volume_id;    /* 0x27  volume serial number            */
  char volume_label[11]; /* 0x2B  volume label, space-padded      */
  char fs_type[8];       /* 0x36  e.g. "FAT16   "                 */
} BPB;

/**
 * DirEntry: 32-byte directory entry.
 *
 * Used for regular files, subdirectories, the volume label, and long
 * filename (LFN) fragments. The first byte of `name` has special meaning:
 *   0x00: end of directory (no more entries follow)
 *   0xE5: entry has been deleted
 */
typedef struct {
  char name[8];           /* 0x00  8-char filename, space-padded    */
  char ext[3];            /* 0x08  3-char extension, space-padded   */
  uint8_t attr;           /* 0x0B  attribute bitmask (see below)    */
  uint8_t nt_reserved;    /* 0x0C  reserved for Windows NT         */
  uint8_t crt_time_tenth; /* 0x0D  creation time, tenths of second */
  uint16_t crt_time;      /* 0x0E  creation time                   */
  uint16_t crt_date;      /* 0x10  creation date                   */
  uint16_t acc_date;      /* 0x12  last-access date                */
  uint16_t first_clus_hi; /* 0x14  high 16 bits of cluster (FAT32) */
  uint16_t wrt_time;      /* 0x16  last-write time                 */
  uint16_t wrt_date;      /* 0x18  last-write date                 */
  uint16_t first_clus_lo; /* 0x1A  first data cluster number       */
  uint32_t file_size;     /* 0x1C  file size in bytes              */
} DirEntry;

#pragma pack(pop)

/* directory-entry attribute flags */

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LFN 0x0F /* combination used by long-filename entries */

/* FAT cluster-chain special values */

#define FAT16_FREE 0x0000 /* cluster is unallocated                */
#define FAT16_BAD 0xFFF7  /* bad cluster marker                    */
#define FAT16_EOC 0xFFF8  /* end-of-chain (any value >= this)      */

/* runtime filesystem context */

/**
 * FAT16 – all bookkeeping state for an open FAT16 image.
 *
 * Created by your fat16_open() implementation, freed by fat16_close().
 *
 * Key relationships you will need:
 *   fat_start   = reserved_sectors × bytes_per_sector
 *   root_start  = fat_start + num_fats × fat_size_16 × bytes_per_sector
 *   data_start  = root_start + root_entry_count × sizeof(DirEntry)
 *   cluster_size = bytes_per_sector × sectors_per_clus
 *
 *   byte offset of cluster C = data_start + (C − 2) × cluster_size
 *       (clusters are numbered from 2; entries 0 and 1 are reserved)
 */
typedef struct {
  FILE *fp;      /* open file handle to the .img file      */
  BPB bpb;       /* copy of the BIOS Parameter Block       */
  uint16_t *fat; /* entire FAT table, loaded into RAM      */

  uint32_t fat_start;    /* byte offset: start of FAT region       */
  uint32_t root_start;   /* byte offset: start of root directory   */
  uint32_t data_start;   /* byte offset: start of data region      */
  uint32_t cluster_size; /* bytes per cluster                      */

  /* bookkeeping for the `ls` summary line */
  uint32_t total_files;
  uint64_t total_bytes;
} FAT16;

#endif /* FAT16_TYPES_H */
