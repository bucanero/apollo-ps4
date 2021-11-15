/**********************************************************************
 *
 * Filename:    crc.h
 * 
 * Description: Slow and fast implementations of the CRC standards.
 *
 * 
 * Copyright (c) 2000 by Michael Barr.  This software is placed into
 * the public domain and may be used for any purpose.  However, this
 * notice must not be changed or removed and no warranty is either
 * expressed or implied by its publication or distribution.
 **********************************************************************/

#ifndef CRC_H_INCLUDED
#define CRC_H_INCLUDED


#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct
{
    uint8_t width;
    uint64_t poly;
    uint64_t init;
    uint64_t xor;
    uint8_t refIn;
    uint8_t refOut;
} custom_crc_t;

#define MOD_ADLER_16                        251
#define MOD_ADLER_32                        65521

/* ---------- Defines for 16-bit CRC/XMODEM calculation (Not reflected) --------------------------------------------------------- */
#define CRC_16_RESULT_WIDTH                 16u
#define CRC_16_POLYNOMIAL                   0x1021u
#define CRC_16_INIT_VALUE                   0x0000u
#define CRC_16_XOR_VALUE                    0x0000u

// 16-bit CCITT
#define CRC_16_INIT_CCITT                   0xFFFFu

/* ---------- Defines for 32-bit CRC/CCITT calculation (Reflected) -------------------------------------------------------------- */
#define CRC_32_RESULT_WIDTH                 32u
#define CRC_32_POLYNOMIAL                   0x04C11DB7u
#define CRC_32_INIT_VALUE                   0xFFFFFFFFu
#define CRC_32_XOR_VALUE                    0xFFFFFFFFu

// Reversed polynomial
#define CRC_32_REVERSED_POLY                0xEDB88320u

/* ---------- Defines for 64-bit CRC calculation -------------------------------------------------------------- */
#define CRC_64_RESULT_WIDTH                 64u
#define CRC_64_ECMA182_POLY                 0x42F0E1EBA9EA3693
#define CRC_64_ECMA182_INIT_VALUE           0x0000000000000000
#define CRC_64_ECMA182_XOR_VALUE            0x0000000000000000

// 64-bit GO-ISO
#define CRC_64_ISO_POLY                     0x000000000000001B
#define CRC_64_ISO_INIT_VALUE               0xFFFFFFFFFFFFFFFF
#define CRC_64_ISO_XOR_VALUE                0xFFFFFFFFFFFFFFFF

/**
 * This function makes a CRC16 calculation on Length data bytes
 *
 * RETURN VALUE: 16 bit result of CRC calculation
 */
uint16_t crc16_hash(const uint8_t* message, uint32_t nBytes, custom_crc_t* cfg);

/**
 * This function makes a CRC32 calculation on Length data bytes
 *
 * RETURN VALUE: 32 bit result of CRC calculation
 */
uint32_t crc32_hash(const uint8_t* message, uint32_t nBytes, custom_crc_t* cfg);

/**
 * This function makes a CRC64 calculation on Length data bytes
 *
 * RETURN VALUE: 64 bit result of CRC calculation
 */
uint64_t crc64_hash(const uint8_t *data, uint32_t len, custom_crc_t* cfg);

/**
 * This function makes a "MC02" Electronic Arts hash calculation on Length data bytes
 *
 * RETURN VALUE: 32 bit result of CRC calculation
 */
uint32_t MC02_hash(const uint8_t *data, uint32_t len);

/**
 * This function makes a SDBM hash calculation on Length data bytes
 *
 * RETURN VALUE: 32 bit result of CRC calculation
 */
uint32_t sdbm_hash(const uint8_t* data, uint32_t len, uint32_t init);

/**
 * This function makes a FNV-1 hash calculation on Length data bytes
 *
 * RETURN VALUE: 32 bit result of CRC calculation
 */
int fnv1_hash(const uint8_t* data, uint32_t len, int init);

/**
 * This function makes a Checksum32 calculation on Length data bytes
 *
 * RETURN VALUE: 32 bit result of CRC calculation
 */
int Checksum32_hash(const uint8_t* data, uint32_t len);

/**
 * This function makes Adler16 hash calculation on Length data bytes
 *
 * RETURN VALUE: 16 bit result of CRC calculation
 */
uint16_t adler16(const uint8_t *data, size_t len);

/**
 * This function makes Final Fantasy X hash calculation on Length data bytes
 *
 * RETURN VALUE: 16 bit result of CRC calculation
 */
uint16_t ffx_hash(const uint8_t* data, uint32_t len);

/**
 * This function makes Kingdom Hearts 2.5 hash calculation on Length data bytes
 *
 * RETURN VALUE: 32 bit result of CRC calculation
 */
uint32_t kh25_hash(const uint8_t* data, uint32_t len);

/**
 * This function makes Kingdom Hearts Chain of Memories hash calculation on Length data bytes
 *
 * RETURN VALUE: 32 bit result of CRC calculation
 */
uint32_t kh_com_hash(const uint8_t* data, uint32_t len);

/**
 * This function makes DuckTales hash calculation on Length data bytes
 *
 * RETURN VALUE: 64 bit result of CRC calculation
 */
uint64_t duckTales_hash(const uint8_t* data, uint32_t len);

/**
 * This function makes Samurai Warriors 4 hash calculation on Length data bytes
 *
 * RETURN VALUE: 32 bit result array of CRC calculation
 */
int sw4_hash(const uint8_t* data, uint32_t size, uint32_t* crcs);

/**
 * This function makes MGS2 hash calculation on Length data bytes
 *
 * RETURN VALUE: 32 bit result of CRC calculation
 */
int mgs2_hash(const uint8_t* data, uint32_t len);

/**
 * This function makes Tears to Tiara 2 hash calculation on Length data bytes
 *
 * RETURN VALUE: 32 bit result of CRC calculation
 */
uint32_t tiara2_hash(const uint8_t* data, uint32_t len);

/**
 * This function makes Tales of Zestiria hash calculation on Length data bytes
 *
 * RETURN VALUE: 20 byte result array of SHA1 calculation
 */
void toz_hash(const uint8_t* data, uint32_t len, uint8_t* sha_hash);

#ifdef __cplusplus
}
#endif


#endif // CRC_H_INCLUDED
