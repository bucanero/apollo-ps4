/*
 * dcsave.c - Dreamcast VMS file headers, and the .DCI and .VMI/.VMS formats
 *
 * DCI and VMI handling based on dci4vmi by Bucanero
 * https://github.com/bucanero/dci4vmi
 *
 * VMS header layout from Marcus Comstedt's Dreamcast documentation
 * http://mc.pp.se/dc/vms/fileheader.html
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

#include "dcsave.h"
#include "util.h"

/* VMS header field offsets */
#define VMS_DESC_VMS    0x00
#define VMS_DESC_DC     0x10
#define VMS_APP_ID      0x30
#define VMS_ICONS       0x40
#define VMS_ANIM        0x42
#define VMS_EYECATCH    0x44
#define VMS_CRC         0x46
#define VMS_PAYLOAD     0x48
#define VMS_PALETTE     0x60
#define VMS_BITMAPS     0x80

/* VMI field offsets */
#define VMI_CHECKSUM    0x00
#define VMI_DESC        0x04
#define VMI_COPYRIGHT   0x24
#define VMI_TIME        0x44
#define VMI_VERSION     0x4C
#define VMI_FILENUM     0x4E
#define VMI_RESOURCE    0x50
#define VMI_FILENAME    0x58
#define VMI_MODE        0x64
#define VMI_FILESIZE    0x68

static const uint32_t eyecatch_sizes[4] = {
	0,
	VMS_EYECATCH_W * VMS_EYECATCH_H * 2,                    /* ARGB4444 */
	512 + VMS_EYECATCH_W * VMS_EYECATCH_H,                  /* 256-entry palette */
	32 + VMS_EYECATCH_W * VMS_EYECATCH_H / 2,               /* 16-entry palette */
};

/* Copy a fixed-width text field, trimming the space or NUL padding */
static void copy_text(char *dst, const uint8_t *src, size_t n)
{
	memcpy(dst, src, n);
	dst[n] = 0;

	while (n > 0 && (dst[n - 1] == ' ' || dst[n - 1] == 0))
		dst[--n] = 0;
}

/* Fill a fixed-width field: no terminator, NUL padded. The field must
 * already be zeroed. */
static void put_text(uint8_t *dst, const char *src, size_t n)
{
	memcpy(dst, src, strnlen(src, n));
}

/* ARGB4444 to RGBA8888 */
static void argb4444(uint16_t c, uint8_t *out)
{
	out[0] = ((c >> 8) & 0xF) * 0x11;
	out[1] = ((c >> 4) & 0xF) * 0x11;
	out[2] = (c & 0xF) * 0x11;
	out[3] = ((c >> 12) & 0xF) * 0x11;
}

size_t vms_header_offset(const vmu_dirent_t *ent)
{
	/* Some tools, FUSE-VMU among them, store the file size here. An offset
	 * past the end of the file cannot be right, so fall back to where the
	 * header always is in practice. */
	if (ent->hdroff < ent->filesize)
		return (size_t)ent->hdroff * VMU_BLOCK_SIZE;

	return ent->filetype == VMU_FILE_GAME ? VMU_BLOCK_SIZE : 0;
}

int vms_parse(const uint8_t *data, size_t len, size_t off, vms_header_t *vms)
{
	const uint8_t *h;

	if (off > len || len - off < VMS_HEADER_SIZE)
		return -1;

	h = data + off;
	memset(vms, 0, sizeof(*vms));
	copy_text(vms->desc_vms, h + VMS_DESC_VMS, 16);
	copy_text(vms->desc_dc, h + VMS_DESC_DC, 32);
	copy_text(vms->app_id, h + VMS_APP_ID, 16);
	vms->icons      = read_le_uint16(h + VMS_ICONS);
	vms->anim_speed = read_le_uint16(h + VMS_ANIM);
	vms->eyecatch   = read_le_uint16(h + VMS_EYECATCH);
	vms->crc        = read_le_uint16(h + VMS_CRC);
	vms->payload    = read_le_uint32(h + VMS_PAYLOAD);
	vms->hdr        = h;
	vms->avail      = len - off;

	if (vms->icons > 16 || vms->eyecatch > 3)
		return -1;

	vms->header_size = VMS_BITMAPS + vms->icons * VMS_ICON_SIZE + eyecatch_sizes[vms->eyecatch];
	if (vms->header_size > vms->avail)
		return -1;

	return 0;
}

int vms_calc_crc(const vms_header_t *vms)
{
	uint32_t n = vms->header_size + vms->payload;
	uint16_t crc = 0;

	if (vms->payload > vms->avail || n > vms->avail)
		return -1;

	for (uint32_t i = 0; i < n; i++) {
		uint8_t b = (i == VMS_CRC || i == VMS_CRC + 1) ? 0 : vms->hdr[i];

		crc ^= b << 8;
		for (int k = 0; k < 8; k++)
			crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
	}

	return crc;
}

/* 4-bit pixels, high nybble first, through a 16-entry ARGB4444 palette */
static void decode_4bpp(const uint8_t *pal, const uint8_t *bits, int npix, uint8_t *rgba)
{
	for (int i = 0; i < npix; i++) {
		int idx = (bits[i / 2] >> ((i & 1) ? 0 : 4)) & 0xF;

		argb4444(read_le_uint16(pal + idx * 2), rgba + i * 4);
	}
}

uint8_t *vms_icon_rgba(const vms_header_t *vms, int frame)
{
	uint8_t *rgba;

	if (frame < 0 || frame >= vms->icons)
		return NULL;

	rgba = malloc(VMS_ICON_W * VMS_ICON_H * 4);
	if (!rgba)
		return NULL;

	decode_4bpp(vms->hdr + VMS_PALETTE, vms->hdr + VMS_BITMAPS + frame * VMS_ICON_SIZE,
		    VMS_ICON_W * VMS_ICON_H, rgba);
	return rgba;
}

uint8_t *vms_eyecatch_rgba(const vms_header_t *vms)
{
	const int npix = VMS_EYECATCH_W * VMS_EYECATCH_H;
	const uint8_t *e;
	uint8_t *rgba;

	if (vms->eyecatch < 1 || vms->eyecatch > 3)
		return NULL;

	rgba = malloc(npix * 4);
	if (!rgba)
		return NULL;

	e = vms->hdr + VMS_BITMAPS + vms->icons * VMS_ICON_SIZE;

	switch (vms->eyecatch) {
	case 1:
		for (int i = 0; i < npix; i++)
			argb4444(read_le_uint16(e + i * 2), rgba + i * 4);
		break;
	case 2:
		for (int i = 0; i < npix; i++)
			argb4444(read_le_uint16(e + e[512 + i] * 2), rgba + i * 4);
		break;
	case 3:
		decode_4bpp(e, e + 32, npix, rgba);
		break;
	}

	return rgba;
}

uint8_t *icondata_rgba(const uint8_t *data, size_t len, int color)
{
	uint32_t off;
	uint8_t *rgba;

	if (len < 0x18)
		return NULL;

	off = read_le_uint32(data + (color ? 0x14 : 0x10));
	if (!off || off > len || len - off < (color ? 32 + VMS_ICON_SIZE : 128))
		return NULL;

	rgba = malloc(VMS_ICON_W * VMS_ICON_H * 4);
	if (!rgba)
		return NULL;

	if (color) {
		decode_4bpp(data + off, data + off + 32, VMS_ICON_W * VMS_ICON_H, rgba);
		return rgba;
	}

	/* 1 bit per pixel, most significant first; a set bit is a lit LCD dot */
	for (int i = 0; i < VMS_ICON_W * VMS_ICON_H; i++) {
		uint8_t v = (data[off + i / 8] & (0x80 >> (i % 8))) ? 0x00 : 0xFF;

		rgba[i * 4] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = v;
		rgba[i * 4 + 3] = 0xFF;
	}

	return rgba;
}

static void dirent_decode(const uint8_t *d, vmu_dirent_t *ent)
{
	memset(ent, 0, sizeof(*ent));
	ent->filetype    = d[0x00];
	ent->copyprotect = d[0x01];
	ent->firstblk    = read_le_uint16(d + 0x02);
	vmu_name_get(ent, d + 0x04);
	memcpy(&ent->timestamp, d + 0x10, sizeof(vmu_timestamp_t));
	ent->filesize    = read_le_uint16(d + 0x18);
	ent->hdroff      = read_le_uint16(d + 0x1A);
}

static void dirent_encode(const vmu_dirent_t *ent, uint8_t *d)
{
	memset(d, 0, VMU_DIRENT_SIZE);
	d[0x00] = ent->filetype;
	d[0x01] = ent->copyprotect;
	append_le_uint16(d + 0x02, ent->firstblk);
	vmu_name_put(d + 0x04, ent);
	memcpy(d + 0x10, &ent->timestamp, sizeof(vmu_timestamp_t));
	append_le_uint16(d + 0x18, ent->filesize);
	append_le_uint16(d + 0x1A, ent->hdroff);
}

int dci_decode(const uint8_t *buf, size_t len, vmu_dirent_t *ent, uint8_t **data, size_t *dlen)
{
	size_t n;
	uint8_t *out;

	if (len < DCI_HEADER_SIZE)
		return -1;

	dirent_decode(buf, ent);

	if (ent->filetype != VMU_FILE_DATA && ent->filetype != VMU_FILE_GAME)
		return -1;

	n = (size_t)ent->filesize * VMU_BLOCK_SIZE;
	if (n == 0 || n > len - DCI_HEADER_SIZE)
		return -1;

	out = malloc(n);
	if (!out)
		return -1;

	memcpy(out, buf + DCI_HEADER_SIZE, n);
	vmu_swap32(out, n);

	*data = out;
	*dlen = n;
	return 0;
}

int dci_encode(const vmu_dirent_t *ent, const uint8_t *data, size_t dlen, uint8_t **out, size_t *olen)
{
	uint8_t *buf;

	if (dlen != (size_t)ent->filesize * VMU_BLOCK_SIZE)
		return -1;

	buf = malloc(DCI_HEADER_SIZE + dlen);
	if (!buf)
		return -1;

	/* The entry goes out as the card holds it, first block and header
	 * offset included, as the Nexus software writes it. */
	dirent_encode(ent, buf);
	memcpy(buf + DCI_HEADER_SIZE, data, dlen);
	vmu_swap32(buf + DCI_HEADER_SIZE, dlen);

	*out = buf;
	*olen = DCI_HEADER_SIZE + dlen;
	return 0;
}

static const char vmi_sega[4] = { 'S', 'E', 'G', 'A' };

static int bcd_valid(uint8_t v, unsigned lo, unsigned hi)
{
	return (v >> 4) <= 9 && (v & 0xF) <= 9 && vmu_bcd_to_dec(v) >= lo && vmu_bcd_to_dec(v) <= hi;
}

static unsigned clamp(unsigned v, unsigned lo, unsigned hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

/*
 * A VMI stores the time in binary, with a 16-bit year and Sunday as day 0;
 * the card wants BCD and Monday as day 0. Not every tool followed that: some
 * wrote the time already in BCD ("20 03 06 28" for 2003-06-28), and some a
 * 0-based month. Valid BCD with a 19xx/20xx century is taken as BCD, valid
 * binary with a plausible year as binary, then any other valid BCD as BCD;
 * anything else is clamped into range, so the card never holds a month 16 or
 * an hour 0x99.
 */
static void vmi_time(const uint8_t *t, vmu_timestamp_t *ts)
{
	unsigned year = read_le_uint16(t) % 10000;
	int bcd = bcd_valid(t[0], 0, 99) && bcd_valid(t[1], 0, 99) && bcd_valid(t[2], 0, 12) &&
		  bcd_valid(t[3], 0, 31) && bcd_valid(t[4], 0, 23) && bcd_valid(t[5], 0, 59) &&
		  bcd_valid(t[6], 0, 59);

	/* BCD bytes such as 20 03 06 08 also pass as a binary date (year 800),
	 * so a binary date needs a plausible year, and BCD with a 19xx/20xx
	 * century is taken as BCD. */
	if (!(bcd && (t[0] == 0x19 || t[0] == 0x20)) && year >= 1980 && year <= 2099 &&
	    t[2] >= 1 && t[2] <= 12 && t[3] >= 1 && t[3] <= 31 &&
	    t[4] <= 23 && t[5] <= 59 && t[6] <= 59) {
		ts->cent  = vmu_dec_to_bcd(year / 100);
		ts->year  = vmu_dec_to_bcd(year % 100);
		ts->month = vmu_dec_to_bcd(t[2]);
		ts->day   = vmu_dec_to_bcd(t[3]);
		ts->hour  = vmu_dec_to_bcd(t[4]);
		ts->min   = vmu_dec_to_bcd(t[5]);
		ts->sec   = vmu_dec_to_bcd(t[6]);
	}
	else if (bcd) {
		/* a month or day of 0 means unset: make it the first */
		ts->cent  = t[0];
		ts->year  = t[1];
		ts->month = t[2] ? t[2] : 1;
		ts->day   = t[3] ? t[3] : 1;
		ts->hour  = t[4];
		ts->min   = t[5];
		ts->sec   = t[6];
	}
	else {
		ts->cent  = vmu_dec_to_bcd(year / 100);
		ts->year  = vmu_dec_to_bcd(year % 100);
		ts->month = vmu_dec_to_bcd(clamp(t[2], 1, 12));
		ts->day   = vmu_dec_to_bcd(clamp(t[3], 1, 31));
		ts->hour  = vmu_dec_to_bcd(clamp(t[4], 0, 23));
		ts->min   = vmu_dec_to_bcd(clamp(t[5], 0, 59));
		ts->sec   = vmu_dec_to_bcd(clamp(t[6], 0, 59));
	}

	ts->dow = vmu_dec_to_bcd(t[7] % 7 == 0 ? 6 : t[7] % 7 - 1);
}

int vmi_decode(const uint8_t *buf, size_t len, vmu_dirent_t *ent, vmi_info_t *info)
{
	uint16_t mode;

	if (len < VMI_SIZE)
		return -1;

	memset(info, 0, sizeof(*info));
	copy_text(info->description, buf + VMI_DESC, 32);
	copy_text(info->copyright, buf + VMI_COPYRIGHT, 32);
	copy_text(info->resource, buf + VMI_RESOURCE, VMI_RESOURCE_LEN);
	info->filesize = read_le_uint32(buf + VMI_FILESIZE);

	/* The "checksum" is the resource name ANDed with "SEGA" */
	info->checksum_ok = 1;
	for (int i = 0; i < 4; i++)
		if (buf[VMI_CHECKSUM + i] != (buf[VMI_RESOURCE + i] & vmi_sega[i]))
			info->checksum_ok = 0;

	/* The Planetweb browser, where most VMIs come from, writes this
	 * constant whatever the resource name; it is as good as a match. */
	if (memcmp(buf + VMI_CHECKSUM, "ADD@", 4) == 0)
		info->checksum_ok = 1;

	if (!info->resource[0] || info->filesize == 0)
		return -1;

	memset(ent, 0, sizeof(*ent));
	vmu_name_get(ent, buf + VMI_FILENAME);
	mode = read_le_uint16(buf + VMI_MODE);
	ent->filetype    = (mode & VMI_MODE_GAME) ? VMU_FILE_GAME : VMU_FILE_DATA;
	ent->copyprotect = (mode & VMI_MODE_NOCOPY) ? VMU_COPY_PROTECTED : VMU_COPY_OK;
	ent->hdroff      = (mode & VMI_MODE_GAME) ? 1 : 0;

	vmi_time(buf + VMI_TIME, &ent->timestamp);
	return 0;
}

void vmi_encode(const vmu_dirent_t *ent, const char *resource, const char *description,
		const char *copyright, uint32_t filesize, uint8_t *out)
{
	const vmu_timestamp_t *ts = &ent->timestamp;
	uint8_t *t = out + VMI_TIME;
	uint8_t dow = vmu_bcd_to_dec(ts->dow);
	uint16_t mode = 0;

	memset(out, 0, VMI_SIZE);
	put_text(out + VMI_DESC, description, 32);
	put_text(out + VMI_COPYRIGHT, copyright, 32);
	put_text(out + VMI_RESOURCE, resource, VMI_RESOURCE_LEN);

	for (int i = 0; i < 4; i++)
		out[VMI_CHECKSUM + i] = out[VMI_RESOURCE + i] & vmi_sega[i];

	append_le_uint16(t, vmu_bcd_to_dec(ts->cent) * 100 + vmu_bcd_to_dec(ts->year));
	t[2] = vmu_bcd_to_dec(ts->month);
	t[3] = vmu_bcd_to_dec(ts->day);
	t[4] = vmu_bcd_to_dec(ts->hour);
	t[5] = vmu_bcd_to_dec(ts->min);
	t[6] = vmu_bcd_to_dec(ts->sec);
	t[7] = dow == 6 ? 0 : dow + 1;

	append_le_uint16(out + VMI_VERSION, 0);
	append_le_uint16(out + VMI_FILENUM, 1);
	vmu_name_put(out + VMI_FILENAME, ent);

	if (ent->filetype == VMU_FILE_GAME)
		mode |= VMI_MODE_GAME;
	if (ent->copyprotect == VMU_COPY_PROTECTED)
		mode |= VMI_MODE_NOCOPY;
	append_le_uint16(out + VMI_MODE, mode);
	append_le_uint32(out + VMI_FILESIZE, filesize);
}
