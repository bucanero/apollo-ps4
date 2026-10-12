/*
 * dccard.h - Dreamcast VMU virtual memory card management
 *
 * A single card is kept open at a time, as ps1card does. Raw (.BIN/.VMU) and
 * Nexus (.DCM) images are both handled; the card is written back in the byte
 * order it was read in.
 */

#ifndef DCCARD_H
#define DCCARD_H

#include <inttypes.h>

#include "dcvmu.h"

typedef struct {
	vmu_dirent_t ent;
	char desc_vms[17];          /* VMU LCD description, Shift-JIS */
	char desc_dc[33];           /* Dreamcast file manager description, Shift-JIS */
	int  has_header;            /* a VMS header was found */
} dccard_file_t;

/* Load a card image. Returns 1 on success; an unformatted card also loads. */
int dccard_open(const char* path);

/* Write the card back to `path`, in the byte order it was read in */
int dccard_save(const char* path);

/* Write the card to `path`, as a Nexus .DCM (dcm = 1) or a raw image */
int dccard_save_as(const char* path, int dcm);

/* 1 if the open card was read as a Nexus .DCM */
int dccard_is_dcm(void);

/* 1 if the open card has a valid root block */
int dccard_is_formatted(void);

int dccard_free_blocks(void);

/* Number of directory slots, used or not */
int dccard_dir_count(void);

/* Details of directory slot `idx`. Returns 1 if it holds a file, 0 otherwise. */
int dccard_file_info(int idx, dccard_file_t* info);

/* The file's first icon frame as 32x32 pixels in the texture format (A,R,G,B
 * bytes), malloc'd. A file without an icon gives a blank image. */
uint8_t* dccard_get_icon(int idx);

int dccard_delete(int idx);

/* Import a .DCI, or a .VMI and the .VMS it names. A file with the same name
 * is replaced. Returns 1 on success. */
int dccard_import_save(const char* path);

/* Export to a .DCI file. Returns 1 on success. */
int dccard_export_dci(int idx, const char* dci_path);

/* Export as .VMI + .VMS into `out_dir`, under a unique 8-character resource
 * name. Returns 1 on success. */
int dccard_export_vmi(int idx, const char* out_dir);

#endif
