/*
 * dccard.c - Dreamcast VMU virtual memory card management
 *
 * Card handling based on dcvmu-tool by Bucanero
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "dccard.h"
#include "dcsave.h"
#include "util.h"
#include "common.h"
#include "saves.h"

// The open card: 128 KB, kept out of the thread stack
static vmu_card_t card;
static int card_dcm = 0;
static int card_ok = 0;
static int card_fmt = 0;

static int has_ext(const char* path, const char* ext)
{
	size_t pl = strlen(path), el = strlen(ext);

	return (pl >= el && strcasecmp(path + pl - el, ext) == 0);
}

/*
 * A .DCM holds the card with every 4-byte group reversed. The 0x55 signature
 * reads the same either way, so the byte order is settled by which one gives
 * a usable root block, trying the one the file name suggests first.
 */
int dccard_open(const char* path)
{
	uint8_t *buf;
	size_t len;
	int r;

	card_ok = card_fmt = 0;
	if (read_buffer(path, &buf, &len) < 0)
		return 0;

	if (len != VMU_SIZE)
	{
		LOG("Error: '%s' is not a VMU image (%d bytes)", path, (int) len);
		free(buf);
		return 0;
	}

	card_dcm = has_ext(path, ".DCM");
	if (card_dcm)
		vmu_swap32(buf, len);

	r = vmu_load(&card, buf, len);
	if (r == VMU_ERR_FORMAT)
	{
		vmu_swap32(buf, len);
		if (vmu_load(&card, buf, len) == VMU_OK)
		{
			card_dcm = !card_dcm;
			r = VMU_OK;
		}
		else
		{
			// neither works: keep the order the name suggested
			vmu_swap32(buf, len);
			vmu_load(&card, buf, len);
		}
	}
	free(buf);

	card_ok = (r == VMU_OK || r == VMU_ERR_FORMAT);
	card_fmt = (r == VMU_OK);
	LOG("VMU '%s' loaded (%s, %s)", path, card_dcm ? "DCM" : "raw", vmu_strerror(r));

	return card_ok;
}

int dccard_save_as(const char* path, int dcm)
{
	uint8_t *buf;
	int r;

	if (!card_ok || (buf = malloc(VMU_SIZE)) == NULL)
		return 0;

	memcpy(buf, card.img, VMU_SIZE);
	if (dcm)
		vmu_swap32(buf, VMU_SIZE);

	r = write_buffer(path, buf, VMU_SIZE);
	free(buf);

	return (r == 0);
}

int dccard_save(const char* path)
{
	return dccard_save_as(path, card_dcm);
}

int dccard_is_dcm(void)
{
	return card_dcm;
}

int dccard_is_formatted(void)
{
	return (card_ok && card_fmt);
}

int dccard_free_blocks(void)
{
	return vmu_free_blocks(&card);
}

int dccard_dir_count(void)
{
	return card_fmt ? vmu_dir_count(&card) : 0;
}

int dccard_file_info(int idx, dccard_file_t* info)
{
	vms_header_t vms;
	uint8_t *data;
	size_t len;

	memset(info, 0, sizeof(dccard_file_t));
	if (vmu_dir_get(&card, idx, &info->ent) != 1)
		return 0;

	if (vmu_read_file(&card, idx, &data, &len) < 0)
		return 1;

	if (vms_parse(data, len, vms_header_offset(&info->ent), &vms) == 0)
	{
		info->has_header = 1;
		strncpy(info->desc_vms, vms.desc_vms, sizeof(info->desc_vms) - 1);
		strncpy(info->desc_dc, vms.desc_dc, sizeof(info->desc_dc) - 1);
	}
	free(data);

	return 1;
}

/* R,G,B,A bytes as the native 0xRRGGBBAA words the textures and svpng take */
static uint8_t* rgbaToNative(uint8_t *img, int pixels)
{
	uint32_t *px = (uint32_t*) img;

	for (int i = 0; px && i < pixels; i++)
		px[i] = __builtin_bswap32(px[i]);

	return img;
}

uint8_t* dccard_get_icon(int idx)
{
	vmu_dirent_t ent;
	vms_header_t vms;
	uint8_t *data, *icon = NULL;
	size_t len;

	if (vmu_dir_get(&card, idx, &ent) == 1 && vmu_read_file(&card, idx, &data, &len) == VMU_OK)
	{
		if (vms_parse(data, len, vms_header_offset(&ent), &vms) == 0)
			icon = vms_icon_rgba(&vms, 0);

		// ICONDATA_VMS has no VMS header, just the card's own icons
		if (!icon && strcmp(ent.filename, "ICONDATA_VMS") == 0)
			if ((icon = icondata_rgba(data, len, 1)) == NULL)
				icon = icondata_rgba(data, len, 0);

		free(data);
	}

	if (!icon)
		return calloc(VMS_ICON_W * VMS_ICON_H, 4);

	return rgbaToNative(icon, VMS_ICON_W * VMS_ICON_H);
}

int dccard_delete(int idx)
{
	return (vmu_delete_file(&card, idx) == VMU_OK);
}

/* Write a file to the card, replacing one with the same name. On failure the
 * card is left as it was. */
static int add_file(vmu_dirent_t *ent, const uint8_t *data, size_t len)
{
	uint8_t *backup;
	int idx, r;

	if ((backup = malloc(VMU_SIZE)) == NULL)
		return 0;

	memcpy(backup, card.img, VMU_SIZE);
	if ((idx = vmu_find(&card, ent->filename)) >= 0)
		vmu_delete_file(&card, idx);

	r = vmu_write_file(&card, ent, data, len);
	if (r != VMU_OK)
	{
		LOG("Error: can't add '%s' (%s)", ent->filename, vmu_strerror(r));
		memcpy(card.img, backup, VMU_SIZE);
	}
	else
		LOG("Added '%s' (%s, %d blocks)", ent->filename, ent->filetype == VMU_FILE_GAME ? "game" : "data", ent->filesize);

	free(backup);
	return (r == VMU_OK);
}

static int import_dci(const char *input)
{
	vmu_dirent_t ent;
	uint8_t *buf, *data;
	size_t len, dlen;
	int r;

	if (read_buffer(input, &buf, &len) < 0)
		return 0;

	r = dci_decode(buf, len, &ent, &data, &dlen);
	free(buf);

	if (r < 0)
	{
		LOG("Error: '%s' is not a valid DCI file", input);
		return 0;
	}

	// Keep the header offset only where it points inside the file
	ent.hdroff = vms_header_offset(&ent) / VMU_BLOCK_SIZE;

	r = add_file(&ent, data, dlen);
	free(data);
	return r;
}

/*
 * The .VMS a .VMI describes is named after its resource field, and lives next
 * to the .VMI. Browsers and file systems do not agree on the case of either,
 * so try the resource name both ways, then the .VMI's own base name.
 */
static int read_vms(const char *vmi_path, const char *resource, uint8_t **buf, size_t *len)
{
	char path[256];
	const char *base = strrchr(vmi_path, '/') ? strrchr(vmi_path, '/') + 1 : vmi_path;
	int dirlen = (int)(base - vmi_path);
	int stemlen = (int)strlen(base) - 4;

	const char *tries[][2] = {
		{ resource, ".VMS" }, { resource, ".vms" },
		{ NULL, ".VMS" }, { NULL, ".vms" },
	};

	for (int i = 0; i < 4; i++)
	{
		if (tries[i][0])
			snprintf(path, sizeof(path), "%.*s%s%s", dirlen, vmi_path, tries[i][0], tries[i][1]);
		else
			snprintf(path, sizeof(path), "%.*s%s", dirlen + stemlen, vmi_path, tries[i][1]);

		if (read_buffer(path, buf, len) == 0)
			return 1;
	}

	LOG("Error: can't find '%s.VMS' next to '%s'", resource, vmi_path);
	return 0;
}

static int import_vmi(const char *input)
{
	vmu_dirent_t ent;
	vmi_info_t info;
	uint8_t *buf;
	size_t len;
	int r;

	if (read_buffer(input, &buf, &len) < 0)
		return 0;

	r = vmi_decode(buf, len, &ent, &info);
	free(buf);

	if (r < 0)
	{
		LOG("Error: '%s' is not a valid VMI file", input);
		return 0;
	}

	if (!info.checksum_ok)
		LOG("Warning: the VMI checksum does not match its resource name");

	if (!read_vms(input, info.resource, &buf, &len))
		return 0;

	if (len < info.filesize)
	{
		LOG("Error: the VMS holds %d bytes, the VMI expects %d", (int) len, (int) info.filesize);
		free(buf);
		return 0;
	}

	r = add_file(&ent, buf, info.filesize);
	free(buf);
	return r;
}

int dccard_import_save(const char* path)
{
	if (!card_fmt)
		return 0;

	if (has_ext(path, ".DCI"))
		return import_dci(path);

	if (has_ext(path, ".VMI"))
		return import_vmi(path);

	return 0;
}

int dccard_export_dci(int idx, const char* dci_path)
{
	vmu_dirent_t ent;
	uint8_t *data, *out;
	size_t len, olen;
	int r;

	if (vmu_dir_get(&card, idx, &ent) != 1 || vmu_read_file(&card, idx, &data, &len) < 0)
		return 0;

	r = dci_encode(&ent, data, len, &out, &olen);
	free(data);
	if (r < 0)
		return 0;

	r = write_buffer(dci_path, out, olen);
	free(out);

	return (r == 0);
}

/* An 8-character resource name, from the VMU file name, that no .VMS or .VMI
 * in `out_dir` uses yet */
static void get_resource_name(const char* out_dir, const char* filename, char* resource)
{
	char path[256];
	int n = 0;

	for (n = 0; filename[n] && n < VMI_RESOURCE_LEN; n++)
		resource[n] = (isalnum((uint8_t)filename[n]) || filename[n] == '_' || filename[n] == '-') ? filename[n] : '_';
	resource[n] = 0;

	if (!n)
		strcpy(resource, "VMUSAVE");

	for (int i = 1; i < 100; i++)
	{
		snprintf(path, sizeof(path), "%s%s.VMS", out_dir, resource);
		if (file_exists(path) != SUCCESS)
			return;

		snprintf(resource + (n > 6 ? 6 : n), 3, "%02d", i);
	}
}

int dccard_export_vmi(int idx, const char* out_dir)
{
	vmu_dirent_t ent;
	vms_header_t vms;
	uint8_t *data, vmi[VMI_SIZE];
	char resource[VMI_RESOURCE_LEN + 1], path[256];
	const char *desc = "";
	size_t len;
	int r;

	if (vmu_dir_get(&card, idx, &ent) != 1 || vmu_read_file(&card, idx, &data, &len) < 0)
		return 0;

	if (vms_parse(data, len, vms_header_offset(&ent), &vms) == 0)
		desc = vms.desc_dc;

	get_resource_name(out_dir, ent.filename, resource);
	vmi_encode(&ent, resource, desc, "Apollo Save Tool (PS4)", (uint32_t)len, vmi);

	snprintf(path, sizeof(path), "%s%s.VMS", out_dir, resource);
	r = write_buffer(path, data, len);
	free(data);
	if (r < 0)
		return 0;

	snprintf(path, sizeof(path), "%s%s.VMI", out_dir, resource);
	return (write_buffer(path, vmi, VMI_SIZE) == 0);
}
