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
#include "shiftjis.h"

#define PSV_SEED_OFFSET 0x8
#define PSV_HASH_OFFSET 0x1C
#define PSV_TYPE_OFFSET 0x3C

const char SJIS_REPLACEMENT_TABLE[] = 
    " ,.,..:;?!\"*'`*^"
    "-_????????*---/\\"
    "~||--''\"\"()()[]{"
    "}<><>[][][]+-+X?"
    "-==<><>????*'\"CY"
    "$c&%#&*@S*******"
    "*******T><^_'='";

const uint8_t psv_ps2key[0x10] = {
	0xFA, 0x72, 0xCE, 0xEF, 0x59, 0xB4, 0xD2, 0x98, 0x9F, 0x11, 0x19, 0x13, 0x28, 0x7F, 0x51, 0xC7
}; 

const uint8_t psv_ps1key[0x10] = {
	0xAB, 0x5A, 0xBC, 0x9F, 0xC1, 0xF4, 0x9D, 0xE6, 0xA0, 0x51, 0xDB, 0xAE, 0xFA, 0x51, 0x88, 0x59
};

const uint8_t psv_iv[0x10] = {
	0xB3, 0x0F, 0xFE, 0xED, 0xB7, 0xDC, 0x5E, 0xB7, 0x13, 0x3D, 0xA6, 0x0D, 0x1B, 0x6B, 0x2C, 0xDC
};


void XorWithByte(uint8_t* buf, uint8_t byte, int length)
{
	for (int i = 0; i < length; ++i) {
    	buf[i] ^= byte;
	}
}

void XorWithIv(uint8_t* buf, const uint8_t* Iv)
{
  uint8_t i;
  for (i = 0; i < 16; ++i) // The block in AES is always 128bit no matter the key size
  {
    buf[i] ^= Iv[i];
  }
}
 
void generateHash(const uint8_t *input, uint8_t *dest, size_t sz, uint8_t type)
{
	aes_context aes_ctx;
	sha1_context sha1_ctx;
	uint8_t iv[0x10];
	uint8_t salt[0x40];
	uint8_t work_buf[0x14];
	const uint8_t *salt_seed = input + PSV_SEED_OFFSET;

	memset(salt , 0, sizeof(salt));
	memset(&aes_ctx, 0, sizeof(aes_context));

	LOG("Type detected: %d", type);
	if(type == 1)
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
	else if(type == 2)
	{	//PS2
		LOG("PS2 Save File");
		uint8_t laid_paid[16]  = {	
			0x10, 0x70, 0x00, 0x00, 0x02, 0x00, 0x00, 0x01, 0x10, 0x70, 0x00, 0x03, 0xFF, 0x00, 0x00, 0x01 };

		memcpy(salt, salt_seed, 0x14);
		memcpy(iv, psv_iv, sizeof(iv));
		XorWithIv(laid_paid, psv_ps2key);

		aes_setkey_dec(&aes_ctx, laid_paid, 128);
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

	LOG("File Size: %ld bytes\n", sz);

	if (memcmp(input, PSV_MAGIC, 4) != 0) {
		LOG("Not a PSV file");
		free(input);
		return 0;
	}
	
	LOG("Old signature: ");
	for(int i = 0; i < 0x14; i++ ) {
		LOG("%02X ", input[PSV_HASH_OFFSET + i]);
	}

	generateHash(input, input + PSV_HASH_OFFSET, sz, input[PSV_TYPE_OFFSET]);

	LOG("New signature: ");
	for(int i = 0; i < 0x14; i++ ) {
		LOG("%02X ", input[PSV_HASH_OFFSET + i]);
	}

	if (write_buffer(src_psv, input, sz) < 0) {
		LOG("Failed to open output file");
		free(input);
		return 0;
	}

	free(input);
	LOG("PSV resigned successfully: %s\n", src_psv);

	return 1;
}

void get_psv_filename(char* psvName, const char* path, const char* dirName)
{
	char tmpName[13];
	const char *ch = &dirName[12];

	memcpy(tmpName, dirName, 12);
	tmpName[12] = 0;

	strcpy(psvName, path);
	strcat(psvName, tmpName);
	while (*ch)
	{
		snprintf(tmpName, sizeof(tmpName), "%02X", *ch++);
		strcat(psvName, tmpName);
	}
	strcat(psvName, ".PSV");
}

void write_psvheader(FILE *fp, uint32_t type)
{
    psv_header_t ph;

    memset(&ph, 0, sizeof(psv_header_t));
    ph.headerSize = (type == 1) ? 0x14000000 : 0x2C000000;
    ph.saveType = ES32(type);
    memcpy(&ph.magic, PSV_MAGIC, sizeof(ph.magic));
    memcpy(&ph.salt, PSV_SALT, sizeof(ph.salt));

    fwrite(&ph, sizeof(psv_header_t), 1, fp);
}

int ps1_mcs2psv(const char* mcsfile, const char* psv_path)
{
	char dstName[256];
	size_t sz;
	uint8_t *input;
	FILE *pf;
	ps1_header_t ps1h;

	if (read_buffer(mcsfile, &input, &sz) < 0) {
		LOG("Failed to open input file");
		return 0;
	}

	if (memcmp(input, "Q\x00", 2) != 0) {
		printf("Not a .mcs file\n");
		free(input);
		return 0;
	}
	
	get_psv_filename(dstName, psv_path, (char*) &input[0x0A]);
	pf = fopen(dstName, "wb");
	if (!pf) {
		perror("Failed to open output file");
		free(input);
		return 0;
	}
	
	write_psvheader(pf, 1);

	memset(&ps1h, 0, sizeof(ps1_header_t));
	ps1h.saveSize = ES32(sz - 0x80);
	ps1h.startOfSaveData = 0x84000000;
	ps1h.blockSize = 0x00020000;
	ps1h.dataSize = ps1h.saveSize;
	ps1h.unknown1 = 0x03900000;
	memcpy(ps1h.prodCode, input + 0x0A, sizeof(ps1h.prodCode));

	fwrite(&ps1h, sizeof(ps1_header_t), 1, pf);
	fwrite(input + 0x80, sz - 0x80, 1, pf);
	fclose(pf);
	free(input);

	psv_resign(dstName);

	return 1;
}

int ps1_psv2mcs(const char* psvfile, const char* mcs_path)
{
	char dstName[256];
	uint8_t mcshdr[128];
	size_t sz;
	uint8_t *input;
	FILE *pf;
	ps1_header_t *ps1h;

	if (read_buffer(psvfile, &input, &sz) < 0) {
		LOG("Failed to open input file");
		return 0;
	}

	if (memcmp(input, PSV_MAGIC, 4) != 0) {
		printf("Not a .psv file");
		free(input);
		return 0;
	}

	snprintf(dstName, sizeof(dstName), "%s%s.mcs", mcs_path, strrchr(psvfile, '/')+1);
	pf = fopen(dstName, "wb");
	if (!pf) {
		perror("Failed to open output file");
		free(input);
		return 0;
	}
	
	ps1h = (ps1_header_t*)(input + 0x40);

	memset(mcshdr, 0, sizeof(mcshdr));
	memcpy(mcshdr + 4, &ps1h->saveSize, 4);
	memcpy(mcshdr + 56, &ps1h->saveSize, 4);
	memcpy(mcshdr + 10, ps1h->prodCode, sizeof(ps1h->prodCode));
	mcshdr[0] = 0x51;
	mcshdr[8] = 0xFF;
	mcshdr[9] = 0xFF;

	for (int x=0; x<127; x++)
		mcshdr[127] ^= mcshdr[x];

	fwrite(mcshdr, sizeof(mcshdr), 1, pf);
	fwrite(input + 0x84, sz - 0x84, 1, pf);
	fclose(pf);
	free(input);

	return 1;
}

int ps1_psx2psv(const char* psxfile, const char* psv_path)
{
	char dstName[256];
	size_t sz;
	uint8_t *input;
	FILE *pf;
	ps1_header_t ps1h;

	if (read_buffer(psxfile, &input, &sz) < 0) {
		LOG("Failed to open input file");
		return 0;
	}

	if (memcmp(input + 0x36, "SC", 2) != 0) {
		printf("Not a .psx file\n");
		free(input);
		return 0;
	}
	
	get_psv_filename(dstName, psv_path, (char*) input);
	pf = fopen(dstName, "wb");
	if (!pf) {
		perror("Failed to open output file");
		free(input);
		return 0;
	}
	
	write_psvheader(pf, 1);

	memset(&ps1h, 0, sizeof(ps1_header_t));
	ps1h.saveSize = ES32(sz - 0x36);
	ps1h.startOfSaveData = 0x84000000;
	ps1h.blockSize = 0x00020000;
	ps1h.dataSize = ps1h.saveSize;
	ps1h.unknown1 = 0x03900000;
	memcpy(ps1h.prodCode, input, sizeof(ps1h.prodCode));

	fwrite(&ps1h, sizeof(ps1_header_t), 1, pf);
	fwrite(input + 0x36, sz - 0x36, 1, pf);
	fclose(pf);
	free(input);

	psv_resign(dstName);

	return 1;
}

//Convert Shift-JIS characters to ASCII equivalent
void sjis2ascii(char* bData)
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
