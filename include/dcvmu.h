/*
 * dcvmu.h - Dreamcast VMU filesystem
 *
 * Based on FUSE-VMU by Ross Meikleham (MIT License)
 * https://github.com/RossMeikleham/FUSE-VMU
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef DCVMU_H
#define DCVMU_H

#include <stddef.h>
#include <inttypes.h>

#define VMU_BLOCK_SIZE          512
#define VMU_TOTAL_BLOCKS        256
#define VMU_SIZE                (VMU_BLOCK_SIZE * VMU_TOTAL_BLOCKS)
#define VMU_ROOT_BLOCK          255
#define VMU_NAME_LEN            12
#define VMU_DIRENT_SIZE         32
#define VMU_DIRENTS_PER_BLOCK   (VMU_BLOCK_SIZE / VMU_DIRENT_SIZE)

/* FAT cell values */
#define VMU_FAT_FREE            0xFFFC
#define VMU_FAT_EOF             0xFFFA

/* Directory entry file types */
#define VMU_FILE_NONE           0x00
#define VMU_FILE_DATA           0x33
#define VMU_FILE_GAME           0xCC

/* Copy protection byte */
#define VMU_COPY_OK             0x00
#define VMU_COPY_PROTECTED      0xFF

/* Error codes, all negative */
enum vmu_error {
	VMU_OK          = 0,
	VMU_ERR_SIZE    = -1,   /* image is not 128 KB */
	VMU_ERR_FORMAT  = -2,   /* root block does not describe a usable card */
	VMU_ERR_NOENT   = -3,   /* no such file */
	VMU_ERR_EXIST   = -4,   /* a file with that name already exists */
	VMU_ERR_NOSPC   = -5,   /* not enough free blocks */
	VMU_ERR_DIRFULL = -6,   /* no free directory entry */
	VMU_ERR_NAME    = -7,   /* empty or over-long file name */
	VMU_ERR_CHAIN   = -8,   /* FAT chain is broken, loops or runs short */
	VMU_ERR_GAME    = -9,   /* a game file needs blocks 0..n-1, and they are taken */
	VMU_ERR_NOMEM   = -10,
	VMU_ERR_ARG     = -11,
};

/* BCD timestamp, exactly as stored. Day of week: 0 = Monday. */
typedef struct {
	uint8_t cent;
	uint8_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t dow;
} vmu_timestamp_t;

/* A directory entry, decoded */
typedef struct {
	uint8_t  filetype;              /* VMU_FILE_DATA / VMU_FILE_GAME */
	uint8_t  copyprotect;           /* VMU_COPY_OK / VMU_COPY_PROTECTED */
	uint16_t firstblk;
	char     filename[VMU_NAME_LEN + 1];
	/* The name's 12 bytes as they were stored, padding and all: game files
	 * are padded with spaces, data files with NULs. Written back as long as
	 * they still spell filename; all zero for an entry made from a name. */
	uint8_t  rawname[VMU_NAME_LEN];
	vmu_timestamp_t timestamp;
	uint16_t filesize;              /* in blocks */
	uint16_t hdroff;                /* VMS header offset, in blocks */
} vmu_dirent_t;

/* The root block, decoded */
typedef struct {
	int      formatted;             /* the 0x55 signature is present */
	uint8_t  custom_color;
	uint8_t  color[4];              /* blue, green, red, alpha */
	vmu_timestamp_t timestamp;
	uint16_t fat_loc;
	uint16_t fat_size;
	uint16_t dir_loc;
	uint16_t dir_size;
	uint16_t icon_shape;
	uint16_t user_blocks;
} vmu_root_t;

typedef struct {
	uint8_t    img[VMU_SIZE];
	vmu_root_t root;
} vmu_card_t;

/* Load a raw 128 KB image. Checks the root block: a card that fails is still
 * loaded, so it can be formatted, but VMU_ERR_FORMAT is returned. */
int vmu_load(vmu_card_t *card, const uint8_t *buf, size_t len);

/* Wipe the card and write a fresh root block, FAT and directory */
void vmu_format(vmu_card_t *card);

/* Number of directory slots, used or not */
int vmu_dir_count(const vmu_card_t *card);

/* Decode directory slot `idx`. Returns 1 if it holds a file, 0 if free,
 * or a negative error. */
int vmu_dir_get(const vmu_card_t *card, int idx, vmu_dirent_t *ent);

/* Directory slot holding `name`, or VMU_ERR_NOENT */
int vmu_find(const vmu_card_t *card, const char *name);

/* Write ent's name into a 12-byte field: its stored bytes when it has them,
 * else the name NUL padded */
void vmu_name_put(uint8_t *dst, const vmu_dirent_t *ent);
/* Keep a name's stored bytes, and the name read from them */
void vmu_name_get(vmu_dirent_t *ent, const uint8_t *src);

/* Blocks available for new data files */
int vmu_free_blocks(const vmu_card_t *card);

/* Files on the card */
int vmu_file_count(const vmu_card_t *card);

/* Copy a file's blocks out. *out is malloc'd, *len = filesize * 512. */
int vmu_read_file(const vmu_card_t *card, int idx, uint8_t **out, size_t *len);

/* Write a new file. `ent` supplies the name, type, copy flag, timestamp and
 * header offset; firstblk and filesize are filled in. `len` is rounded up to
 * whole blocks, the tail padded with zeros. Game files go at block 0 and must
 * be contiguous, as the VMU runs them in place. */
int vmu_write_file(vmu_card_t *card, vmu_dirent_t *ent, const uint8_t *data, size_t len);

/* Free a file's blocks and clear its directory slot */
int vmu_delete_file(vmu_card_t *card, int idx);

/* Rename a file */
int vmu_rename_file(vmu_card_t *card, int idx, const char *name);

/* Current local time as a VMU timestamp */
void vmu_timestamp_now(vmu_timestamp_t *ts);

/* 4-byte groups reversed in place, as .DCI and .DCM store data. `len` must be
 * a multiple of 4. */
void vmu_swap32(uint8_t *buf, size_t len);

uint8_t vmu_bcd_to_dec(uint8_t bcd);
uint8_t vmu_dec_to_bcd(unsigned dec);     /* the last two decimal digits */

const char *vmu_strerror(int err);

#endif
