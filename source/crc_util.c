/**********************************************************************
 *
 * Filename:    crc.c
 * 
 * Description: Slow and fast implementations of the CRC standards.
 *
 * Notes:       The parameters for each supported CRC standard are
 *				defined in the header file crc.h.  The implementations
 *				here should stand up to further additions to that list.
 *
 * 
 * Copyright (c) 2000 by Michael Barr.  This software is placed into
 * the public domain and may be used for any purpose.  However, this
 * notice must not be changed or removed and no warranty is either
 * expressed or implied by its publication or distribution.
 **********************************************************************/

#include <stdio.h>
#include <polarssl/sha1.h>
#include "crc_util.h"

#define SW4_OFF_1      0x00004
#define SW4_OFF_2      0x000A8
#define SW4_OFF_3      0x00C16
#define SW4_OFF_JP     0x58298

#define CREATE_CRC_FUNCTION(UINT, CRC_WIDTH) \
    UINT crc##CRC_WIDTH##_hash (const uint8_t* data, uint32_t len, custom_crc_t* cfg) \
    { \
        UINT crc = (UINT)cfg->init; \
        /* Perform modulo-2 division, a byte at a time. */ \
        while (len--) { \
            /* Bring the next byte into the remainder. */ \
            crc ^= ((UINT)(cfg->refIn ? (uint8_t)reflect(*data++, 8) : *data++) << (CRC_WIDTH - 8)); \
            /* Perform modulo-2 division, a bit at a time. */ \
            for (uint8_t bit = 8; bit > 0; --bit) \
                /* Try to divide the current data bit. */ \
                crc = (crc & ((UINT)1 << (CRC_WIDTH - 1))) ? (crc << 1) ^ (UINT)cfg->poly : (crc << 1); \
        } \
        /* The final remainder is the CRC result. */ \
        if (cfg->refOut) \
            crc = (UINT)reflect(crc, CRC_WIDTH); \
        return (crc ^ (UINT)cfg->xor); \
    }

void generate_crc16_table(uint16_t poly, uint16_t* crc_table)
{
    for (int i = 0; i < 256; ++i)
    {
        uint16_t r = i << 8;
        
        for (int j = 0; j < 8; ++j)
            r = (r & 0x8000) ? (r << 1) ^ poly : (r << 1);
        
        crc_table[i] = r;
    }
}

void generate_crc32_table(uint32_t poly, uint32_t* crc_table)
{
    for (int i = 0; i < 256; ++i)
    {
        uint32_t r = i << 24;
        
        for (int j = 0; j < 8; ++j)
            r = (r & 0x80000000) ? (r << 1) ^ poly : (r << 1);
        
        crc_table[i] = r;
    }
}

uint32_t add_csum(const uint8_t* data, uint32_t len)
{
    uint32_t checksum = 0;

    while (len--)
        checksum += *data++;

    return checksum;
}

// Custom CRC table for Kingdom Hearts 2.5
void kh25_crc32_table(uint32_t poly, uint32_t* crc_table)
{
    for (int x = 0; x < 0x100; x++)
    {
        int r = x << 24;
        for (int j = 0; j < 0xff; j++)
            r = (r << 1) ^ (r < 0 ? poly : 0);

        crc_table[x] = r;
    }
}

uint32_t kh25_hash(const uint8_t* data, uint32_t len)
{
    uint32_t crc32_table[256];
    uint32_t crc = CRC_32_INIT_VALUE;

    kh25_crc32_table(CRC_32_POLYNOMIAL, crc32_table);

    for (int i = 0; i < 8; i++, len--)
        crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *data++) & 0xFF];

    // skip crc bytes (0x8..0xB)
    len -= 4;
    data += 4;

    while (len--)
        crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *data++) & 0xFF];

    return (~crc);
}

uint32_t kh_com_hash(const uint8_t* data, uint32_t len)
{
    int csum = CRC_32_INIT_VALUE;

    while (len--)
    {
        csum ^= (*data++ << 31);
        csum = (csum << 1) ^ (csum < 0 ? CRC_32_POLYNOMIAL : 0);
    }

    return (~csum);
}

// "MC02" Electronic Arts hash table
// https://gist.github.com/Experiment5X/5025310 / https://ideone.com/cy2rM7
// I have no clue how this works and understand absolutely none of the math behind it.
// I just reversed it from Dead Space 3.
uint32_t MC02_hash(const uint8_t *pb, uint32_t cb)
{
	uint32_t MC02_table[0x100];
	generate_crc32_table(CRC_32_POLYNOMIAL, MC02_table);

	if (cb < 4)
		return 0;

	uint32_t rotatedThird = (pb[2] << 8) | (pb[2] >> 24);
	uint32_t ORedFirstPair = ((pb[0] << 24) | (pb[0] >> 8)) | ((pb[1] << 16) | (pb[1] >> 16));

	uint32_t seedValue = ~((rotatedThird | ORedFirstPair) | pb[3]);
	pb += 4;
	cb -= 4;

	for (uint32_t i = 0; i < cb; i++)
	{
		uint32_t lookedUpValue = MC02_table[((seedValue >> 22) & 0x3FC) >> 2];
		uint32_t insertedNum = pb[i] | (seedValue << 8);
		seedValue = lookedUpValue ^ insertedNum;
	}

	return ~seedValue;
}

// http://www.cse.yorku.ca/~oz/hash.html#sdbm
uint32_t sdbm_hash(const uint8_t* data, uint32_t len, uint32_t init)
{
    uint32_t crc = init;
    
    while (len--)
        crc = (crc * 0x1003f) + *data++;

    return (crc);
}

// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
int fnv1_hash(const uint8_t* data, uint32_t size, int init)
{
    int sum = init;

    while (size--)
        sum = (sum * 0x1000193) ^ *data++;

    return (sum);
}

int Checksum32_hash(const uint8_t* data, uint32_t size)
{
    int sum = 0;

    while (size--)
        sum += (signed char) *data++;

    return sum;
}

uint16_t ffx_hash(const uint8_t* data, uint32_t len)
{
    uint16_t crc16_table[0x100];
    uint16_t crc = CRC_16_INIT_CCITT;

    // FFX-2 & FFX (or PS2 Bug?) - Table Last Entry Should be 0x1EF0, but is 0x0000
    generate_crc16_table(CRC_16_POLYNOMIAL, crc16_table);
    crc16_table[0xFF] = 0;

    while (len--)
        crc = (crc << 8) ^ crc16_table[((crc >> 8) ^ *data++) & 0xFF];

    return (~crc);
}

uint64_t duckTales_hash(const uint8_t* data, uint32_t size)
{
    uint32_t r1, r2, r3, r4, r5, r6, r7, r8;
    uint32_t x0, x1;
    int offset = 0;

    r4 = (uint32_t) size + 0xDEADC3C1;
    r6 = r4 + 0x162E;
    r5 = r4;

    for (int i = size / 12; i > 0; --i)
    {
        r7 =  (uint32_t) (data[offset]   + (data[offset+1] << 8) + (data[offset+2] << 16)  + (data[offset+3] << 24));
        r4 += (uint32_t) (data[offset+4] + (data[offset+5] << 8) + (data[offset+6] << 16)  + (data[offset+7] << 24));
        r6 += (uint32_t) (data[offset+8] + (data[offset+9] << 8) + (data[offset+10] << 16) + (data[offset+11] << 24));
        offset += 12;

        r5 = (r5 + r7 - r6) ^ ((r6 << 4) | (r6 >> 28));
        r2 = r4 - r5;
        r4 += r6;
        r6 = r2 ^ ((r5 << 6) | (r5 >> 26));
        r5 += r4;
        r3 = r4 - r6;
        r4 = r6 + r5;
        r6 = r3 ^ ((r6 << 8) | (r6 >> 24));
        r3 = r4 + r6;
        r6 = (r5 - r6) ^ ((r6 << 16) | (r6 >> 16));
        r5 = r3 + r6;
        r6 = (r4 - r6) ^ ((r6 >> 13) | (r6 << 19));
        r4 = r5 + r6;
        r6 = (r3 - r6) ^ ((r6 <<  4) | (r6 >> 28));
    }

    for (int i = 0; i < 4; ++i)
        r5 += (uint32_t) data[offset++] << (i * 8);

    r3 = (r6 ^ r4) - ((r4 << 14) | (r4 >> 18));
    r1 = (r5 ^ r3) - ((r3 << 11) | (r3 >> 21));
    r4 = (r4 ^ r1) - ((r1 >>  7) | (r1 << 25));
    r6 = (r3 ^ r4) - ((r4 >> 16) | (r4 << 16));
    r8 = (r1 ^ r6) - ((r6 <<  4) | (r6 >> 28));
    r5 = (r4 ^ r8) - ((r8 << 14) | (r8 >> 18));
    x1 = (r6 ^ r5) - ((r5 >> 8) | (r5 << 24));
    x0 = r5;

    return ((uint64_t) x1 << 32) | x0;
}

int sw4_hash(const uint8_t* data, uint32_t size, uint32_t* crcs)
{
	uint32_t num1, num2, num3, num4, num5, num6;
	uint8_t is_jp = (*(uint32_t*)(data + SW4_OFF_JP) != 0);

	num2 = add_csum(data + SW4_OFF_1 + 4, SW4_OFF_2 - (SW4_OFF_1 + 4));
	num3 = add_csum(data + SW4_OFF_2 + 4, 2284 - (SW4_OFF_2 + 4));

	if (is_jp)
	{
		num1 = add_csum(data + SW4_OFF_3 + 4, 30294 - (SW4_OFF_3 + 4));
		num4 = add_csum(data + 33630, 361106 - 33630);
		num5 = add_csum(data + 30294, 33631 - 30294);
		num6 = add_csum(data + SW4_OFF_1 + 4, 30294 - (SW4_OFF_1 + 4));
	}
	else
	{
		num1 = add_csum(data + SW4_OFF_3 + 4, 30934 - (SW4_OFF_3 + 4));
		num4 = add_csum(data + 34270, 361778 - 34270);
		num5 = add_csum(data + 30934, 34271 - 30934);
		num6 = add_csum(data + SW4_OFF_1 + 4, 30934 - (SW4_OFF_1 + 4));
	}

	crcs[0] = 0x7FFFFFFF & (num5 + (num1 + num2) * num3);
	crcs[1] = 0x7FFFFFFF & (num2 * (num1 + num3 + num4));
    crcs[2] = num6;
	crcs[3] = 0x7FFFFFFF & (crcs[0] + crcs[1] + num6);

    return is_jp;
}

int mgs2_hash(const uint8_t* data, uint32_t size)
{
    int num, crc = -1;

    while (size--)
    {
        crc = crc ^ *data++ << 24 >> 24;
        for (int j = 0; j < 8; j++)
        {
            num = crc & 1;
            crc = crc >> 1 & 0x7fffffff;

            if (num != 0)
                crc ^= CRC_32_REVERSED_POLY;
        }
    }

    return ~crc;
}

uint32_t tiara2_hash(const uint8_t* data, uint32_t len)
{
	uint32_t crc = 1;
	uint32_t add = 0x3428;

	while (len--)
	{
		add += *data++;
		crc = (crc * add) + add;
	}

	return (crc);
}

void _toz_sha1(const uint8_t* data, uint32_t length, const char* key, uint8_t* hash_out)
{
    sha1_context ctx;

    sha1_starts(&ctx);
    sha1_update(&ctx, data, length);
    sha1_update(&ctx, (const unsigned char*)key, strlen(key));
    sha1_finish(&ctx, hash_out);
}

// https://github.com/bucanero/ps3-save-decrypters/blob/master/toz-checksum-fixer/samples/Crypto.txt
void toz_hash(const uint8_t* data, uint32_t len, uint8_t* hash)
{
    const char array[8][4] = {"SRA", "ROS", "MIC", "LAI", "EDN", "DEZ", "ZAB", "ALI"};

    _toz_sha1(data, len, "TO12", hash);

    for (int i = 0; i < 100; i++)
        _toz_sha1(hash, 20, array[i % 8], hash);

    return;
}

uint16_t adler16(const uint8_t *data, size_t len)
/* 
    where data is the location of the data in physical memory and 
    len is the length of the data in bytes 
*/
{
    uint32_t a = 1, b = 0;

    // Process each byte of the data in order
    while (len--)
    {
        a = (a + *data++) % MOD_ADLER_16;
        b = (b + a) % MOD_ADLER_16;
    }

    return ((b << 8) | a);
}

uint64_t reflect(uint64_t data, uint8_t nBits)
{
    uint64_t reflection = 0;
    uint8_t bit;
    /*
     * Reflect the data about the center bit.
     */
    for (bit = 0; bit < nBits; ++bit)
    {
        /*
         * If the LSB bit is set, set the reflection of it.
         */
        if (data & 0x01)
            reflection |= ((uint64_t)1 << ((nBits - 1) - bit));

        data = (data >> 1);
    }

    return (reflection);

}   /* reflect() */

/* crc16_hash() */
CREATE_CRC_FUNCTION(uint16_t, 16)

/* crc32_hash() */
CREATE_CRC_FUNCTION(uint32_t, 32)

/* crc64_hash() */
CREATE_CRC_FUNCTION(uint64_t, 64)
