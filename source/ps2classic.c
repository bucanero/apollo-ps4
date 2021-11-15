/*+-----------------------------+*/
/*|2013 USER                    |*/
/*|decrypt algos by flatz       |*/
/*|                             |*/
/*|GPL v3,  DO NOT USE IF YOU   |*/
/*|DISAGREE TO RELEASE SRC :P   |*/
/*+-----------------------------+*/

#include "types.h"
#include "ps2_data.h"
#include "settings.h"
#include "common.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <polarssl/aes.h>
#include <polarssl/sha1.h>

#define PS2_META_SEGMENT_START      1
#define PS2_DATA_SEGMENT_START      2
#define PS2_DEFAULT_SEGMENT_SIZE    0x4000
#define PS2_META_ENTRY_SIZE         0x20
#define PS2_CONFIG_NAME             "CONFIG"
#define PS2_REAL_OUT_NAME           "ISO.BIN.ENC"
#define PS2_PLACEHOLDER_CID         "2P0001-PS2U10000_00-0000111122223333"
#define PS2_REACTPSN_CID            "UP0001-RPS200000_00-0000000000000000"

#define PS2_VMC_ENCRYPT             1
#define PS2_VMC_DECRYPT             0

#define PS2_VMC_RAW_8MB             0x800000
#define PS2_VMC_RAW_16MB            0x1000000
#define PS2_VMC_RAW_32MB            0x2000000
#define PS2_VMC_RAW_64MB            0x4000000
#define PS2_VMC_ECC_SIZE(x)         x/32 + x
#define PS2_VMC_DATASIZE            512

#define be32(x)                     *(u32*)(x)
#define be64(x)                     *(u64*)(x)

typedef struct
{
	uint8_t cluster[4][128];
	uint8_t ecc[4][3];
	uint32_t padding;
} ps2_block_t;


//prototypes
static void build_ps2_header(u8 * buffer, int npd_type, const char* content_id, const char* filename, s64 iso_size);
int vmc_hash(const char *mc_in);
int ps2_iso9660_sig(const char *img_in);
void ps2_build_limg(const char *img_in, int64_t size);
int rif2klicensee(const u8* idps_key, const char* exdata_path, const char* rif_file, u8* klic);
int64_t get_fsize(const char* fname);


void aes128cbc(const u8 *key, const u8 *iv_in, const u8 *in, u64 len, u8 *out)
{
	u8 iv[16];
	aes_context ctx;

	memcpy(iv, iv_in, 16);
	memset(&ctx, 0, sizeof(aes_context));
	aes_setkey_dec(&ctx, key, 128);
	aes_crypt_cbc(&ctx, AES_DECRYPT, len, iv, in, out);
}

void aes128cbc_enc(const u8 *key, const u8 *iv_in, const u8 *in, u64 len, u8 *out)
{
	u8 iv[16];
	aes_context ctx;

	memcpy(iv, iv_in, 16);
	memset(&ctx, 0, sizeof(aes_context));
	aes_setkey_enc(&ctx, key, 128);
	aes_crypt_cbc(&ctx, AES_ENCRYPT, len, iv, in, out);
}

void rol1(u8* worthless) {
	int i;
	u8 xor = (worthless[0]&0x80)?0x87:0;

	for(i=0;i<0xF;i++) {
		worthless[i] <<= 1;
		worthless[i] |= worthless[i+1]>>7;
	}
	worthless[0xF] <<= 1;
	worthless[0xF] ^= xor;
}

void aesOmacMode1(u8* output, const u8* input, int len, const u8* aes_key_data, int aes_key_bits)
{
	int i,j;
	i = 0;
	u8 running[0x10];
	u8 hash[0x10];
	u8 worthless[0x10];
	aes_context ctx;

	memset(running, 0, 0x10);
	memset(&ctx, 0, sizeof(aes_context));

	aes_setkey_enc(&ctx, aes_key_data, aes_key_bits);
	aes_crypt_ecb(&ctx, AES_ENCRYPT, running, worthless);
	rol1(worthless);

	if(len > 0x10) {
		for(i=0;i<(len-0x10);i+=0x10) {
			for(j=0;j<0x10;j++) hash[j] = running[j] ^ input[i+j];
			aes_crypt_ecb(&ctx, AES_ENCRYPT, hash, running);
		}
	}
	int overrun = len&0xF;
	if( (len%0x10) == 0 ) overrun = 0x10;

	memset(hash, 0, 0x10);
	memcpy(hash, &input[i], overrun);

	if(overrun != 0x10) {
		hash[overrun] = 0x80;
		rol1(worthless);
	}

	for(j=0;j<0x10;j++) hash[j] ^= running[j] ^ worthless[j];
	aes_crypt_ecb(&ctx, AES_ENCRYPT, hash, output);
}

/*
 *  vmc proper hash
 */
int vmc_hash(const char *mc_in)
{    
    int i, segment_count = 0, TotalSize=0 ;
    uint8_t *segment_hashes, segment_data[PS2_DEFAULT_SEGMENT_SIZE], sha1_hash[0x14];
    u32 read = 0;
    int64_t mc_size = 0;
    
    FILE *f = fopen(mc_in, "rb+");
    
    if(fseek(f, 0, SEEK_SET) == 0) {
        uint8_t buf[8];
        
        if(fread(buf, 1, 8, f)) {
            if (memcmp(buf, "Sony PS2", sizeof(buf)) != 0)
            {
                LOG("Not a valid Virtual Memory Card...:\n");
                
                fclose(f);
                return(0);
            }
        }
        
        fseeko(f, 0, SEEK_END);
        TotalSize = ftello(f);
        
        segment_count = TotalSize / sizeof(segment_data);
        
        TotalSize = (((segment_count * sizeof(sha1_hash)) / PS2_DEFAULT_SEGMENT_SIZE) + 1) * PS2_DEFAULT_SEGMENT_SIZE;
        segment_hashes = malloc(TotalSize);
        memset(segment_hashes, 0, TotalSize);

        for (i = 0; i < segment_count; i++)
        {
            fseek(f, (i * sizeof(segment_data)), SEEK_SET);
            read = fread(segment_data, 1, sizeof(segment_data), f);
            sha1(segment_data, sizeof(segment_data), sha1_hash);
            memcpy(segment_hashes + i * sizeof(sha1_hash), sha1_hash, sizeof(sha1_hash));
            
            mc_size += read;
            LOG("Rehash: Block %03d | Offset 0x%08lx", i, mc_size);
        }
        
        fseek(f, (segment_count * sizeof(segment_data)), SEEK_SET);
        fwrite(segment_hashes, 1, TotalSize, f);
        fclose(f);
        free(segment_hashes);
        
        LOG("Done...");
    }
    return 1;
}

/*
* ECC source code by hanimar@geocities.com
* http://www.geocities.com/SiliconValley/Station/8269/sma02/sma02.html#ecc
*/
void calc_ECC(uint8_t *ecc, const uint8_t *data)
{
	int i, c;

	ecc[0] = ecc[1] = ecc[2] = 0;

	for (i = 0 ; i < 0x80 ; i ++) {
		c = ECC_Table[data[i]];

		ecc[0] ^= c;
		if (c & 0x80) {
			ecc[1] ^= ~i;
			ecc[2] ^= i;
		}
	}
	ecc[0] = ~ecc[0] & 0x77;
	ecc[1] = ~ecc[1] & 0x7f;
	ecc[2] = ~ecc[2] & 0x7f;

	return;
}

int ps2_add_vmc_ecc(const char* src, const char* dst)
{
	ps2_block_t block;
	int i;
	int64_t vmc_size = get_fsize(src);

	switch (vmc_size)
	{
	case PS2_VMC_RAW_8MB:
	case PS2_VMC_RAW_16MB:
	case PS2_VMC_RAW_32MB:
		LOG("Adding ECC to '%s' (0x%X)", src, vmc_size);
		break;

	case PS2_VMC_RAW_64MB:
	case PS2_VMC_ECC_SIZE(PS2_VMC_RAW_64MB):
		LOG("Error: 64Mb memcards are not supported by the PS3");
		return 0;

	case PS2_VMC_ECC_SIZE(PS2_VMC_RAW_8MB):
	case PS2_VMC_ECC_SIZE(PS2_VMC_RAW_16MB):
	case PS2_VMC_ECC_SIZE(PS2_VMC_RAW_32MB):
		LOG("[!] File already has ECC '%s' (0x%X)", src, vmc_size);
		copy_file(src, dst);
		return 1;

	default:
		LOG("Error: invalid memcard size");
		return 0;
	}

	FILE *in = fopen(src, "rb");
	FILE *fo = fopen(dst, "wb");

	if (!in || !fo)
		return 0;

	while(fread(&block, 1, PS2_VMC_DATASIZE, in) > 0)
	{
		block.padding = 0;
		for (i = 0; i < 4; i++)
			calc_ECC(block.ecc[i], block.cluster[i]);

		fwrite(&block, sizeof(block), 1, fo);
	}

	fclose(in);
	fclose(fo);

	return 1;
}

int ps2_remove_vmc_ecc(const char* src, const char* dst)
{
	ps2_block_t block;

	FILE *in = fopen(src, "rb");
	FILE *fo = fopen(dst, "wb");

	if (!in || !fo)
		return 0;

	while(fread(&block, 1, sizeof(ps2_block_t), in) > 0)
	{
		fwrite(&block, PS2_VMC_DATASIZE, 1, fo);
	}

	fclose(in);
	fclose(fo);

	return 1;
}

/*
 *  ps2_iso9660_sig
 */
int ps2_iso9660_sig(const char *img_in)
{
	FILE *f = fopen(img_in, "rb");
    
	/* iso9660 offset DVD */
	if(fseek(f, 0x8000, SEEK_SET) == 0) {
		uint8_t buf[8];

		if(fread(buf, 1, 6, f)) {
			if(buf[0] != 0x01 && buf[1] != 0x43 && buf[2] != 0x44 && buf[3] != 0x30 && buf[4] != 0x30 && buf[5] != 0x31)
            {
                /* NOW TRY CD OFFSET */
                fseek(f, 0x9318, SEEK_SET);
                memset(buf, 0, sizeof(buf));
                                
                if(fread(buf, 1, 6, f)) {
                    if(buf[0] != 0x01 && buf[1] != 0x43 && buf[2] != 0x44 && buf[3] != 0x30 && buf[4] != 0x30 && buf[5] != 0x31) {
                        
                        LOG("Not a valid ISO9660 Image...:\n");
                        LOG(" input\t\t%s [ERROR]\n", img_in);
                        
                        fclose(f);
                        return(0);
                    }
                }
            }
        }
	}
	fclose(f);

	return 1;
}

/*
 * ps2_build_limg
 */
void ps2_build_limg(const char *img_in, int64_t size) {
	int i, b_size = 0;
	uint8_t buf[8], tmp[8], num_sectors[8];
	int64_t tsize=0;

	FILE *f = fopen(img_in, "rb+");

	/* limg offset */
	if(fseek(f, -0x4000, SEEK_END) == 0) {
		if(fread(buf, 1, 4, f)) {
			/* skip */
			if(buf[0] == 0x4c && buf[1] == 0x49 && buf[2] == 0x4d && buf[3] == 0x47) {
				LOG("[*] LIMG Header:\n\t\t[OK]\n");
				fclose(f);
				return;
			}
		}
	}
	
	/* seek start */
	fseek(f, 0, SEEK_SET);
	
	// Get MSB Value size, DVD or CD
	if(size > 0x2BC00000) {
        fseek(f, 0x8000 + 0x54, SEEK_SET);
        fread(num_sectors, 1, 4, f);
    } else {
        fseek(f, 0x9318 + 0x54, SEEK_SET);
        fread(num_sectors, 1, 4, f);
    }

	for(i = 0; i < 8; i++)
		buf[i] = tmp[i] = 0x00;
	
	tsize = size;

	/* seek end */
	fseek(f, 0, SEEK_END);
	
	/* Check if image is multiple of 0x4000, if not then pad with zeros*/
	while((tsize%16384) != 0) {
        tsize++;
        fwrite(&buf[0], 1, 1, f);
    }
    
    LOG("Multiple of 0x4000 [OK]\n");

	if(size > 0x2BC00000)
		b_size = 0x800; /* dvd */
	else
		b_size = 0x930; /* cd */

	if(b_size == 0x800) {
		buf[3] = 0x01;
		tmp[2] = 0x08;
	} else {
		buf[3] = 0x02;
		tmp[2] = 0x09;
		tmp[3] = 0x30;
	}

	/* print output */
	LOG("[*] LIMG Header:\n 4C 49 4D 47 ");

	fwrite("LIMG", 1, 4, f);
	fwrite(buf, 1, 4, f);

	/* print output */
	for(i = 0; i < 4; i++)
		LOG("%02X ", buf[i]);

	/* write number of sectors*/
	fwrite(num_sectors, 1, 4, f);

	/* print output */
	for(i = 0; i < 4; i++)
		LOG("%02X ", num_sectors[i]);

	fwrite(tmp, 1, 4, f);

	/* print output */
	for(i = 0; i < 4; i++)
		LOG("%02X ", tmp[i]);

	LOG("\n");

	/* 0x4000 - 0x10 - Rellena con ceros la seccion final de 16384 bytes */
	for(i = 0; i < 0x3FF0; i++)
		fwrite(&buf[0], 1, 1, f);

	fclose(f);
}

int get_image_klicensee(const char* fname, u8* klic)
{
	char cid[37];
	FILE* fp;
	
	fp = fopen(fname, "rb");
	fseek(fp, 0x10, SEEK_SET);
	fread(cid, 1, sizeof(cid), fp);
	fclose(fp);

	if (memcmp(cid, PS2_PLACEHOLDER_CID, 36) == 0)
	{
		memcpy(klic, ps2_placeholder_klic, 16);
		return 1;
	}
	else if (memcmp(cid, PS2_REACTPSN_CID, 36) == 0)
	{
		memcpy(klic, ps2_reactpsn_klic, 16);
		return 1;
	}
	else
	{
		/* look for .rif and get klic */
		char path[256];
		struct dirent *dir;

		DIR *d = opendir("/dev_hdd0/home/");

		if (!d)
			return 0;

		while ((dir = readdir(d)) != NULL)
		{
			snprintf(path, sizeof(path), "/dev_hdd0/home/%s/exdata/%s.rif", dir->d_name, cid);
			if (dir->d_type == DT_DIR && file_exists(path) == SUCCESS)
			{
				snprintf(path, sizeof(path), "/dev_hdd0/home/%s/exdata//%s.rif", dir->d_name, cid);
				char *rif = strrchr(path, '/');
				*rif = 0;

return 0;
//				return rif2klicensee((u8*) apollo_config.idps, path, ++rif, klic);
			}
		}
		closedir(d);
	}
	
	return 0;
}

void wbe16(u8* buf, u16 data)
{
	memcpy(buf, &data, sizeof(u16));
}

void wbe32(u8* buf, u32 data)
{
	memcpy(buf, &data, sizeof(u32));
}

void wbe64(u8* buf, u64 data)
{
	memcpy(buf, &data, sizeof(u64));
}

static void build_ps2_header(u8 * buffer, int npd_type, const char* content_id, const char* filename, s64 iso_size)
{
	int i;

	wbe32(buffer, 0x50533200);			// PS2\0
	wbe16(buffer + 0x4, 0x1);			// ver major
	wbe16(buffer + 0x6, 0x1);			// ver minor
	wbe32(buffer + 0x8, npd_type);		// NPD type XX
	wbe32(buffer + 0xc, 0x1);			// type
		
	wbe64(buffer + 0x88, iso_size); 		//iso size
	wbe32(buffer + 0x84, PS2_DEFAULT_SEGMENT_SIZE); //segment size
	
	strncpy((char*)(buffer + 0x10), content_id, 0x30);

	u8 npd_omac_key[0x10];

	for(i=0;i<0x10;i++) npd_omac_key[i] = npd_kek[i] ^ npd_omac_key2[i];

	memcpy(buffer + 0x40, "bucanero.com.ar", 0x10);

  	int buf_len = 0x30+strlen(filename);
	char *buf = (char*)malloc(buf_len+1);
	memcpy(buf, buffer + 0x10, 0x30);
	strcpy(buf+0x30, filename);
	aesOmacMode1(buffer + 0x50, (u8*)buf, buf_len, npd_omac_key3, sizeof(npd_omac_key3)*8);  //npdhash2
	free(buf);
	aesOmacMode1(buffer + 0x60, buffer, 0x60, npd_omac_key, sizeof(npd_omac_key)*8);  //npdhash3

}

void ps2_decrypt_image(u8 dex_mode, const char* image_name, const char* data_file, char* msg_update)
{
	FILE * in;
	FILE * data_out;

	u8 ps2_data_key[0x10];
	u8 ps2_meta_key[0x10];
	u8 iv[0x10];
	u8 klicensee[0x10];

	int segment_size;
	s64 data_size;
	int i;
	u8 header[256];
	u8 * data_buffer;
	u8 * meta_buffer;
	u32 read = 0;
	int num_child_segments;
	
	int64_t c, flush_size, decr_size, total_size;
	int percent;
	
	decr_size = c = percent = 0;

	if (!get_image_klicensee(image_name, klicensee))
		return;

	//open files
	in = fopen(image_name, "rb");
	data_out = fopen(data_file, "wb");

	//get file info
	read = fread(header, 256, 1, in);
	segment_size = be32(header + 0x84);
	data_size = be64(header + 0x88);
	num_child_segments = segment_size / PS2_META_ENTRY_SIZE;
	
	total_size = data_size;
	flush_size = total_size / 100;

	LOG("segment size: %x\ndata_size: %lx\n", segment_size, data_size);

	//alloc buffers
	data_buffer = malloc(segment_size*num_child_segments);
	meta_buffer = malloc(segment_size);

	//generate keys
	memset(iv, 0, 0x10);

	if(dex_mode)
	{
		LOG("[* DEX] PS2 Classic:\n");
		aes128cbc_enc(ps2_key_dex_data, iv, klicensee, 0x10, ps2_data_key);
		aes128cbc_enc(ps2_key_dex_meta, iv, klicensee, 0x10, ps2_meta_key);
	}else{
		LOG("[* CEX] PS2 Classic:\n");
		aes128cbc_enc(ps2_key_cex_data, iv, klicensee, 0x10, ps2_data_key);
		aes128cbc_enc(ps2_key_cex_meta, iv, klicensee, 0x10, ps2_meta_key);
	}

	//decrypt iso
	fseek(in, segment_size, SEEK_SET);
	
	while(data_size > 0)
	{
		//decrypt meta
		read = fread(meta_buffer, 1, segment_size, in);

		read = fread(data_buffer, 1, segment_size*num_child_segments, in);		
		for(i=0;i<num_child_segments;i++)	
			aes128cbc(ps2_data_key, iv, data_buffer+(i*segment_size), segment_size, data_buffer+(i*segment_size));
		if(data_size >= read)
			fwrite(data_buffer, read, 1, data_out);
		else
			fwrite(data_buffer, data_size, 1, data_out);
			
		c += read;
		if(c >= flush_size) {
			percent += 1;
			decr_size = decr_size + c;
			sprintf(msg_update, "Decrypted: %ld%% (%d Blocks)", (100*decr_size)/total_size, percent);
			LOG("Decrypted: %d Blocks 0x%08lx", percent, decr_size);
			c = 0;
		}
		
		data_size -= read;
	}

	//cleanup
	free(data_buffer);
	free(meta_buffer);

	fclose(in);
	fclose(data_out);
}

int64_t get_fsize(const char* fname)
{
	struct stat st;
	stat(fname, &st);
	return st.st_size;
}

void ps2_encrypt_image(u8 cfg_mode, const char* image_name, const char* data_file, char* msg_update)
{
	FILE * in;
	FILE * data_out;

	u8 ps2_data_key[0x10];
	u8 ps2_meta_key[0x10];
	u8 iv[0x10];

	u32 segment_size;
	s64 data_size;
	u32 i;
	u8 * data_buffer;
	u8 * meta_buffer;
	u8 * ps2_header;
	
	u32 read = 0;
	u32 num_child_segments = 0x200;
	u32 segment_number = 0;
	
	int64_t c, flush_size, encr_size, total_size;
	int percent;
	
	encr_size = c = percent = 0;

	//open files
	data_out = fopen(data_file, "wb");
	
	/* iso9660 check */
	if (!ps2_iso9660_sig(image_name))
		return;

	//Get file info
	segment_size = PS2_DEFAULT_SEGMENT_SIZE;
	data_size = get_fsize(image_name);
	
	total_size = data_size;
	flush_size = total_size / 100;

	/* limg section */
	if (!cfg_mode)
		ps2_build_limg(image_name, data_size);
	
	// Get new file info -- FIX FAKE SIZE VALUE on PS2 HEADER
	data_size = get_fsize(image_name);

	in = fopen(image_name, "rb");

	LOG("segment size: %x\ndata_size: %lx\nfile name: %s\nContent_id: %s\niso %s\nout file: %s\n", segment_size, data_size, cfg_mode ? PS2_CONFIG_NAME : PS2_REAL_OUT_NAME, PS2_PLACEHOLDER_CID, image_name, data_file);

	//prepare buffers
	data_buffer = malloc(segment_size * 0x200);
	meta_buffer = malloc(segment_size);
	ps2_header = malloc(segment_size);
	memset(ps2_header, 0, segment_size);

	//generate keys
	LOG("[* CEX] PS2 Classic:\n");
	memset(iv, 0, 0x10);
	aes128cbc_enc(ps2_key_cex_data, iv, ps2_placeholder_klic, 0x10, ps2_data_key);
	aes128cbc_enc(ps2_key_cex_meta, iv, ps2_placeholder_klic, 0x10, ps2_meta_key);

	//write incomplete ps2 header
	build_ps2_header(ps2_header, 2, PS2_PLACEHOLDER_CID, cfg_mode ? PS2_CONFIG_NAME : PS2_REAL_OUT_NAME, data_size);
	fwrite(ps2_header, segment_size, 1, data_out);

	//write encrypted image
	while((read = fread(data_buffer, 1, segment_size*num_child_segments, in)))
	{
		//last segments?
		if(read != (segment_size*num_child_segments)){
			num_child_segments = (read / segment_size);
			if((read % segment_size) > 0)
				num_child_segments++;
		}

		//encrypt data and create meta		
		memset(meta_buffer, 0, segment_size);
		for(i=0;i<num_child_segments;i++)
		{	
			aes128cbc_enc(ps2_data_key, iv, data_buffer+(i*segment_size), segment_size, data_buffer+(i*segment_size));
			sha1(data_buffer+(i*segment_size), segment_size, meta_buffer+(i*PS2_META_ENTRY_SIZE));
			wbe32(meta_buffer+(i*PS2_META_ENTRY_SIZE)+0x14, segment_number);
			segment_number++;
		}

		//encrypt meta
		aes128cbc_enc(ps2_meta_key, iv, meta_buffer, segment_size, meta_buffer);
		
		//write meta and data
		fwrite(meta_buffer, segment_size, 1, data_out);
		fwrite(data_buffer, segment_size, num_child_segments, data_out);
		
		c += read;
		if(msg_update && c >= flush_size) {
			percent += 1;
			encr_size = encr_size + c;
			sprintf(msg_update, "Encrypted: %ld%% (%d Blocks)", (100*encr_size)/data_size, percent);
			LOG("Encrypted: %d Blocks 0x%08lx", percent, encr_size);
			c = 0;
		}
	
		memset(data_buffer, 0, segment_size*num_child_segments);
	}

	//cleanup
	free(data_buffer);
	free(meta_buffer);
	free(ps2_header);

	fclose(in);
	fclose(data_out);

	// Restore original image file size
	truncate(image_name, total_size);
}

void ps2_crypt_vmc(u8 dex_mode, const char* vmc_path, const char* vmc_out, int crypt_mode)
{
	FILE * in;
	FILE * data_out;
	u8 ps2_vmc_key[0x10];
	u8 iv[0x10];
	u8 * data_buffer;
	u32 read = 0;

	// Validate new hashes
	if ((crypt_mode == PS2_VMC_ENCRYPT) && !vmc_hash(vmc_path))
		return;

	//open files
	in = fopen(vmc_path, "rb");
	data_out = fopen(vmc_out, "wb");

	//alloc buffers
	data_buffer = malloc(PS2_DEFAULT_SEGMENT_SIZE);

	//generate keys
	if(dex_mode)
	{
        LOG("VMC (dex)\n");
		memcpy(ps2_vmc_key, ps2_key_dex_vmc, 0x10);
	}else{
        LOG("VMC (cex)\n");
		memcpy(ps2_vmc_key, ps2_key_cex_vmc, 0x10);
	}
	
	memset(iv, 0, 0x10);

	while((read = fread(data_buffer, 1, PS2_DEFAULT_SEGMENT_SIZE, in)))
	{
		//decrypt or encrypt vmc
		if(crypt_mode == PS2_VMC_DECRYPT)
			aes128cbc(ps2_vmc_key, iv, data_buffer, read, data_buffer);
		else
			aes128cbc_enc(ps2_vmc_key, iv, data_buffer, read, data_buffer);

		fwrite(data_buffer, read, 1, data_out);
	}

	long sz = ftell(data_out);

	//cleanup
	free(data_buffer);

	fclose(in);
	fclose(data_out);
	
	// Remove hashes
	truncate((crypt_mode ? vmc_path : vmc_out), sz - PS2_DEFAULT_SEGMENT_SIZE);
}

/*
int ps2classics_main(int argc, char *argv[])
{
	{
		LOG("usage:\n\tiso:\n\t\t%s d [cex/dex] [klicensee] [encrypted image] [out data] [out meta]\n", argv[0]);
		LOG("\t\t%s e [cex/dex] [klicensee] [iso] [out data] [real out name] [CID]\n", argv[0]);
		LOG("\tvmc:\n\t\t%s vd [cex/dex] [vme file] [out vmc]\n", argv[0]);
		LOG("\t\t%s ve [cex/dex] [vmc file] [out vme]\n", argv[0]);
		exit(0);
	}
}
*/
