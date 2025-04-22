#ifndef _UTIL_H_
#define _UTIL_H_

#include <apollo.h>
#include "types.h"

#define NOTIFICATION_ICON_BAN           "cxml://psnotification/tex_icon_ban"
#define NOTIFICATION_ICON_DEFAULT       "cxml://psnotification/tex_default_icon_notification"

#define ALIGN(_value, _alignment) (((_value) + (_alignment) - 1) & ~((_alignment) - 1))

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
