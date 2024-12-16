#ifndef _COMMON_H_
#define _COMMON_H_

#define SUCCESS 	0
#define FAILED	 	-1

//----------------------------------------
//String Utils
//----------------------------------------
int is_char_integer(char c);
int is_char_letter(char c);

//----------------------------------------
//FILE UTILS
//----------------------------------------

int file_exists(const char *path);
int dir_exists(const char *path);
int unlink_secure(const char *path);
int mkdirs(const char* dir);
int copy_file(const char* input, const char* output);
int copy_directory(const char* startdir, const char* inputdir, const char* outputdir);
int clean_directory(const char* inputdir);
uint32_t file_crc32(const char* input);

#endif
