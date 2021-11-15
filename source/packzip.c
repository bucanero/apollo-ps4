/*
    Copyright 2004-2015 Luigi Auriemma

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
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <zlib.h>
#include "unpack.h"

#include <dbglogger.h>
#define LOG dbglogger_log


#define VER         "0.3.1"

//#define MAXZIPLEN(n) ((n)+(((n)/1000)+1)+12)
#define MAXZIPLEN(n) ((n)+(((n)/10)+1)+12)  // for lzma


uint32_t zipit(FILE *fdi, FILE *fdo, int wbits, int flags, int store);
uint32_t zlib_compress(uint8_t *in, int insz, uint8_t *out, int outsz, int wbits, int flags, int store);


int packzip_util(const char *input, const char *output, uint32_t offset, int wbits) {
    FILE    *fdi,
            *fdo;
    uint32_t len;

    LOG("PackZip " VER "\n"
        "by Luigi Auriemma\n"
        "e-mail: aluigi@autistici.org\n"
        "web:    aluigi.org");

/*
        LOG("\n"
            "Usage: %s [options] <input> <output>\n"
            "\n"
            "Options:\n"
            "-o OFF   offset of the output file where storing the raw zipped data\n"
            "-c       recreate from scratch the output file if already exists\n"
            "         keep this option in mind if you want to re-create a file because the\n"
            "         main usage of this tool is reinjecting files inside archives!\n"
            "-w BITS  windowBits value (usually -15 to -8 and 8 to 15), default is %d\n"
            "         negative for raw deflate and positive for zlib rfc1950 (78...)\n"
            "         if 0 then it will perform the LZMA compression\n"
            "-m MODE  mode:\n"
            "         zlib/deflate            LZMA\n"
            "           %d default strategy     %d normal (default)\n"
            "           %d filtered             %d 86_header\n"
            "           %d huffman only         %d 86_decoder\n"
            "           %d rle                  %d 86_dechead\n"
            "           %d fixed                %d efs\n"
            "-0       store only, no compression\n"
            "-f       use AdvanceComp compression which is faster\n"
            "\n"
            "By default this tool \"injects\" the new compressed data in the output file if\n"
            "already exists, useful for modifying archives of unknown formats replacing\n"
            "only the data that has been modified without touching the rest.\n"
            "The tool uses zopfli or AdvanceCOMP for zlib/deflate compression but switches\n"
            "to zlib if the -m option is used.\n"
            "\n", argv[0],
            wbits,
            Z_DEFAULT_STRATEGY,     LZMA_FLAGS_NONE,
            Z_FILTERED,             LZMA_FLAGS_86_HEADER,
            Z_HUFFMAN_ONLY,         LZMA_FLAGS_86_DECODER,
            Z_RLE,                  LZMA_FLAGS_86_DECODER | LZMA_FLAGS_86_HEADER,
            Z_FIXED,                LZMA_FLAGS_EFS
        );
*/

    LOG("- open input  %s", input);
    fdi = fopen(input, "rb");
    if(!fdi)
    {
        LOG("Error: can't open input file");
        return 0;
    }

    LOG("- open output %s", output);
    fdo = fopen(output, "r+b");
    if(!fdo) {
        fdo = fopen(output, "wb");
        if(!fdo)
        {
            LOG("Error: can't open output file");
            return 0;
        }
    }

    LOG("- offset        0x%08x", offset);
    LOG("- windowbits    %d", wbits);

    if(offset) {
        LOG("- seek offset");
        if(fseek(fdo, offset, SEEK_SET))
        {
            LOG("Error: can't seek to offset");
            return 0;
        }
    }

    len = zipit(fdi, fdo, wbits, Z_DEFAULT_STRATEGY, 0);

    if(fdi != stdin)  fclose(fdi);
    if(fdo != stdout) fclose(fdo);

    if(!len) {
        LOG("- the compression failed");
    } else {
        LOG("- output size   0x%08x / %u", len, len);
    }

    return(len > 0);
}


uint32_t zipit(FILE *fdi, FILE *fdo, int wbits, int flags, int store) {
    struct stat xstat;
    int     ret = 0;
    uint32_t in_size,
            out_size;
    uint8_t *in_data,
            *out_data;

    fstat(fileno(fdi), &xstat);
    in_size = xstat.st_size;
    in_data = (uint8_t *)malloc(in_size);

    in_size = fread(in_data, 1, in_size, fdi);

    out_size = MAXZIPLEN(in_size);
    out_data = (uint8_t *)malloc(out_size);

    if(!in_data || !out_data) return(0);

    LOG("- input size    0x%08x / %u", in_size, in_size);

    LOG("- compression  ");
    if(!wbits) {
        LOG(" LZMA (Not supported)");

    } else if(wbits > 0) {
        LOG(" ZLIB");
        ret = zlib_compress(in_data, in_size, out_data, out_size, wbits, flags, store);

    } else {
        LOG(" DEFLATE");
        ret = zlib_compress(in_data, in_size, out_data, out_size, wbits, flags, store);
    }

    if(ret <= 0) {
        out_size = 0;
    } else {
        out_size = ret;
    }

    if (fwrite(out_data, 1, out_size, fdo) != out_size)
    {
        LOG("Error: problems during the writing of the output file, check your free space");
        out_size = 0;
    }

    free(in_data);
    free(out_data);
    return(out_size);
}


uint32_t zlib_compress(uint8_t *in, int insz, uint8_t *out, int outsz, int wbits, int flags, int store) {
    z_stream    z;
    uint32_t    ret;
    int     zerr;

    z.zalloc = Z_NULL;
    z.zfree  = Z_NULL;
    z.opaque = Z_NULL;
    if(deflateInit2(&z, store ? Z_NO_COMPRESSION : Z_BEST_COMPRESSION, Z_DEFLATED, wbits, 9, flags)) {
        LOG("Error: zlib initialization error");
        return(-1);
    }

    z.next_in   = in;
    z.avail_in  = insz;
    z.next_out  = out;
    z.avail_out = outsz;
    zerr = deflate(&z, Z_FINISH);
    if(zerr != Z_STREAM_END) {
        LOG("Error: zlib error, %s", z.msg ? z.msg : "");
        deflateEnd(&z);
        return(-1);
    }

    ret = z.total_out;
    deflateEnd(&z);
    return(ret);
}
