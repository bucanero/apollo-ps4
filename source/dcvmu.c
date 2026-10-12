/*
 * dcvmu.c - Dreamcast VMU filesystem
 *
 * Based on FUSE-VMU by Ross Meikleham (MIT License)
 * https://github.com/RossMeikleham/FUSE-VMU
 *
 * FUSE-VMU keeps a decoded copy of the directory and serialises it back when
 * the card is saved. This works on the image in place instead: every change
 * goes straight into the root block, FAT and directory blocks, so the buffer
 * is always a valid card and there is no second copy to fall out of step.
 *
 * Layout of a standard card (all multi-byte values little endian):
 *
 *   blocks   0-199   user data; data files are allocated from 199 downward,
 *                    a game (VMU minigame) sits contiguously from block 0
 *   blocks 200-240   unused by the Dreamcast BIOS
 *   blocks 241-253   directory, 13 blocks of 16 entries, read from 253 down
 *   block      254   FAT, one 16-bit cell per block
 *   block      255   root block
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

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "dcvmu.h"
#include "util.h"

/* Root block field offsets */
#define ROOT_MAGIC      0x00
#define ROOT_CUSTOM     0x10
#define ROOT_COLOR      0x11
#define ROOT_TIME       0x30
#define ROOT_FAT_LOC    0x46
#define ROOT_FAT_SIZE   0x48
#define ROOT_DIR_LOC    0x4A
#define ROOT_DIR_SIZE   0x4C
#define ROOT_ICON       0x4E
#define ROOT_USER_BLKS  0x50

/* Directory entry field offsets */
#define DIR_TYPE        0x00
#define DIR_COPY        0x01
#define DIR_FIRST       0x02
#define DIR_NAME        0x04
#define DIR_TIME        0x10
#define DIR_SIZE        0x18
#define DIR_HDROFF      0x1A

/* What the BIOS writes on a standard card */
#define STD_FAT_LOC     254
#define STD_DIR_LOC     253
#define STD_DIR_SIZE    13
#define STD_USER_BLKS   200

uint8_t vmu_bcd_to_dec(uint8_t bcd)
{
	return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

uint8_t vmu_dec_to_bcd(unsigned dec)
{
	return (((dec / 10) % 10) << 4) | (dec % 10);
}

void vmu_swap32(uint8_t *buf, size_t len)
{
	uint8_t t;

	for (size_t i = 0; i + 4 <= len; i += 4) {
		t = buf[i];     buf[i] = buf[i + 3];     buf[i + 3] = t;
		t = buf[i + 1]; buf[i + 1] = buf[i + 2]; buf[i + 2] = t;
	}
}

void vmu_timestamp_now(vmu_timestamp_t *ts)
{
	time_t now = time(NULL);
	struct tm *tm = localtime(&now);

	ts->cent  = vmu_dec_to_bcd((tm->tm_year + 1900) / 100);
	ts->year  = vmu_dec_to_bcd(tm->tm_year % 100);
	ts->month = vmu_dec_to_bcd(tm->tm_mon + 1);
	ts->day   = vmu_dec_to_bcd(tm->tm_mday);
	ts->hour  = vmu_dec_to_bcd(tm->tm_hour);
	ts->min   = vmu_dec_to_bcd(tm->tm_min);
	ts->sec   = vmu_dec_to_bcd(tm->tm_sec);
	/* tm_wday counts from Sunday, the VMU from Monday. (FUSE-VMU stored
	 * tm_wday as it was, a day late.) */
	ts->dow   = vmu_dec_to_bcd((tm->tm_wday + 6) % 7);
}

static uint8_t *root_ptr(const vmu_card_t *card)
{
	return (uint8_t *)card->img + VMU_ROOT_BLOCK * VMU_BLOCK_SIZE;
}

static void read_root(vmu_card_t *card)
{
	const uint8_t *r = root_ptr(card);
	vmu_root_t *root = &card->root;
	int i;

	root->formatted = 1;
	for (i = 0; i < 16; i++)
		if (r[ROOT_MAGIC + i] != 0x55)
			root->formatted = 0;

	root->custom_color = r[ROOT_CUSTOM];
	memcpy(root->color, r + ROOT_COLOR, 4);
	memcpy(&root->timestamp, r + ROOT_TIME, sizeof(vmu_timestamp_t));
	root->fat_loc     = read_le_uint16(r + ROOT_FAT_LOC);
	root->fat_size    = read_le_uint16(r + ROOT_FAT_SIZE);
	root->dir_loc     = read_le_uint16(r + ROOT_DIR_LOC);
	root->dir_size    = read_le_uint16(r + ROOT_DIR_SIZE);
	root->icon_shape  = read_le_uint16(r + ROOT_ICON);
	root->user_blocks = read_le_uint16(r + ROOT_USER_BLKS);
}

/*
 * Every block number below comes from the card, so anything that would index
 * outside the image is rejected here, once, rather than trusted everywhere.
 */
static int root_is_sane(const vmu_root_t *root)
{
	if (!root->formatted)
		return 0;
	if (root->fat_loc >= VMU_TOTAL_BLOCKS || root->fat_size < 1)
		return 0;
	if (root->dir_loc >= VMU_TOTAL_BLOCKS || root->dir_size < 1 ||
	    root->dir_size > root->dir_loc + 1)
		return 0;
	if (root->user_blocks < 1 || root->user_blocks > VMU_TOTAL_BLOCKS)
		return 0;
	return 1;
}

int vmu_load(vmu_card_t *card, const uint8_t *buf, size_t len)
{
	if (len != VMU_SIZE)
		return VMU_ERR_SIZE;

	memcpy(card->img, buf, VMU_SIZE);
	read_root(card);

	return root_is_sane(&card->root) ? VMU_OK : VMU_ERR_FORMAT;
}

static uint16_t fat_get(const vmu_card_t *card, int blk)
{
	return read_le_uint16(card->img + card->root.fat_loc * VMU_BLOCK_SIZE + blk * 2);
}

static void fat_set(vmu_card_t *card, int blk, uint16_t val)
{
	append_le_uint16(card->img + card->root.fat_loc * VMU_BLOCK_SIZE + blk * 2, val);
}

void vmu_format(vmu_card_t *card)
{
	uint8_t *r = root_ptr(card);
	vmu_timestamp_t now;
	int i;

	memset(card->img, 0, VMU_SIZE);

	memset(r + ROOT_MAGIC, 0x55, 16);
	vmu_timestamp_now(&now);
	memcpy(r + ROOT_TIME, &now, sizeof(now));
	append_le_uint16(r + ROOT_FAT_LOC, STD_FAT_LOC);
	append_le_uint16(r + ROOT_FAT_SIZE, 1);
	append_le_uint16(r + ROOT_DIR_LOC, STD_DIR_LOC);
	append_le_uint16(r + ROOT_DIR_SIZE, STD_DIR_SIZE);
	append_le_uint16(r + ROOT_ICON, 0);
	append_le_uint16(r + ROOT_USER_BLKS, STD_USER_BLKS);
	/* Undocumented, but present on every BIOS-formatted card */
	append_le_uint16(r + 0x52, 0x1F);
	append_le_uint16(r + 0x56, 0x80);

	read_root(card);

	for (i = 0; i < VMU_TOTAL_BLOCKS; i++)
		fat_set(card, i, VMU_FAT_FREE);

	/* The system blocks are allocated to themselves: the directory is a
	 * chain from 253 down to 241, the FAT and root are one block each. */
	for (i = STD_DIR_LOC; i > STD_DIR_LOC - STD_DIR_SIZE + 1; i--)
		fat_set(card, i, i - 1);
	fat_set(card, STD_DIR_LOC - STD_DIR_SIZE + 1, VMU_FAT_EOF);
	fat_set(card, STD_FAT_LOC, VMU_FAT_EOF);
	fat_set(card, VMU_ROOT_BLOCK, VMU_FAT_EOF);
}

int vmu_dir_count(const vmu_card_t *card)
{
	return card->root.dir_size * VMU_DIRENTS_PER_BLOCK;
}

/* The directory runs downward from dir_loc: slot 0 is the first entry of
 * block 253, slot 16 the first of block 252, and so on. */
static uint8_t *dirent_ptr(const vmu_card_t *card, int idx)
{
	int blk = card->root.dir_loc - idx / VMU_DIRENTS_PER_BLOCK;

	return (uint8_t *)card->img + blk * VMU_BLOCK_SIZE +
		(idx % VMU_DIRENTS_PER_BLOCK) * VMU_DIRENT_SIZE;
}

/* Copy a 12-byte name out, dropping the NUL or space padding */
static void name_from_card(char *dst, const uint8_t *src)
{
	int n;

	memcpy(dst, src, VMU_NAME_LEN);
	dst[VMU_NAME_LEN] = 0;

	for (n = VMU_NAME_LEN; n > 0 && (dst[n - 1] == ' ' || dst[n - 1] == 0); n--)
		dst[n - 1] = 0;
}

void vmu_name_get(vmu_dirent_t *ent, const uint8_t *src)
{
	memcpy(ent->rawname, src, VMU_NAME_LEN);
	name_from_card(ent->filename, src);
}

void vmu_name_put(uint8_t *dst, const vmu_dirent_t *ent)
{
	char stored[VMU_NAME_LEN + 1];

	name_from_card(stored, ent->rawname);
	if (stored[0] && strcmp(stored, ent->filename) == 0) {
		memcpy(dst, ent->rawname, VMU_NAME_LEN);
		return;
	}
	/* NUL padded, as KallistiOS writes it */
	memset(dst, 0, VMU_NAME_LEN);
	memcpy(dst, ent->filename, strnlen(ent->filename, VMU_NAME_LEN));
}

int vmu_dir_get(const vmu_card_t *card, int idx, vmu_dirent_t *ent)
{
	const uint8_t *d;

	if (idx < 0 || idx >= vmu_dir_count(card))
		return VMU_ERR_ARG;

	d = dirent_ptr(card, idx);
	if (d[DIR_TYPE] != VMU_FILE_DATA && d[DIR_TYPE] != VMU_FILE_GAME)
		return 0;

	memset(ent, 0, sizeof(*ent));
	ent->filetype    = d[DIR_TYPE];
	ent->copyprotect = d[DIR_COPY];
	ent->firstblk    = read_le_uint16(d + DIR_FIRST);
	vmu_name_get(ent, d + DIR_NAME);
	memcpy(&ent->timestamp, d + DIR_TIME, sizeof(vmu_timestamp_t));
	ent->filesize    = read_le_uint16(d + DIR_SIZE);
	ent->hdroff      = read_le_uint16(d + DIR_HDROFF);

	return 1;
}

static void dir_put(vmu_card_t *card, int idx, const vmu_dirent_t *ent)
{
	uint8_t *d = dirent_ptr(card, idx);

	memset(d, 0, VMU_DIRENT_SIZE);
	d[DIR_TYPE] = ent->filetype;
	d[DIR_COPY] = ent->copyprotect;
	append_le_uint16(d + DIR_FIRST, ent->firstblk);
	vmu_name_put(d + DIR_NAME, ent);
	memcpy(d + DIR_TIME, &ent->timestamp, sizeof(vmu_timestamp_t));
	append_le_uint16(d + DIR_SIZE, ent->filesize);
	append_le_uint16(d + DIR_HDROFF, ent->hdroff);
}

static int name_is_valid(const char *name)
{
	size_t n = strlen(name);

	return n > 0 && n <= VMU_NAME_LEN;
}

int vmu_find(const vmu_card_t *card, const char *name)
{
	vmu_dirent_t ent;
	char want[VMU_NAME_LEN + 1];
	uint8_t raw[VMU_NAME_LEN] = {0};

	if (!name_is_valid(name))
		return VMU_ERR_NOENT;

	/* Normalise the request the same way names are read off the card */
	memcpy(raw, name, strlen(name));
	name_from_card(want, raw);

	for (int i = 0; i < vmu_dir_count(card); i++)
		if (vmu_dir_get(card, i, &ent) == 1 && strcmp(ent.filename, want) == 0)
			return i;

	return VMU_ERR_NOENT;
}

int vmu_free_blocks(const vmu_card_t *card)
{
	int n = 0;

	for (int i = 0; i < card->root.user_blocks; i++)
		if (fat_get(card, i) == VMU_FAT_FREE)
			n++;

	return n;
}

int vmu_file_count(const vmu_card_t *card)
{
	vmu_dirent_t ent;
	int n = 0;

	for (int i = 0; i < vmu_dir_count(card); i++)
		if (vmu_dir_get(card, i, &ent) == 1)
			n++;

	return n;
}

/*
 * Walk a file's chain, storing each block number in `blocks` (filesize
 * entries). A chain that leaves the card, ends early or revisits a block is
 * reported rather than followed: the FAT comes from the file, not from us.
 */
static int walk_chain(const vmu_card_t *card, const vmu_dirent_t *ent, uint16_t *blocks)
{
	uint8_t seen[VMU_TOTAL_BLOCKS] = {0};
	uint16_t blk = ent->firstblk;

	for (int i = 0; i < ent->filesize; i++) {
		if (blk >= VMU_TOTAL_BLOCKS || seen[blk])
			return VMU_ERR_CHAIN;

		seen[blk] = 1;
		blocks[i] = blk;
		blk = fat_get(card, blk);
	}

	return VMU_OK;
}

int vmu_read_file(const vmu_card_t *card, int idx, uint8_t **out, size_t *len)
{
	vmu_dirent_t ent;
	uint16_t blocks[VMU_TOTAL_BLOCKS];
	uint8_t *buf;
	int r;

	if ((r = vmu_dir_get(card, idx, &ent)) != 1)
		return r < 0 ? r : VMU_ERR_NOENT;

	if (ent.filesize > VMU_TOTAL_BLOCKS)
		return VMU_ERR_CHAIN;

	if ((r = walk_chain(card, &ent, blocks)) < 0)
		return r;

	buf = malloc(ent.filesize ? ent.filesize * VMU_BLOCK_SIZE : 1);
	if (!buf)
		return VMU_ERR_NOMEM;

	for (int i = 0; i < ent.filesize; i++)
		memcpy(buf + i * VMU_BLOCK_SIZE, card->img + blocks[i] * VMU_BLOCK_SIZE, VMU_BLOCK_SIZE);

	*out = buf;
	*len = (size_t)ent.filesize * VMU_BLOCK_SIZE;
	return VMU_OK;
}

int vmu_write_file(vmu_card_t *card, vmu_dirent_t *ent, const uint8_t *data, size_t len)
{
	uint16_t blocks[VMU_TOTAL_BLOCKS];
	int nblocks, slot = -1, found = 0;
	vmu_dirent_t tmp;

	if (!name_is_valid(ent->filename))
		return VMU_ERR_NAME;

	if (vmu_find(card, ent->filename) >= 0)
		return VMU_ERR_EXIST;

	nblocks = (int)((len + VMU_BLOCK_SIZE - 1) / VMU_BLOCK_SIZE);
	if (nblocks < 1 || nblocks > card->root.user_blocks)
		return VMU_ERR_NOSPC;

	for (int i = 0; i < vmu_dir_count(card) && slot < 0; i++)
		if (vmu_dir_get(card, i, &tmp) == 0)
			slot = i;

	if (slot < 0)
		return VMU_ERR_DIRFULL;

	if (ent->filetype == VMU_FILE_GAME) {
		/* The VMU executes a game in place, from block 0 up. There is
		 * room for one, and only when nothing else has taken the low
		 * blocks. */
		for (int i = 0; i < vmu_dir_count(card); i++)
			if (vmu_dir_get(card, i, &tmp) == 1 && tmp.filetype == VMU_FILE_GAME)
				return VMU_ERR_GAME;

		for (int i = 0; i < nblocks; i++) {
			if (fat_get(card, i) != VMU_FAT_FREE)
				return VMU_ERR_GAME;
			blocks[i] = i;
		}
		found = nblocks;
	} else {
		/* Data files take the highest free blocks first, as the BIOS
		 * does, which keeps block 0 clear for a game. */
		for (int i = card->root.user_blocks - 1; i >= 0 && found < nblocks; i--)
			if (fat_get(card, i) == VMU_FAT_FREE)
				blocks[found++] = i;

		if (found < nblocks)
			return VMU_ERR_NOSPC;
	}

	for (int i = 0; i < nblocks; i++) {
		uint8_t *dst = card->img + blocks[i] * VMU_BLOCK_SIZE;
		size_t off = (size_t)i * VMU_BLOCK_SIZE;
		size_t n = len - off < VMU_BLOCK_SIZE ? len - off : VMU_BLOCK_SIZE;

		memset(dst, 0, VMU_BLOCK_SIZE);
		memcpy(dst, data + off, n);
		fat_set(card, blocks[i], i + 1 < nblocks ? blocks[i + 1] : VMU_FAT_EOF);
	}

	ent->firstblk = blocks[0];
	ent->filesize = nblocks;
	dir_put(card, slot, ent);

	return VMU_OK;
}

/*
 * Mark the blocks a file's chain reaches, stopping where the chain breaks:
 * at EOF, a block off the card, or one already visited.
 */
static void mark_chain(const vmu_card_t *card, const vmu_dirent_t *ent, uint8_t *used)
{
	uint8_t seen[VMU_TOTAL_BLOCKS] = {0};
	uint16_t blk = ent->firstblk;

	for (int i = 0; i < ent->filesize && blk < VMU_TOTAL_BLOCKS && !seen[blk]; i++) {
		seen[blk] = used[blk] = 1;
		blk = fat_get(card, blk);
	}
}

int vmu_delete_file(vmu_card_t *card, int idx)
{
	vmu_dirent_t ent, other;
	uint8_t mine[VMU_TOTAL_BLOCKS] = {0}, theirs[VMU_TOTAL_BLOCKS] = {0};
	int r;

	if ((r = vmu_dir_get(card, idx, &ent)) != 1)
		return r < 0 ? r : VMU_ERR_NOENT;

	/* A damaged file has to stay removable, or it could never be got rid
	 * of. So free what its chain reaches - but a broken chain may run into
	 * another file's blocks, and those are left alone. */
	mark_chain(card, &ent, mine);

	for (int i = 0; i < vmu_dir_count(card); i++)
		if (i != idx && vmu_dir_get(card, i, &other) == 1)
			mark_chain(card, &other, theirs);

	for (int i = 0; i < card->root.user_blocks; i++)
		if (mine[i] && !theirs[i])
			fat_set(card, i, VMU_FAT_FREE);

	memset(dirent_ptr(card, idx), 0, VMU_DIRENT_SIZE);
	return VMU_OK;
}

int vmu_rename_file(vmu_card_t *card, int idx, const char *name)
{
	vmu_dirent_t ent;
	int r, other;

	if (!name_is_valid(name))
		return VMU_ERR_NAME;

	if ((r = vmu_dir_get(card, idx, &ent)) != 1)
		return r < 0 ? r : VMU_ERR_NOENT;

	other = vmu_find(card, name);
	if (other >= 0 && other != idx)
		return VMU_ERR_EXIST;

	/* a new name is written NUL padded, a same one keeps its bytes */
	memset(ent.filename, 0, sizeof(ent.filename));
	strncpy(ent.filename, name, VMU_NAME_LEN);
	dir_put(card, idx, &ent);

	return VMU_OK;
}

const char *vmu_strerror(int err)
{
	switch (err) {
	case VMU_OK:            return "success";
	case VMU_ERR_SIZE:      return "not a 128 KB VMU image";
	case VMU_ERR_FORMAT:    return "memory card is not formatted";
	case VMU_ERR_NOENT:     return "file not found";
	case VMU_ERR_EXIST:     return "a file with that name already exists";
	case VMU_ERR_NOSPC:     return "not enough free blocks";
	case VMU_ERR_DIRFULL:   return "the directory is full";
	case VMU_ERR_NAME:      return "file names are 1 to 12 characters";
	case VMU_ERR_CHAIN:     return "the file's FAT chain is broken";
	case VMU_ERR_GAME:      return "a game file needs the first blocks of the card, which are in use";
	case VMU_ERR_NOMEM:     return "out of memory";
	case VMU_ERR_ARG:       return "invalid argument";
	default:                return "unknown error";
	}
}
