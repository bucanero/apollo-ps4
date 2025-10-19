#ifndef _COMMON_H_
#define _COMMON_H_

#define SUCCESS 	0
#define FAILED	 	-1

//----------------------------------------
//String Utils
//----------------------------------------
int is_char_integer(char c);
int is_char_letter(char c);
char * rstrip(char *s);
char * lskip(const char *s);
char * safe_strncpy(char *dst, const char* src, size_t size);
const char * get_user_language(void);

//----------------------------------------
//FILE UTILS
//----------------------------------------

int file_exists(const char *path);
int dir_exists(const char *path);
int unlink_secure(const char *path);
int mkdirs(const char* dir);
int copy_file(const char* input, const char* output);
int copy_directory(const char* startdir, const char* inputdir, const char* outputdir);
int clean_directory(const char* inputdir, const char* filter);
uint32_t file_crc32(const char* input);

#endif
