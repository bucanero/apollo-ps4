#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <dirent.h>
#include <zlib.h>

#include "types.h"
#include "common.h"

#define TMP_BUFF_SIZE 0x20000

//----------------------------------------
//String Utils
//----------------------------------------
int is_char_integer(char c)
{
	if (c >= '0' && c <= '9')
		return SUCCESS;
	return FAILED;
}

int is_char_letter(char c)
{
	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
		return SUCCESS;
	return FAILED;
}

//----------------------------------------
//FILE UTILS
//----------------------------------------

int file_exists(const char *path)
{
    if (access(path, F_OK) == 0) {
    	return SUCCESS;
    }
    
	return FAILED;
}

int dir_exists(const char *path)
{
    struct stat sb;
    if ((stat(path, &sb) == 0) && S_ISDIR(sb.st_mode)) {
        return SUCCESS;
    }
    return FAILED;
}

int unlink_secure(const char *path)
{   
    if(file_exists(path)==SUCCESS)
    {
        chmod(path, 0777);
		return remove(path);
    }
    return FAILED;
}

/*
* Creates all the directories in the provided path. (can include a filename)
* (directory must end with '/')
*/
int mkdirs(const char* dir)
{
    char path[256];
    snprintf(path, sizeof(path), "%s", dir);

    char* ptr = strrchr(path, '/');
    *ptr = 0;
    ptr = path;
    ptr++;
    while (*ptr)
    {
        while (*ptr && *ptr != '/')
            ptr++;

        char last = *ptr;
        *ptr = 0;

        if (file_exists(path) == FAILED)
        {
            if (mkdir(path, 0777) < 0)
                return FAILED;
            else
                chmod(path, 0777);
        }
        
        *ptr++ = last;
        if (last == 0)
            break;

    }

    return SUCCESS;
}

int copy_file(const char* input, const char* output)
{
    size_t read, written;
    FILE *fd, *fd2;

    if (mkdirs(output) != SUCCESS)
        return FAILED;

    if((fd = fopen(input, "rb")) == NULL)
        return FAILED;

    if((fd2 = fopen(output, "wb")) == NULL)
    {
        fclose(fd);
        return FAILED;
    }

    char* buffer = malloc(TMP_BUFF_SIZE);

    if (!buffer)
        return FAILED;

    do
    {
        read = fread(buffer, 1, TMP_BUFF_SIZE, fd);
        written = fwrite(buffer, 1, read, fd2);
    }
    while ((read == written) && (read == TMP_BUFF_SIZE));

    free(buffer);
    fclose(fd);
    fclose(fd2);
    chmod(output, 0777);

    return (read - written);
}

uint32_t file_crc32(const char* input)
{
    Bytef *buffer;
    uLong crc = crc32(0L, Z_NULL, 0);
    size_t read;

    FILE* in = fopen(input, "rb");
    
    if (!in)
        return FAILED;

    buffer = malloc(TMP_BUFF_SIZE);
    do
    {
        read = fread(buffer, 1, TMP_BUFF_SIZE, in);
        crc = crc32(crc, (u8*)buffer, read);
    }
    while (read == TMP_BUFF_SIZE);

    free(buffer);
    fclose(in);

    return crc;
}

int copy_directory(const char* startdir, const char* inputdir, const char* outputdir)
{
	char fullname[256];
    char out_name[256];
	struct dirent *dirp;
	int len = strlen(startdir);
	DIR *dp = opendir(inputdir);

	if (!dp) {
		return FAILED;
	}

	while ((dirp = readdir(dp)) != NULL) {
		if ((strcmp(dirp->d_name, ".")  != 0) && (strcmp(dirp->d_name, "..") != 0)) {
  			snprintf(fullname, sizeof(fullname), "%s%s", inputdir, dirp->d_name);

  			if (dirp->d_type == DT_DIR) {
                strcat(fullname, "/");
    			copy_directory(startdir, fullname, outputdir);
  			} else {
  			    snprintf(out_name, sizeof(out_name), "%s%s", outputdir, &fullname[len]);
    			if (copy_file(fullname, out_name) != SUCCESS) {
     				return FAILED;
    			}
  			}
		}
	}
	closedir(dp);

    return SUCCESS;
}

int clean_directory(const char* inputdir)
{
	DIR *d;
	struct dirent *dir;
	char dataPath[256];

	d = opendir(inputdir);
	if (!d)
		return FAILED;

	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
		{
			snprintf(dataPath, sizeof(dataPath), "%s" "%s", inputdir, dir->d_name);

			if (dir->d_type == DT_DIR) {
				strcat(dataPath, "/");
				clean_directory(dataPath);
				rmdir(dataPath);
			}

			unlink_secure(dataPath);
		}
	}
	closedir(d);

    return SUCCESS;
}
