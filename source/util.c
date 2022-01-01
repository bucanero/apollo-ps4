#include "util.h"
#include "types.h"

#include <polarssl/sha1.h>

void dump_data(const u8 *data, u64 size) {
	u64 i;
	for (i = 0; i < size; i++)
		dbglogger_printf("%02X", data[i]);
	dbglogger_printf("\n");
}

int get_file_size(const char *file_path, u64 *size) {
	struct stat stat_buf;

	if (!file_path || !size)
		return -1;

	if (stat(file_path, &stat_buf) < 0)
		return -1;

	*size = stat_buf.st_size;

	return 0;
}

int read_file(const char *file_path, u8 *data, u64 size) {
	FILE *fp;

	if (!file_path || !data)
		return -1;

	fp = fopen(file_path, "rb");
	if (!fp)
		return -1;

	memset(data, 0, size);

	if (fread(data, 1, size, fp) != size) {
		fclose(fp);
		return -1;
	}

	fclose(fp);

	return 0;
}

int write_file(const char *file_path, u8 *data, u64 size) {
	FILE *fp;

	if (!file_path || !data)
		return -1;

	fp = fopen(file_path, "wb");
	if (!fp)
		return -1;

	if (fwrite(data, 1, size, fp) != size) {
		fclose(fp);
		return -1;
	}

	fclose(fp);

	return 0;
}

int mmap_file(const char *file_path, u8 **data, u64 *size) {
	int fd;
	struct stat stat_buf;
	void *ptr;

	if (!file_path || !data || !size)
		return -1;

	fd = open(file_path, O_RDONLY);
	if (fd == -1)
		return -1;

	if (fstat(fd, &stat_buf) != 0) {
		close(fd);
		return -1;
	}

	ptr = malloc(stat_buf.st_size);
	if (!ptr) {
		close(fd);
		return -1;
	}

	close(fd);

	read_file(file_path, ptr, stat_buf.st_size);

	*data = (u8 *)ptr;
	*size = stat_buf.st_size;

	return 0;
}

int unmmap_file(u8 *data, u64 size) {
	if (!data || !size)
		return -1;

	free(data);
//	if (munmap(data, size) < 0)
//		return -1;

	return 0;
}

int calculate_hmac_hash(const u8 *data, u64 size, const u8 *key, u32 key_length, u8 output[20]) {
	sha1_context sha1;

	if (!key_length || !output)
		return -1;

	memset(&sha1, 0, sizeof(sha1_context));

	sha1_hmac_starts(&sha1, key, key_length);
	sha1_hmac_update(&sha1, data, size);
	sha1_hmac_finish(&sha1, output);

	memset(&sha1, 0, sizeof(sha1_context));

	return 0;
}

int calculate_file_hmac_hash(const char *file_path, const u8 *key, u32 key_length, u8 output[20]) {
	FILE *fp;
	u8 buf[512];
	sha1_context sha1;
	size_t n;

	if ((fp = fopen(file_path, "rb")) == NULL)
		return -1;

	memset(&sha1, 0, sizeof(sha1_context));

	sha1_hmac_starts(&sha1, key, key_length);
	while ((n = fread(buf, 1, sizeof(buf), fp)) > 0)
		sha1_hmac_update(&sha1, buf, n);
	sha1_hmac_finish(&sha1, output);

	memset(&sha1, 0, sizeof(sha1_context));

	if (ferror(fp) != 0) {
		fclose(fp);
		return -1;
	}

	fclose(fp);

	return 0;
}

u64 align_to_pow2(u64 offset, u64 alignment) {
	return (offset + alignment - 1) & ~(alignment - 1);
}
