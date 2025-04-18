#ifndef _UTIL_H_
#define _UTIL_H_

#include <apollo.h>
#include "types.h"

#define NOTIFICATION_ICON_BAN           "cxml://psnotification/tex_icon_ban"
#define NOTIFICATION_ICON_DEFAULT       "cxml://psnotification/tex_default_icon_notification"

#define ALIGN(_value, _alignment) (((_value) + (_alignment) - 1) & ~((_alignment) - 1))

// ---------------------------------------------------------------------
// https://www.psdevwiki.com/ps3/Keys#TMDB_Key
// ---------------------------------------------------------------------
static const unsigned char TMDB_HMAC_Key[64] = {
	0xF5, 0xDE, 0x66, 0xD2, 0x68, 0x0E, 0x25, 0x5B, 0x2D, 0xF7, 0x9E, 0x74, 0xF8, 0x90, 0xEB, 0xF3,
	0x49, 0x26, 0x2F, 0x61, 0x8B, 0xCA, 0xE2, 0xA9, 0xAC, 0xCD, 0xEE, 0x51, 0x56, 0xCE, 0x8D, 0xF2,
	0xCD, 0xF2, 0xD4, 0x8C, 0x71, 0x17, 0x3C, 0xDC, 0x25, 0x94, 0x46, 0x5B, 0x87, 0x40, 0x5D, 0x19,
	0x7C, 0xF1, 0xAE, 0xD3, 0xB7, 0xE9, 0x67, 0x1E, 0xEB, 0x56, 0xCA, 0x67, 0x53, 0xC2, 0xE6, 0xB0
};

void dump_data(const u8 *data, u64 size);
void get_psv_filename(char* psvName, const char* path, const char* dirName);

int get_file_size(const char *file_path, u64 *size);
int read_file(const char *file_path, u8 *data, u64 size);
int write_file(const char *file_path, u8 *data, u64 size);
int mmap_file(const char *file_path, u8 **data, u64 *size);
int unmmap_file(u8 *data, u64 size);

int calculate_hmac_hash(const u8 *data, u64 size, const u8 *key, u32 key_length, u8 output[20]);
int calculate_file_hmac_hash(const char *file_path, const u8 *key, u32 key_length, u8 output[20]);

void append_le_uint16(uint8_t *buf, uint16_t val);
void append_le_uint32(uint8_t *buf, uint32_t val);
void append_le_uint64(uint8_t *buf, uint64_t val);
uint16_t read_le_uint16(const uint8_t *buf);
uint32_t read_le_uint32(const uint8_t *buf);
uint64_t read_le_uint64(const uint8_t *buf);

u64 align_to_pow2(u64 offset, u64 alignment);
void notify_popup(const char *p_Uri, const char *p_Format, ...);

#endif /* !_UTIL_H_ */
