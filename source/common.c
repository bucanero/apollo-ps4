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
    if ((stat(path, &sb) == 0) && sb.st_mode & S_IFDIR) {
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
    char buffer[TMP_BUFF_SIZE];
    uLong crc = crc32(0L, Z_NULL, 0);
    size_t read;

    FILE* in = fopen(input, "rb");
    
    if (!in)
        return FAILED;

    do
    {
        read = fread(buffer, 1, TMP_BUFF_SIZE, in);
        crc = crc32(crc, (u8*)buffer, read);
    }
    while (read == TMP_BUFF_SIZE);

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
			unlink_secure(dataPath);
		}
	}
	closedir(d);

    return SUCCESS;
}
/*
//----------------------------------------
//POWER UTILS
//----------------------------------------

int sys_shutdown()
{   
    unlink_secure("/dev_hdd0/tmp/turnoff");
    
    lv2syscall4(379,0x1100,0,0,0);
    return_to_user_prog(int);
}

int sys_reboot()
{
    unlink_secure("/dev_hdd0/tmp/turnoff");

    lv2syscall4(379,0x1200,0,0,0);
    return_to_user_prog(int);
}

//----------------------------------------
//CONSOLE ID UTILS
//----------------------------------------

#define SYS_SS_APPLIANCE_INFO_MANAGER                  867
#define SYS_SS_GET_OPEN_PSID                           872
#define AIM_GET_DEVICE_ID                              0x19003
#define AIM_GET_OPEN_PSID                              0x19005
#define SYSCALL8_OPCODE_IS_HEN                         0x1337

int sys_ss_get_open_psid(uint64_t psid[2])
{
	lv2syscall1(SYS_SS_GET_OPEN_PSID, (uint64_t) psid);
	return_to_user_prog(int);
}

int sys_ss_appliance_info_manager(u32 packet_id, u64 arg)
{
	lv2syscall2(SYS_SS_APPLIANCE_INFO_MANAGER, (uint64_t)packet_id, (uint64_t)arg);
	return_to_user_prog(int);
}

int ss_aim_get_device_id(uint8_t *idps)
{
	return sys_ss_appliance_info_manager(AIM_GET_DEVICE_ID, (uint64_t)idps);
}

int ss_aim_get_open_psid(uint8_t *psid)
{
	return sys_ss_appliance_info_manager(AIM_GET_OPEN_PSID, (uint64_t)psid);
}

int sys8_get_hen(void)
{
    lv2syscall1(8, SYSCALL8_OPCODE_IS_HEN);
    return_to_user_prog(int);
}

int is_ps3hen(void)
{
    return (sys8_get_hen() == 0x1337);
}
*/