/*
 * dcsave.h - Dreamcast VMS file headers, and the .DCI and .VMI/.VMS formats
 *
 * DCI and VMI handling based on dci4vmi by Bucanero
 * https://github.com/bucanero/dci4vmi
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

#ifndef DCSAVE_H
#define DCSAVE_H

#include <stddef.h>
#include <inttypes.h>

#include "dcvmu.h"

#define VMS_HEADER_SIZE         0x80
#define VMS_ICON_SIZE           512     /* 32x32, 4 bits per pixel */
#define VMS_ICON_W              32
#define VMS_ICON_H              32
#define VMS_EYECATCH_W          72
#define VMS_EYECATCH_H          56

#define DCI_HEADER_SIZE         VMU_DIRENT_SIZE
#define VMI_SIZE                0x6C
#define VMI_RESOURCE_LEN        8

/* VMI file mode bits */
#define VMI_MODE_NOCOPY         0x01
#define VMI_MODE_GAME           0x02

/* The header every VMS file carries: at block 0 of a data file, and at the
 * block the directory entry names (normally 1) in a game. */
typedef struct {
	char     desc_vms[17];          /* shown on the VMU LCD */
	char     desc_dc[33];           /* shown in the Dreamcast file manager */
	char     app_id[17];            /* the application that made it */
	uint16_t icons;                 /* animation frames */
	uint16_t anim_speed;
	uint16_t eyecatch;              /* 0 none, 1 true colour, 2 256 colours, 3 16 colours */
	uint16_t crc;
	uint32_t payload;               /* bytes of data after the header */
	uint32_t header_size;           /* 0x80 + icons + eyecatch */
	const uint8_t *hdr;             /* the header, inside the file buffer */
	size_t   avail;                 /* bytes from hdr to the end of the file */
} vms_header_t;

/* Where a file's VMS header sits, in bytes */
size_t vms_header_offset(const vmu_dirent_t *ent);

/* Decode the header at `off`. Returns 0, or -1 if the file is too short or
 * the icon count is not plausible (ICONDATA_VMS, for one, has no header). */
int vms_parse(const uint8_t *data, size_t len, size_t off, vms_header_t *vms);

/* CRC-16 (CCITT, polynomial 0x1021, seed 0) over header and payload, with the
 * CRC field itself taken as zero. Returns -1 if the file is too short. */
int vms_calc_crc(const vms_header_t *vms);

/* Icon frame `frame` as 32x32 RGBA, malloc'd; NULL if there is none */
uint8_t *vms_icon_rgba(const vms_header_t *vms, int frame);

/* The eyecatch as 72x56 RGBA, malloc'd; NULL if there is none */
uint8_t *vms_eyecatch_rgba(const vms_header_t *vms);

/* ICONDATA_VMS: the file-manager icon for the whole card. `color` 0 gives
 * the monochrome LCD icon, 1 the colour one. 32x32 RGBA, malloc'd. */
uint8_t *icondata_rgba(const uint8_t *data, size_t len, int color);

/*
 * .DCI (Nexus): the 32-byte directory entry, then the file's blocks with every
 * 4-byte group reversed.
 */

/* Decode a .DCI. *data is malloc'd and already swapped back. */
int dci_decode(const uint8_t *buf, size_t len, vmu_dirent_t *ent, uint8_t **data, size_t *dlen);

/* Encode a .DCI. *out is malloc'd. */
int dci_encode(const vmu_dirent_t *ent, const uint8_t *data, size_t dlen, uint8_t **out, size_t *olen);

/*
 * .VMI/.VMS (Dreamcast web browser downloads): a 108-byte .VMI describing the
 * file, and the file itself as a separate .VMS named after the resource.
 */

typedef struct {
	char     description[33];
	char     copyright[33];
	char     resource[VMI_RESOURCE_LEN + 1];    /* the .VMS base name */
	uint32_t filesize;                          /* bytes */
	int      checksum_ok;
} vmi_info_t;

/* Decode a .VMI. `ent` gets the name, type, copy flag and timestamp. */
int vmi_decode(const uint8_t *buf, size_t len, vmu_dirent_t *ent, vmi_info_t *info);

/* Build a .VMI into `out` (VMI_SIZE bytes) */
void vmi_encode(const vmu_dirent_t *ent, const char *resource, const char *description,
		const char *copyright, uint32_t filesize, uint8_t *out);

#endif
