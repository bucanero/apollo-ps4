/*
*  ps3-psvresigner by @dots_tb - Resigns non-console specific PS3 PSV savefiles.
*  PSV files embed PS1 and PS2 save data. This does not inject!
*  With help from the CBPS (https://discord.gg/2nDCbxJ) , especially:
*   @AnalogMan151, @teakhanirons, Silica, @notzecoxao, @nyaaasen
*/

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <polarssl/aes.h>
#include <polarssl/sha1.h>

#include "types.h"
#include "util.h"
#include "ps2mc.h"
#include "mcio.h"
#include "shiftjis.h"

#define PSV_TYPE_PS1    0x01
#define PSV_TYPE_PS2    0x02
#define PSV_SEED_OFFSET 0x08
#define PSV_HASH_OFFSET 0x1C
#define PSV_TYPE_OFFSET 0x3C
#define VMP_SEED_OFFSET 0x0C
#define VMP_HASH_OFFSET 0x20
#define VMP_MAGIC       0x564D5000
#define VMP_SIZE        0x20080

static const char SJIS_REPLACEMENT_TABLE[] = 
    " ,.,..:;?!\"*'`*^"
    "-_????????*---/\\"
    "~||--''\"\"()()[]{"
    "}<><>[][][]+-+X?"
    "-==<><>????*'\"CY"
    "$c&%#&*@S*******"
    "*******T><^_'='";

static const uint8_t psv_ps2key[0x10] = {
	0xEA, 0x02, 0xCE, 0xEF, 0x5B, 0xB4, 0xD2, 0x99, 0x8F, 0x61, 0x19, 0x10, 0xD7, 0x7F, 0x51, 0xC6
}; 

static const uint8_t psv_ps1key[0x10] = {
	0xAB, 0x5A, 0xBC, 0x9F, 0xC1, 0xF4, 0x9D, 0xE6, 0xA0, 0x51, 0xDB, 0xAE, 0xFA, 0x51, 0x88, 0x59
};

static const uint8_t psv_iv[0x10] = {
	0xB3, 0x0F, 0xFE, 0xED, 0xB7, 0xDC, 0x5E, 0xB7, 0x13, 0x3D, 0xA6, 0x0D, 0x1B, 0x6B, 0x2C, 0xDC
};


static void XorWithByte(uint8_t* buf, uint8_t byte, int length)
{
	for (int i = 0; i < length; ++i) {
    	buf[i] ^= byte;
	}
}

static void XorWithIv(uint8_t* buf, const uint8_t* Iv)
{
  uint8_t i;
  for (i = 0; i < 16; ++i) // The block in AES is always 128bit no matter the key size
  {
    buf[i] ^= Iv[i];
  }
}
 
static void generateHash(const uint8_t *input, const uint8_t *salt_seed, uint8_t *dest, size_t sz, uint8_t type)
{
	aes_context aes_ctx;
	sha1_context sha1_ctx;
	uint8_t iv[0x10];
	uint8_t salt[0x40];
	uint8_t work_buf[0x14];

	memset(salt , 0, sizeof(salt));
	memset(&aes_ctx, 0, sizeof(aes_context));

	LOG("Type detected: %d", type);
	if(type == PSV_TYPE_PS1)
	{	//PS1
		LOG("PS1 Save File");
		//idk why the normal cbc doesn't work.
		memcpy(work_buf, salt_seed, 0x10);

		aes_setkey_dec(&aes_ctx, psv_ps1key, 128);
		aes_crypt_ecb(&aes_ctx, AES_DECRYPT, work_buf, salt);
		aes_setkey_enc(&aes_ctx, psv_ps1key, 128);
		aes_crypt_ecb(&aes_ctx, AES_ENCRYPT, work_buf, salt + 0x10);

		XorWithIv(salt, psv_iv);

		memset(work_buf, 0xFF, sizeof(work_buf));
		memcpy(work_buf, salt_seed + 0x10, 0x4);

		XorWithIv(salt + 0x10, work_buf);
	} 
	else if(type == PSV_TYPE_PS2)
	{	//PS2
		LOG("PS2 Save File");
		memcpy(salt, salt_seed, 0x14);
		memcpy(iv, psv_iv, sizeof(iv));

		aes_setkey_dec(&aes_ctx, psv_ps2key, 128);
		aes_crypt_cbc(&aes_ctx, AES_DECRYPT, sizeof(salt), iv, salt, salt);
	}
	else
	{
		LOG("Error: Unknown type");
		return;
	}
	
	memset(salt + 0x14, 0, sizeof(salt) - 0x14);
	memset(dest, 0, 0x14);

	XorWithByte(salt, 0x36, sizeof(salt));

	memset(&sha1_ctx, 0, sizeof(sha1_context));
	sha1_starts(&sha1_ctx);
	sha1_update(&sha1_ctx, salt, sizeof(salt));
	sha1_update(&sha1_ctx, input, sz);
	sha1_finish(&sha1_ctx, work_buf);

	XorWithByte(salt, 0x6A, sizeof(salt));

	memset(&sha1_ctx, 0, sizeof(sha1_context));
	sha1_starts(&sha1_ctx);
	sha1_update(&sha1_ctx, salt, sizeof(salt));
	sha1_update(&sha1_ctx, work_buf, 0x14);
	sha1_finish(&sha1_ctx, dest);
}

int psv_resign(const char *src_psv)
{
	size_t sz;
	uint8_t *input;

	LOG("=====ps3-psvresigner by @dots_tb=====");
//	LOG("\nWith CBPS help especially: @AnalogMan151, @teakhanirons, Silica, @nyaaasen, and @notzecoxao\n");
//	LOG("Resigns non-console specific PS3 PSV savefiles. PSV files embed PS1 and PS2 save data. This does not inject!\n\n");

	if (read_buffer(src_psv, &input, &sz) < 0) {
		LOG("Failed to open input file");
		return 0;
	}

	LOG("File Size: %ld bytes", sz);

	if (memcmp(input, PSV_MAGIC, 4) != 0) {
		LOG("Not a PSV file");
		free(input);
		return 0;
	}

	generateHash(input, input + PSV_SEED_OFFSET, input + PSV_HASH_OFFSET, sz, input[PSV_TYPE_OFFSET]);

	LOG("New signature: ");
	dump_data(input + PSV_HASH_OFFSET, 0x14);

	if (write_buffer(src_psv, input, sz) < 0) {
		LOG("Failed to open output file");
		free(input);
		return 0;
	}

	free(input);
	LOG("PSV resigned successfully: %s\n", src_psv);

	return 1;
}

int vmp_resign(const char *src_vmp)
{
	size_t sz;
	uint8_t *input;

	LOG("=====Vita MCR2VMP by @dots_tb=====");

	if (read_buffer(src_vmp, &input, &sz) < 0) {
		LOG("Failed to open input file");
		return 0;
	}

	if (sz != VMP_SIZE || *(uint32_t*)input != VMP_MAGIC) {
		LOG("Not a VMP file");
		free(input);
		return 0;
	}

	generateHash(input, input + VMP_SEED_OFFSET, input + VMP_HASH_OFFSET, sz, 1);

	LOG("New signature:");
	dump_data(input+VMP_HASH_OFFSET, 20);

	if (write_buffer(src_vmp, input, sz) < 0) {
		LOG("Failed to open output file");
		free(input);
		return 0;
	}

	free(input);
	LOG("VMP resigned successfully: %s", src_vmp);

	return 1;
}

void get_psv_filename(char* psvName, const char* path, const char* dirName)
{
	char tmpName[4];
	const char *ch = &dirName[12];

	sprintf(psvName, "%s%.12s", path, dirName);
	while (*ch)
	{
		snprintf(tmpName, sizeof(tmpName), "%02X", *ch++);
		strcat(psvName, tmpName);
	}
	strcat(psvName, ".PSV");
}

void write_psv_header(FILE *fp, uint32_t type)
{
    psv_header_t ph;

    memset(&ph, 0, sizeof(psv_header_t));
    ph.headerSize = (type == PSV_TYPE_PS1) ? 0x14 : 0x2C;
    ph.saveType = type;
    memcpy(&ph.magic, PSV_MAGIC, sizeof(ph.magic));
    memcpy(&ph.salt, PSV_SALT, sizeof(ph.salt));

    fwrite(&ph, sizeof(psv_header_t), 1, fp);
}

//Convert Shift-JIS characters to ASCII equivalent
static void sjis2ascii(char* bData)
{
	uint16_t ch;
	int i, j = 0;
	int len = strlen(bData);
	
	for (i = 0; i < len; i += 2)
	{
		ch = (bData[i]<<8) | bData[i+1];

		// 'A' .. 'Z'
		// '0' .. '9'
		if ((ch >= 0x8260 && ch <= 0x8279) || (ch >= 0x824F && ch <= 0x8258))
		{
			bData[j++] = (ch & 0xFF) - 0x1F;
			continue;
		}

		// 'a' .. 'z'
		if (ch >= 0x8281 && ch <= 0x829A)
		{
			bData[j++] = (ch & 0xFF) - 0x20;
			continue;
		}

		if (ch >= 0x8140 && ch <= 0x81AC)
		{
			bData[j++] = SJIS_REPLACEMENT_TABLE[(ch & 0xFF) - 0x40];
			continue;
		}

		if (ch == 0x0000)
		{
			//End of the string
			bData[j] = 0;
			return;
		}

		// Character not found
		bData[j++] = bData[i];
		bData[j++] = bData[i+1];
	}

	bData[j] = 0;
	return;
}

// PSV files (PS1/PS2) savegame titles are stored in Shift-JIS
char* sjis2utf8(char* input)
{
    // Simplify the input and decode standard ASCII characters
    sjis2ascii(input);

    int len = strlen(input);
    char* output = malloc(3 * len); //ShiftJis won't give 4byte UTF8, so max. 3 byte per input char are needed
    size_t indexInput = 0, indexOutput = 0;

    while(indexInput < len)
    {
        char arraySection = ((uint8_t)input[indexInput]) >> 4;

        size_t arrayOffset;
        if(arraySection == 0x8) arrayOffset = 0x100; //these are two-byte shiftjis
        else if(arraySection == 0x9) arrayOffset = 0x1100;
        else if(arraySection == 0xE) arrayOffset = 0x2100;
        else arrayOffset = 0; //this is one byte shiftjis

        //determining real array offset
        if(arrayOffset)
        {
            arrayOffset += (((uint8_t)input[indexInput]) & 0xf) << 8;
            indexInput++;
            if(indexInput >= len) break;
        }
        arrayOffset += (uint8_t)input[indexInput++];
        arrayOffset <<= 1;

        //unicode number is...
        uint16_t unicodeValue = (shiftJIS_convTable[arrayOffset] << 8) | shiftJIS_convTable[arrayOffset + 1];

        //converting to UTF8
        if(unicodeValue < 0x80)
        {
            output[indexOutput++] = unicodeValue;
        }
        else if(unicodeValue < 0x800)
        {
            output[indexOutput++] = 0xC0 | (unicodeValue >> 6);
            output[indexOutput++] = 0x80 | (unicodeValue & 0x3f);
        }
        else
        {
            output[indexOutput++] = 0xE0 | (unicodeValue >> 12);
            output[indexOutput++] = 0x80 | ((unicodeValue & 0xfff) >> 6);
            output[indexOutput++] = 0x80 | (unicodeValue & 0x3f);
        }
    }

	//remove the unnecessary bytes
    output[indexOutput] = 0;
    return output;
}

int vmc_import_psv(const char *input)
{
	int fd, r;
	uint8_t *p;
	size_t filesize;
	char filepath[256];
    struct io_dirent entry;
    ps2_MainDirInfo_t *ps2md;
    ps2_FileInfo_t *ps2fi;

	if (read_buffer(input, &p, &filesize) < 0)
		return 0;

	if (memcmp(PSV_MAGIC, p, 4) != 0 || p[PSV_TYPE_OFFSET] != PSV_TYPE_PS2) {
		LOG("Not a PS2 .PSV file");
		free(p);
		return 0;
	}

	ps2md = (ps2_MainDirInfo_t *)&p[0x68];
	ps2fi = (ps2_FileInfo_t *)&ps2md[1];

	LOG("Writing data to: '/%s'...", ps2md->filename);

	r = mcio_mcMkDir(ps2md->filename);
	if (r < 0)
		LOG("Error: can't create directory '%s'... (%d)", ps2md->filename, r);
	else
		mcio_mcClose(r);

	for (int total = (read_le_uint32((uint8_t*)&ps2md->numberOfFilesInDir) - 2), i = 0; i < total; i++, ps2fi++)
	{
		filesize = read_le_uint32((uint8_t*)&ps2fi->filesize);

		snprintf(filepath, sizeof(filepath), "%s/%s", ps2md->filename, ps2fi->filename);
		LOG("Adding %-48s | %8d bytes", filepath, filesize);

		fd = mcio_mcOpen(filepath, sceMcFileCreateFile | sceMcFileAttrWriteable | sceMcFileAttrFile);
		if (fd < 0) {
			free(p);
			return 0;
		}

		r = mcio_mcWrite(fd, &p[read_le_uint32((uint8_t*)&ps2fi->positionInFile)], filesize);
		mcio_mcClose(fd);
		if (r != filesize) {
			free(p);
			return 0;
		}

		mcio_mcStat(filepath, &entry);
		memcpy(&entry.stat.ctime, &ps2fi->created, sizeof(struct sceMcStDateTime));
		memcpy(&entry.stat.mtime, &ps2fi->modified, sizeof(struct sceMcStDateTime));
		entry.stat.mode = read_le_uint32((uint8_t*)&ps2fi->attribute);
		mcio_mcSetStat(filepath, &entry);
	}

	mcio_mcStat(ps2md->filename, &entry);
	memcpy(&entry.stat.ctime, &ps2md->created, sizeof(struct sceMcStDateTime));
	memcpy(&entry.stat.mtime, &ps2md->modified, sizeof(struct sceMcStDateTime));
	entry.stat.mode = read_le_uint32((uint8_t*)&ps2md->attribute);
	mcio_mcSetStat(ps2md->filename, &entry);

	free(p);
	LOG("Save succesfully imported: %s", input);

	return 1;
}

int vmc_import_psu(const char *input)
{
	int fd, r;
	char filepath[256];
	struct io_dirent entry;
	McFsEntry psu_entry, file_entry;

	FILE *fh = fopen(input, "rb");
	if (fh == NULL)
	{
		LOG("ERROR! Could not open PSU file: %s", input);
		return 0;
	}

	fseek(fh, 0, SEEK_END);
	r = ftell(fh);

	if (!r || r % 512) {
		LOG("Not a .PSU file");
		fclose(fh);
		return 0;
	}

	LOG("Reading file: '%s'...", input);
	fseek(fh, 0, SEEK_SET);

	r = fread(&psu_entry, sizeof(McFsEntry), 1, fh);
	if (!r) {
		fclose(fh);
		return 0;
	}

	// Skip "." and ".."
	fseek(fh, sizeof(McFsEntry)*2, SEEK_CUR);

	LOG("Writing data to: '/%s'...", psu_entry.name);

	r = mcio_mcMkDir(psu_entry.name);
	if (r < 0)
		LOG("Error: can't create directory '%s'... (%d)", psu_entry.name, r);
	else
		mcio_mcClose(r);

	for (int total = (psu_entry.length - 2), i = 0; i < total; i++)
	{
		fread(&file_entry, 1, sizeof(McFsEntry), fh);

		snprintf(filepath, sizeof(filepath), "%s/%s", psu_entry.name, file_entry.name);
		LOG("Adding %-48s | %8d bytes", filepath, file_entry.length);

		fd = mcio_mcOpen(filepath, sceMcFileCreateFile | sceMcFileAttrWriteable | sceMcFileAttrFile);
		if (fd < 0)
			return 0;

		uint8_t *p = malloc(file_entry.length);
		if (!p)
		{
			mcio_mcClose(fd);
			return 0;
		}

		fread(p, 1, file_entry.length, fh);
		r = mcio_mcWrite(fd, p, file_entry.length);
		mcio_mcClose(fd);
		free(p);

		if (r != (int)file_entry.length)
			return 0;

		mcio_mcStat(filepath, &entry);
		memcpy(&entry.stat.ctime, &file_entry.created, sizeof(struct sceMcStDateTime));
		memcpy(&entry.stat.mtime, &file_entry.modified, sizeof(struct sceMcStDateTime));
		entry.stat.mode = file_entry.mode;
		mcio_mcSetStat(filepath, &entry);

		r = 1024 - (file_entry.length % 1024);
		if(r < 1024)
			fseek(fh, r, SEEK_CUR);
	}

	mcio_mcStat(psu_entry.name, &entry);
	memcpy(&entry.stat.ctime, &psu_entry.created, sizeof(struct sceMcStDateTime));
	memcpy(&entry.stat.mtime, &psu_entry.modified, sizeof(struct sceMcStDateTime));
	entry.stat.mode = psu_entry.mode;
	mcio_mcSetStat(psu_entry.name, &entry);

	return 1;
}

int vmc_export_psv(const char* save, const char* out_path)
{
	FILE *psvFile;
	ps2_header_t ps2h;
	ps2_FileInfo_t *ps2fi;
	ps2_MainDirInfo_t ps2md;
	ps2_IconSys_t iconsys;
	uint32_t dataPos, i;
	char filePath[256];
	uint8_t *data;
	int r, dd, fd;
	struct io_dirent dirent;

	snprintf(filePath, sizeof(filePath), "%s/icon.sys", save);
	fd = mcio_mcOpen(filePath, sceMcFileAttrReadable | sceMcFileAttrFile);
	if (fd < 0)
	{
		LOG("Error! '%s' not found", filePath);
		return 0;
	}

	r = mcio_mcRead(fd, &iconsys, sizeof(ps2_IconSys_t));
	mcio_mcClose(fd);

	if (r != sizeof(ps2_IconSys_t))
		return 0;

	get_psv_filename(filePath, out_path, save);

	LOG("Export %s -> %s ...", save, filePath);
	psvFile = fopen(filePath, "wb");
	if(!psvFile)
		return 0;

	write_psv_header(psvFile, PSV_TYPE_PS2);

	memset(&ps2h, 0, sizeof(ps2_header_t));
	memset(&ps2md, 0, sizeof(ps2_MainDirInfo_t));

	// Read main directory entry
	mcio_mcStat(save, &dirent);

	// PSV root directory values
	ps2h.numberOfFiles = dirent.stat.size - 2;
	ps2md.attribute = dirent.stat.mode;
	ps2md.numberOfFilesInDir = dirent.stat.size;
	memcpy(&ps2md.created, &dirent.stat.ctime, sizeof(sceMcStDateTime));
	memcpy(&ps2md.modified, &dirent.stat.mtime, sizeof(sceMcStDateTime));
	memcpy(ps2md.filename, dirent.name, sizeof(ps2md.filename));

	dd = mcio_mcDopen(save);
	if (dd < 0)
	{
		fclose(psvFile);
		return 0;
	}

	// Calculate the start offset for the file's data
	dataPos = sizeof(psv_header_t) + sizeof(ps2_header_t) + sizeof(ps2_MainDirInfo_t) + sizeof(ps2_FileInfo_t)*ps2h.numberOfFiles;
	ps2fi = malloc(sizeof(ps2_FileInfo_t)*ps2h.numberOfFiles);

	// Build the PS2 FileInfo entries
	i = 0;
	do {
		r = mcio_mcDread(dd, &dirent);
		if (r && (strcmp(dirent.name, ".")) && (strcmp(dirent.name, "..")))
		{
			snprintf(filePath, sizeof(filePath), "%s/%s", save, dirent.name);
			mcio_mcStat(filePath, &dirent);

			ps2fi[i].attribute = dirent.stat.mode;
			ps2fi[i].positionInFile = dataPos;
			ps2fi[i].filesize = dirent.stat.size;
			memcpy(&ps2fi[i].created, &dirent.stat.ctime, sizeof(sceMcStDateTime));
			memcpy(&ps2fi[i].modified, &dirent.stat.mtime, sizeof(sceMcStDateTime));
			memcpy(ps2fi[i].filename, dirent.name, sizeof(ps2fi[i].filename));

			dataPos += dirent.stat.size;
			ps2h.displaySize += dirent.stat.size;

			if (strcmp(ps2fi[i].filename, iconsys.IconName) == 0)
			{
				ps2h.icon1Size = ps2fi[i].filesize;
				ps2h.icon1Pos = ps2fi[i].positionInFile;
			}

			if (strcmp(ps2fi[i].filename, iconsys.copyIconName) == 0)
			{
				ps2h.icon2Size = ps2fi[i].filesize;
				ps2h.icon2Pos = ps2fi[i].positionInFile;
			}

			if (strcmp(ps2fi[i].filename, iconsys.deleteIconName) == 0)
			{
				ps2h.icon3Size = ps2fi[i].filesize;
				ps2h.icon3Pos = ps2fi[i].positionInFile;
			}

			if(strcmp(ps2fi[i].filename, "icon.sys") == 0)
			{
				ps2h.sysSize = ps2fi[i].filesize;
				ps2h.sysPos = ps2fi[i].positionInFile;
			}
			i++;
		}
	} while (r);

	mcio_mcDclose(dd);

	LOG("%d Files: %8d Total bytes", i, ps2h.displaySize);

	fwrite(&ps2h, sizeof(ps2_header_t), 1, psvFile);
	fwrite(&ps2md, sizeof(ps2_MainDirInfo_t), 1, psvFile);
	fwrite(ps2fi, sizeof(ps2_FileInfo_t), ps2h.numberOfFiles, psvFile);
	free(ps2fi);

	// Write the file's data
	dd = mcio_mcDopen(save);

	i = 0;
	do {
		r = mcio_mcDread(dd, &dirent);
		if (r && (strcmp(dirent.name, ".")) && (strcmp(dirent.name, "..")))
		{
			snprintf(filePath, sizeof(filePath), "%s/%s", save, dirent.name);
			LOG("(%d/%d) Add '%s'", ++i, ps2h.numberOfFiles, filePath);

			fd = mcio_mcOpen(filePath, sceMcFileAttrReadable | sceMcFileAttrFile);
			if (fd < 0)
			{
				i = 0;
				break;
			}

			data = malloc(dirent.stat.size);
			if (!data)
			{
				i = 0;
				break;
			}

			r = mcio_mcRead(fd, data, dirent.stat.size);
			mcio_mcClose(fd);

			if (r != (int)dirent.stat.size)
			{
				i = 0;
				free(data);
				break;
			}

			fwrite(data, 1, dirent.stat.size, psvFile);
			free(data);
		}
	} while (r);

	mcio_mcDclose(dd);

	fclose(psvFile);
	get_psv_filename(filePath, out_path, save);
	psv_resign(filePath);

	return (i > 0);
}

int vmc_export_psu(const char* path, const char* output)
{
	int r, fd, dd;
	uint32_t total, i = 0;
	struct io_dirent dirent;
	McFsEntry entry;
	char filepath[256];

	LOG("Exporting '%s' to %s...", path, output);

	dd = mcio_mcDopen(path);
	if (dd < 0)
		return 0;

	FILE *fh = fopen(output, "wb");
	if (fh == NULL) {
		mcio_mcDclose(dd);
		return 0;
	}

	// Read main directory entry
	mcio_mcStat(path, &dirent);

	memset(&entry, 0, sizeof(McFsEntry));
	memcpy(&entry.created, &dirent.stat.ctime, sizeof(sceMcStDateTime));
	memcpy(&entry.modified, &dirent.stat.mtime, sizeof(sceMcStDateTime));
	memcpy(entry.name, dirent.name, sizeof(entry.name));
	entry.mode = dirent.stat.mode;
	entry.length = dirent.stat.size;
	fwrite(&entry, sizeof(McFsEntry), 1, fh);
	total = dirent.stat.size - 2;

	// "."
	memset(entry.name, 0, sizeof(entry.name));
	strncpy(entry.name, ".", sizeof(entry.name));
	entry.length = 0;
	fwrite(&entry, sizeof(McFsEntry), 1, fh);

	// ".."
	strncpy(entry.name, "..", sizeof(entry.name));
	fwrite(&entry, sizeof(McFsEntry), 1, fh);

	do {
		r = mcio_mcDread(dd, &dirent);
		if (r && (strcmp(dirent.name, ".")) && (strcmp(dirent.name, "..")))
		{
			snprintf(filepath, sizeof(filepath), "%s/%s", path, dirent.name);
			LOG("Adding (%d/%d) %-40s | %8d bytes", ++i, total, filepath, dirent.stat.size);

			mcio_mcStat(filepath, &dirent);

			memset(&entry, 0, sizeof(McFsEntry));
			memcpy(&entry.created, &dirent.stat.ctime, sizeof(sceMcStDateTime));
			memcpy(&entry.modified, &dirent.stat.mtime, sizeof(sceMcStDateTime));
			memcpy(entry.name, dirent.name, sizeof(entry.name));
			entry.mode = dirent.stat.mode;
			entry.length = dirent.stat.size;
			fwrite(&entry, sizeof(McFsEntry), 1, fh);

			fd = mcio_mcOpen(filepath, sceMcFileAttrReadable | sceMcFileAttrFile);
			if (fd < 0) {
				i = 0;
				break;
			}

			uint8_t *p = malloc(dirent.stat.size);
			if (p == NULL) {
				i = 0;
				break;
			}

			r = mcio_mcRead(fd, p, dirent.stat.size);
			mcio_mcClose(fd);

			if (r != (int)dirent.stat.size) {
				free(p);
				i = 0;
				break;
			}

			r = fwrite(p, 1, dirent.stat.size, fh);
			if (r != (int)dirent.stat.size) {
				fclose(fh);
				free(p);
				i = 0;
				break;
			}
			free(p);

			entry.length = 1024 - (dirent.stat.size % 1024);
			while(--entry.length < 1023)
				fputc(0xFF, fh);
		}
	} while (r);

	mcio_mcDclose(dd);
	fclose(fh);

	LOG("Save succesfully exported to %s.", output);

	return (i > 0);
}

int vmc_delete_save(const char* path)
{
	int r, dd;
	struct io_dirent dirent;
	char filepath[256];

	LOG("Deleting '%s'...", path);

	dd = mcio_mcDopen(path);
	if (dd < 0)
		return 0;

	do {
		r = mcio_mcDread(dd, &dirent);
		if (r && (strcmp(dirent.name, ".")) && (strcmp(dirent.name, "..")))
		{
			snprintf(filepath, sizeof(filepath), "%s/%s", path, dirent.name);
			LOG("Deleting '%s'", filepath);
			mcio_mcRemove(filepath);
		}
	} while (r);

	mcio_mcDclose(dd);
	r = mcio_mcRmDir(path);

	return (r == sceMcResSucceed);
}
