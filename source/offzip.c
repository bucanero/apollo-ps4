/*
    Copyright 2004-2019 Luigi Auriemma

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

    http://www.gnu.org/licenses/gpl-2.0.txt
*/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <inttypes.h>
#include <zlib.h>
#include <dirent.h>
#include <unistd.h>

#include "unpack.h"

#include <dbglogger.h>
#define LOG dbglogger_log


typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;


#define VER             "0.3.5"
#define INSZ            0x800   // the amount of bytes we want to decompress each time
#define OUTSZ           0x10000 // the buffer used for decompressing the data
#define FBUFFSZ         0x40000 // this buffer is used for reading, faster
#define SHOWX           0x7ff   // AND to show the current scanned offset each SHOWX offsets
#define FCLOSE(X)       { if(X) fclose(X); X = NULL; }

enum {
    ZIPDOSCAN1,
    ZIPDOSCAN,
    ZIPDOWRITE,
    ZIPDODUMP,
    ZIPDOFILE,
};

#define Z_INIT_ERROR    -1000
#define Z_END_ERROR     -1001
#define Z_RESET_ERROR   -1002

#define g_minzip        32



int buffread(FILE *fd, u8 *buff, int size);
void buffseek(FILE *fd, int off, int mode);
void buffinc(int increase);
int zip_search(FILE *fd);
int unzip_all(FILE *fd, const char* out_path, int zipdo);
int unzip(FILE *fd, FILE **fdo, u32 *inlen, u32 *outlen, int zipdo, const char *dumpname);
int zlib_err(int err);
FILE *save_file(const char *fname);
int myfwrite(u8 *buff, int size, FILE *fd);


z_stream    z;
u32     g_offset        = 0,
        g_filebuffoff   = 0,
        g_filebuffsz    = 0;
int     g_zipwbits      = OFFZIP_WBITS_ZLIB;
char    *g_basename     = NULL;
u8      *g_in           = NULL,
        *g_out          = NULL,
        *g_filebuff     = NULL;


int offzip_util(const char *file_input, const char *output_dir, const char *basename, int wbits) {
    FILE    *fd,
            *fdo  = NULL;
    int     files;

    LOG("Offzip "VER"\n"
        "by Luigi Auriemma\n"
        "e-mail: aluigi@autistici.org\n"
        "web:    aluigi.org");

/*
        LOG("\n"
            "Usage: %s [options] <input> <output/dir> <offset>\n"
            "\n"
            "Options:\n"
            "-s       search for possible zip/gzip data in the input file, the scan starts\n"
            "         from the specified offset and finishs when something is found\n"
            "         the output field is ignored so you can use any name you want\n"
            "-S       as above but continues the scan (just like -a but without extraction)\n"
            "-a       unzip all the possible zip data found in the file. the output\n"
            "         directory where are unzipped the files is identified by <output>\n"
            "         all the output filenames contain the offset where they have been found\n"
            "-A       as above but without unzipping data, the output files will contain the\n"
            "         same original zipped data, just like a simple data dumper\n"
            "-1       related to -a/-A, generates one unique output file instead of many\n"
            "-m SIZE  lets you to decide the length of the zip block to check if it is a\n"
            "         valid zip data. default is %d. use a higher value to reduce the number\n"
            "         of false positive or a smaller one (eg 16) to see small zip data too\n"
            "-z NUM   this option is needed to specify a windowBits value. If you don't find\n"
            "         zip data in a file (like a classical zip file) try to set it to -15\n"
            "         valid values go from -8 to -15 and from 8 to 15. Default is 15\n"
            "-q       quiet, all the verbose error messages will be suppressed (-Q for more)\n"
            "-r       don't remove the invalid uncompressed files generated with -a and -A\n"
            "-x       visualize hexadecimal numbers\n"
            "-L FILE  dump the list of \"0xoffset zsize size\" in the specified FILE\n"
            "\n"
            "Note: offset is a decimal number or a hex number if you add a 0x before it\n"
            "      examples: 1234 or 0x4d2\n"
            "\n", argv[0], g_minzip);
*/

    g_zipwbits = wbits;
    g_basename = (char*)basename;
    g_offset        = 0;
    g_filebuffoff   = 0;
    g_filebuffsz    = 0;

    LOG("- open input file:    %s", file_input);
    fd = fopen(file_input, "rb");
    if(!fd)
    {
        LOG("Error: can't open file");
        return 0;
    }

    LOG("- zip data to check:  %d bytes", g_minzip);
    LOG("- zip windowBits:     %d", g_zipwbits);

    g_in       = malloc(INSZ);
    g_out      = malloc(OUTSZ);
    g_filebuff = malloc(FBUFFSZ);
    if(!g_in || !g_out || !g_filebuff)
    {
        LOG("Error: unable to create buffers");
        return 0;
    }

    LOG("- seek offset:        0x%08x  (%u)", g_offset, g_offset);
    buffseek(fd, g_offset, SEEK_SET);

    memset(&z, 0, sizeof(z));
    if(inflateInit2(&z, g_zipwbits) != Z_OK) 
        return zlib_err(Z_INIT_ERROR);

    LOG("+------------+-------------+-------------------------+");
    LOG("| hex_offset | blocks_dots | zip_size --> unzip_size |");
    LOG("+------------+-------------+-------------------------+");

    files = unzip_all(fd, output_dir, ZIPDOFILE);
    if(files) {
        LOG("- %u valid zip blocks found", files);
    } else {
        LOG("- no valid full zip data found");
    }

    FCLOSE(fdo);
    FCLOSE(fd);
    inflateEnd(&z);
    free(g_in);
    free(g_out);
    free(g_filebuff);
    return(files > 0);
}


int buffread(FILE *fd, u8 *buff, int size) {
    int     len,
            rest,
            ret;

    rest = g_filebuffsz - g_filebuffoff;

    ret = size;
    if(rest < size) {
        ret = size - rest;
        memmove(g_filebuff, g_filebuff + g_filebuffoff, rest);
        len = fread(g_filebuff + rest, 1, FBUFFSZ - rest, fd);
        g_filebuffoff = 0;
        g_filebuffsz  = rest + len;
        if(len < ret) {
            ret = rest + len;
        } else {
            ret = size;
        }
    }

    memcpy(buff, g_filebuff + g_filebuffoff, ret);
    return ret;
}


void buffseek(FILE *fd, int off, int mode) {
    if(fseek(fd, off, mode) < 0)
    {
        LOG("Error: buffseek");
        return;
    }
    g_filebuffoff = 0;
    g_filebuffsz  = 0;
    g_offset      = ftell(fd);
}


void buffinc(int increase) {
    g_filebuffoff += increase;
    g_offset      += increase;
}


int zip_search(FILE *fd) {
    int     len,
            zerr,
            ret;

    for(ret = - 1; (len = buffread(fd, g_in, g_minzip)) >= g_minzip; buffinc(1)) {
        z.next_in   = g_in;
        z.avail_in  = len;
        z.next_out  = g_out;
        z.avail_out = OUTSZ;

        inflateReset(&z);
        zerr = inflate(&z, Z_SYNC_FLUSH);

        if(zerr == Z_OK) {  // do not use Z_STREAM_END here! gives only troubles!!!
            LOG("Zip found at 0x%08x offset", g_offset);

            ret = 0;
            break;
        }

        if(!(g_offset & SHOWX))
            LOG("Scanned 0x%08x offset", g_offset);
    }
    return ret;
}


int unzip_all(FILE *fd, const char* out_path, int zipdo) {
    FILE    *fdo    = NULL;
    u32     inlen,
            outlen;
    int     zipres,
            extracted;
    char    filename[256]   = "";

    extracted = 0;
    zipres    = -1;

    while(!zip_search(fd)) {
        snprintf(filename, sizeof(filename), "%s[%s]%08x.dat", out_path, g_basename, g_offset);
        LOG("Unzip (0x%08x) to %s", g_offset, filename);

        zipres = unzip(fd, &fdo, &inlen, &outlen, zipdo, filename);
        FCLOSE(fdo);

        if((zipres < 0) && filename[0]) unlink(filename);

        if(!zipres) {
            LOG(" %u --> %u", inlen, outlen);

            extracted++;
        } else {
            LOG(" error");
        }

    }

    FCLOSE(fdo);
    return extracted;
}


int unzip(FILE *fd, FILE **fdo, u32 *inlen, u32 *outlen, int zipdo, const char *dumpname) {
    u32     oldsz = 0,
            oldoff,
            len;
    int     ret     = -1,
            zerr    = Z_OK;

    if ((*fdo = save_file(dumpname)) == NULL)
        return 0;

    oldoff = g_offset;
    inflateReset(&z);

    for(; (len = buffread(fd, g_in, INSZ)); buffinc(len)) {
        //if(g_quiet >= 0) fputc('.', stderr);

        z.next_in   = g_in;
        z.avail_in  = len;
        do {
            z.next_out  = g_out;
            z.avail_out = OUTSZ;
            zerr = inflate(&z, Z_SYNC_FLUSH);

            myfwrite(g_out, z.total_out - oldsz, *fdo);
            oldsz = z.total_out;

            if(zerr != Z_OK) {      // inflate() return value MUST be handled now
                if(zerr == Z_STREAM_END) {
                    ret = 0;
                } else {
                    zlib_err(zerr);
                }
                break;
            }
            ret = 0;    // it's better to return 0 even if the z stream is incomplete... or not?
        } while(z.avail_in);

        if(zerr != Z_OK) break;     // Z_STREAM_END included, for avoiding "goto"
    }

    *inlen  = z.total_in;
    *outlen = z.total_out;
    if(!ret) {
        oldoff += z.total_in;
    } else {
        oldoff++;
    }
    buffseek(fd, oldoff, SEEK_SET);
    return ret;
}


int zlib_err(int zerr) {
    switch(zerr) {
        case Z_DATA_ERROR: {
            LOG("- zlib Z_DATA_ERROR, the data in the file is not in zip format"
                "  or uses a different windowBits value (-z). Try to use -z %d",
                -g_zipwbits);
            break;
        }
        case Z_NEED_DICT: {
            LOG("- zlib Z_NEED_DICT, you need to set a dictionary (option not available)");
            break;
        }
        case Z_MEM_ERROR: {
            LOG("- zlib Z_MEM_ERROR, insufficient memory");
            break;
        }
        case Z_BUF_ERROR: {
            LOG("- zlib Z_BUF_ERROR, the output buffer for zlib decompression is not enough");
            break;
        }
        case Z_INIT_ERROR: {
            LOG("Error: zlib initialization error (inflateInit2)");
            break;
        }
        case Z_END_ERROR: {
            LOG("Error: zlib free error (inflateEnd)");
            break;
        }
        case Z_RESET_ERROR: {
            LOG("Error: zlib reset error (inflateReset)");
            break;
        }
        default: {
            LOG("Error: zlib unknown error %d", zerr);
            break;
        }
    }
    return 0;
}


FILE *save_file(const char *fname) {
    FILE    *fd;

    fd = fopen(fname, "wb");
    if(!fd)
    {
        LOG("Error: can't open file");
        return NULL;
    }
    return fd;
}


int myfwrite(u8 *buff, int size, FILE *fd) {
    if(!fd) {
        LOG("Error: myfw fd is NULL, contact me.");
        return(0);
    }
    if(size <= 0) return 1;
    if(fwrite(buff, 1, size, fd) != size) {
        LOG("Error: problems during files writing, check permissions and disk space");
        return(0);
    }
    return 1;
}
