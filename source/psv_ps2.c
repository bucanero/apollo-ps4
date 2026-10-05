/*
* original .MAX, .CBS, .PSU file decoding from Cheat Device PS2 by root670
* https://github.com/root670/CheatDevicePS2
*/

#include <stdio.h>
#include <stddef.h>
#include <zlib.h>
#include <mbedtls/arc4.h>

#include "util.h"
#include "lzari.h"
#include "ps2mc.h"

#define  MAX_HEADER_MAGIC   "Ps2PowerSave"
#define  CBS_HEADER_MAGIC   "CFU\0"
#define  XPS_HEADER_MAGIC   "SharkPortSave\0\0\0"

#define  xps_mode_swap(M)   ((M & 0x00FF) << 8) + ((M & 0xFF00) >> 8)

// This is the initial permutation state ("S") for the RC4 stream cipher
// algorithm used to encrypt and decrypt Codebreaker saves.
// Source: https://github.com/ps2dev/mymc/blob/master/ps2save.py#L36
static const uint8_t cbsKey[256] = {
    0x5f, 0x1f, 0x85, 0x6f, 0x31, 0xaa, 0x3b, 0x18,
    0x21, 0xb9, 0xce, 0x1c, 0x07, 0x4c, 0x9c, 0xb4,
    0x81, 0xb8, 0xef, 0x98, 0x59, 0xae, 0xf9, 0x26,
    0xe3, 0x80, 0xa3, 0x29, 0x2d, 0x73, 0x51, 0x62,
    0x7c, 0x64, 0x46, 0xf4, 0x34, 0x1a, 0xf6, 0xe1,
    0xba, 0x3a, 0x0d, 0x82, 0x79, 0x0a, 0x5c, 0x16,
    0x71, 0x49, 0x8e, 0xac, 0x8c, 0x9f, 0x35, 0x19,
    0x45, 0x94, 0x3f, 0x56, 0x0c, 0x91, 0x00, 0x0b,
    0xd7, 0xb0, 0xdd, 0x39, 0x66, 0xa1, 0x76, 0x52,
    0x13, 0x57, 0xf3, 0xbb, 0x4e, 0xe5, 0xdc, 0xf0,
    0x65, 0x84, 0xb2, 0xd6, 0xdf, 0x15, 0x3c, 0x63,
    0x1d, 0x89, 0x14, 0xbd, 0xd2, 0x36, 0xfe, 0xb1,
    0xca, 0x8b, 0xa4, 0xc6, 0x9e, 0x67, 0x47, 0x37,
    0x42, 0x6d, 0x6a, 0x03, 0x92, 0x70, 0x05, 0x7d,
    0x96, 0x2f, 0x40, 0x90, 0xc4, 0xf1, 0x3e, 0x3d,
    0x01, 0xf7, 0x68, 0x1e, 0xc3, 0xfc, 0x72, 0xb5,
    0x54, 0xcf, 0xe7, 0x41, 0xe4, 0x4d, 0x83, 0x55,
    0x12, 0x22, 0x09, 0x78, 0xfa, 0xde, 0xa7, 0x06,
    0x08, 0x23, 0xbf, 0x0f, 0xcc, 0xc1, 0x97, 0x61,
    0xc5, 0x4a, 0xe6, 0xa0, 0x11, 0xc2, 0xea, 0x74,
    0x02, 0x87, 0xd5, 0xd1, 0x9d, 0xb7, 0x7e, 0x38,
    0x60, 0x53, 0x95, 0x8d, 0x25, 0x77, 0x10, 0x5e,
    0x9b, 0x7f, 0xd8, 0x6e, 0xda, 0xa2, 0x2e, 0x20,
    0x4f, 0xcd, 0x8f, 0xcb, 0xbe, 0x5a, 0xe0, 0xed,
    0x2c, 0x9a, 0xd4, 0xe2, 0xaf, 0xd0, 0xa9, 0xe8,
    0xad, 0x7a, 0xbc, 0xa8, 0xf2, 0xee, 0xeb, 0xf5,
    0xa6, 0x99, 0x28, 0x24, 0x6c, 0x2b, 0x75, 0x5d,
    0xf8, 0xd3, 0x86, 0x17, 0xfb, 0xc0, 0x7b, 0xb3,
    0x58, 0xdb, 0xc7, 0x4b, 0xff, 0x04, 0x50, 0xe9,
    0x88, 0x69, 0xc9, 0x2a, 0xab, 0xfd, 0x5b, 0x1b,
    0x8a, 0xd9, 0xec, 0x27, 0x44, 0x0e, 0x33, 0xc8,
    0x6b, 0x93, 0x32, 0x48, 0xb6, 0x30, 0x43, 0xa5
}; 

void write_psv_header(FILE *fp, uint32_t type);

static void printMAXHeader(const maxHeader_t *header)
{
    if(!header)
        return;

    LOG("Magic            : %.*s", (int)sizeof(header->magic), header->magic);
    LOG("CRC              : %08X", (header->crc));
    LOG("dirName          : %.*s", (int)sizeof(header->dirName), header->dirName);
    LOG("iconSysName      : %.*s", (int)sizeof(header->iconSysName), header->iconSysName);
    LOG("compressedSize   : %u", (header->compressedSize));
    LOG("numFiles         : %u", (header->numFiles));
    LOG("decompressedSize : %u", (header->decompressedSize));
}

/*
 * A .max carries a CRC-32 of the whole file, computed with its own four header
 * bytes taken as zero. Nothing else in the container can catch a corrupted
 * byte: the LZARI stream has no integrity check of its own, so a flipped bit
 * silently becomes a flipped bit in the save. A stored zero means the writer
 * left the field alone, which the original Action Replay software does.
 */
static int maxChecksumOk(FILE *f, u32 stored)
{
    const long at = (long) offsetof(maxHeader_t, crc);
    u8 buf[4096];
    uLong crc = crc32(0L, Z_NULL, 0);
    long pos = 0;
    size_t n, i;

    if(stored == 0)
        return 1;

    if(fseek(f, 0, SEEK_SET) != 0)
        return 0;

    while((n = fread(buf, 1, sizeof(buf), f)) > 0)
    {
        for(i = 0; i < n; i++)
            if(pos + (long) i >= at && pos + (long) i < at + 4)
                buf[i] = 0;

        crc = crc32(crc, buf, (uInt) n);
        pos += (long) n;
    }

    return (u32) crc == stored;
}

/*
 * Entries are 16-byte aligned, counting from 8 bytes before the buffer: the
 * next one begins at roundUp(offset + 8, 16) - 8. Kept in u32 the whole way -
 * the arithmetic used to run through a roundUp(int, int) helper, and
 * offset + length + 8 can exceed INT_MAX on a large save, where the narrowing
 * that followed could wrap negative.
 */
static u32 nextEntryOffset(u32 offset)
{
    return offset + (16 - ((offset + 8) % 16)) % 16;
}

/*
 * Walk the entry chain and check every header and every file's data lies
 * inside what unlzari() actually produced.
 *
 * unlzari() stops when its input runs out and reports how much it wrote, so a
 * truncated or corrupt stream still "succeeds" - it just returns a short
 * buffer. The loops below index the buffer using each entry's declared length
 * and never look at that figure, so without this check a short decode is
 * written out as a save whose last file ends in zeros, and a malformed one
 * reads off the end of the allocation entirely. Refuse both.
 */
static int maxEntriesFit(const u8 *buf, u32 len, u32 numFiles)
{
    const maxEntry_t *e;
    u32 offset = 0, i;

    for(i = 0; i < numFiles; i++)
    {
        u32 next, length;

        /* offset <= len is an invariant of the loop, so len - offset is safe */
        if(len - offset < sizeof(maxEntry_t))
            return 0;

        e = (const maxEntry_t*) &buf[offset];
        length = e->length;
        offset += sizeof(maxEntry_t);

        if(length > len - offset)
            return 0;

        offset += length;

        /* Padding that runs past the data is only meaningful after the last
         * entry; clamping keeps offset <= len, and a further entry then fails
         * the size check at the top. */
        next = nextEntryOffset(offset);
        offset = (next > len) ? len : next;
    }
    return 1;
}

static int isMAXFile(const char *path)
{
    if(!path)
        return 0;

    FILE *f = fopen(path, "rb");
    if(!f)
        return 0;

    // Verify file size
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(len < sizeof(maxHeader_t))
    {
        fclose(f);
        return 0;
    }

    // Verify header
    maxHeader_t header;
    fread(&header, 1, sizeof(maxHeader_t), f);
    fclose(f);

    printMAXHeader(&header);

    return (header.compressedSize > 0) &&
           (header.decompressedSize > 0) &&
           (header.numFiles > 0) &&
           strncmp(header.magic, MAX_HEADER_MAGIC, sizeof(header.magic)) == 0 &&
           strlen(header.dirName) > 0;
}

static void setMcDateTime(sceMcStDateTime* mc, struct tm *ftm)
{
    mc->Resv2 = 0;
    mc->Sec = ftm->tm_sec;
    mc->Min = ftm->tm_min;
    mc->Hour = ftm->tm_hour;
    mc->Day = ftm->tm_mday;
    mc->Month = ftm->tm_mon + 1;
    mc->Year = (ftm->tm_year + 1900);
}

static void set_ps2header_values(ps2_header_t *ps2h, const ps2_FileInfo_t *ps2fi, const ps2_IconSys_t *ps2sys)
{
    if (strcmp(ps2fi->filename, ps2sys->IconName) == 0)
    {
        ps2h->icon1Size = ps2fi->filesize;
        ps2h->icon1Pos = ps2fi->positionInFile;
    }

    if (strcmp(ps2fi->filename, ps2sys->copyIconName) == 0)
    {
        ps2h->icon2Size = ps2fi->filesize;
        ps2h->icon2Pos = ps2fi->positionInFile;
    }

    if (strcmp(ps2fi->filename, ps2sys->deleteIconName) == 0)
    {
        ps2h->icon3Size = ps2fi->filesize;
        ps2h->icon3Pos = ps2fi->positionInFile;
    }

    if(strcmp(ps2fi->filename, "icon.sys") == 0)
    {
        ps2h->sysSize = ps2fi->filesize;
        ps2h->sysPos = ps2fi->positionInFile;
    }
}

int ps2_max2psv(const char *save, const char* psv_path)
{
    struct stat st;
    sceMcStDateTime fctime, fmtime;
    maxHeader_t header;
    FILE *f, *psv;

    if (!isMAXFile(save))
    {
        LOG("ERROR! Not a valid AR Max save: %s", save);
        return 0;
    }

    f = fopen(save, "rb");
    if(!f)
        return 0;

    fstat(fileno(f), &st);
    setMcDateTime(&fctime, gmtime(&st.st_ctime));
    setMcDateTime(&fmtime, gmtime(&st.st_mtime));

    fread(&header, 1, sizeof(maxHeader_t), f);

    if(!maxChecksumOk(f, header.crc))
    {
        LOG("ERROR! Damaged save: the file's CRC does not match the one in "
            "its header. Refusing to convert %s", save);
        fclose(f);
        return 0;
    }

    // Get compressed file entries. compressedSize cannot be trusted either:
    // real saves under-report it (BASLUS-20963FF1200.max claims 17529 for a
    // 17533 byte stream), and handing unlzari only what the header allows
    // starves the decoder - it stops early, returns a short buffer, and the
    // last file in the save is silently truncated. Read everything from the
    // start of the stream to the end of the file instead; the stream carries
    // its own length in its first word, so the extra bytes are harmless.
    long streamStart = sizeof(maxHeader_t) - 4;
    long fileLen;
    u32 avail;

    fseek(f, 0, SEEK_END);
    fileLen = ftell(f);

    /* isMAXFile() has already rejected anything shorter than the header, but
     * that was a different open of the file; and ftell() can simply fail. */
    if(fileLen < streamStart)
    {
        LOG("ERROR! Cannot read the save's length: %s", save);
        fclose(f);
        return 0;
    }

    avail = (u32)(fileLen - streamStart);

    u8 *compressed = malloc(avail);
    if(!compressed)
    {
        fclose(f);
        return 0;
    }

    fseek(f, streamStart, SEEK_SET); // Seek to beginning of LZARI stream.
    u32 ret = fread(compressed, 1, avail, f);
    if(ret != avail)
    {
        LOG("Compressed size: actual=%u, expected=%u", ret, avail);
        avail = ret;
    }

    fclose(f);
    // calloc, not malloc: a short stream leaves the tail untouched, and it
    // must read as zeros rather than as whatever was on the heap.
    u8 *decompressed = calloc(1, header.decompressedSize);
    if(!decompressed)
    {
        free(compressed);
        return 0;
    }

    ret = unlzari(compressed, avail, decompressed, header.decompressedSize);
    free(compressed);
    // As with other save formats, decompressedSize isn't acccurate.
    if(ret == 0)
    {
        LOG("Decompression failed.");
        free(decompressed);
        return 0;
    }

    if(!maxEntriesFit(decompressed, ret, header.numFiles))
    {
        LOG("ERROR! Truncated or corrupt save: the %u decompressed bytes do "
            "not cover the %u files the header declares.", ret, header.numFiles);
        free(decompressed);
        return 0;
    }

    psv = fopen(psv_path, "wb");
    if (!psv)
    {
        LOG("ERROR! Could not create PSV file: %s", psv_path);
        free(decompressed);
        return 0;
    }

    int i;
    u32 offset = 0;
    u32 dataPos = 0;
    maxEntry_t *entry;
    
    ps2_header_t ps2h;
    ps2_IconSys_t *ps2sys = NULL;
    ps2_MainDirInfo_t ps2md;
    
    memset(&ps2h, 0, sizeof(ps2_header_t));
    memset(&ps2md, 0, sizeof(ps2_MainDirInfo_t));
    
    ps2h.numberOfFiles = (header.numFiles);

    ps2md.attribute = 0x00008427;
    ps2md.numberOfFilesInDir = (header.numFiles+2);
    memcpy(&ps2md.created, &fctime, sizeof(sceMcStDateTime));
    memcpy(&ps2md.modified, &fmtime, sizeof(sceMcStDateTime));
    memcpy(ps2md.filename, header.dirName, sizeof(ps2md.filename));
    
    write_psv_header(psv, 2);

    LOG("\nSave contents:\n");

    // Find the icon.sys (need to know the icons names)
    for(i = 0, offset = 0; i < header.numFiles; i++)
    {
        entry = (maxEntry_t*) &decompressed[offset];
        offset += sizeof(maxEntry_t);

        if(strcmp(entry->name, "icon.sys") == 0)
            ps2sys = (ps2_IconSys_t*) &decompressed[offset];

        offset = nextEntryOffset(offset + entry->length);
        ps2h.displaySize += entry->length;

        LOG(" %8d bytes  : %s", entry->length, entry->name);
    }

    LOG(" %8d Total bytes", ps2h.displaySize);

    if (!ps2sys)
    {
        LOG("ERROR! Save has no icon.sys.");
        fclose(psv);
        remove(psv_path);
        free(decompressed);
        return 0;
    }

    // Calculate the start offset for the file's data
    dataPos = sizeof(psv_header_t) + sizeof(ps2_header_t) + sizeof(ps2_MainDirInfo_t) + sizeof(ps2_FileInfo_t)*header.numFiles;

    ps2_FileInfo_t *ps2fi = malloc(sizeof(ps2_FileInfo_t)*header.numFiles);

    // Build the PS2 FileInfo entries
    for(i = 0, offset = 0; i < header.numFiles; i++)
    {
        entry = (maxEntry_t*) &decompressed[offset];
        offset += sizeof(maxEntry_t);

        ps2fi[i].attribute = 0x00008497;
        ps2fi[i].positionInFile = (dataPos);
        ps2fi[i].filesize = (entry->length);
        memcpy(&ps2fi[i].created, &fctime, sizeof(sceMcStDateTime));
        memcpy(&ps2fi[i].modified, &fmtime, sizeof(sceMcStDateTime));
        memcpy(ps2fi[i].filename, entry->name, sizeof(ps2fi[i].filename));

        dataPos += entry->length;

        set_ps2header_values(&ps2h, &ps2fi[i], ps2sys);

        offset = nextEntryOffset(offset + entry->length);
    }

    fwrite(&ps2h, sizeof(ps2_header_t), 1, psv);
    fwrite(&ps2md, sizeof(ps2_MainDirInfo_t), 1, psv);
    fwrite(ps2fi, sizeof(ps2_FileInfo_t), header.numFiles, psv);

    free(ps2fi);
    
    // Write the file's data
    for(i = 0, offset = 0; i < header.numFiles; i++)
    {
        entry = (maxEntry_t*) &decompressed[offset];
        offset += sizeof(maxEntry_t);

        fwrite(&decompressed[offset], 1, entry->length, psv);
 
        offset = nextEntryOffset(offset + entry->length);
    }

    fclose(psv);
    free(decompressed);

    return 1;
}

static void cbsCrypt(uint8_t *buf, size_t bufLen)
{
    mbedtls_arc4_context ctx;

    mbedtls_arc4_init(&ctx);
    memcpy(ctx.m, cbsKey, sizeof(cbsKey));
    mbedtls_arc4_crypt(&ctx, bufLen, buf, buf);
}

static int isCBSFile(const char *path)
{
    if(!path)
        return 0;
    
    FILE *f = fopen(path, "rb");
    if(!f)
        return 0;

    // Verify file size
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(len < sizeof(cbsHeader_t))
    {
        fclose(f);
        return 0;
    }

    // Verify header magic
    char magic[4];
    fread(magic, 1, 4, f);
    fclose(f);

    if(memcmp(magic, CBS_HEADER_MAGIC, 4) != 0)
        return 0;

    return 1;
}

int ps2_cbs2psv(const char *save, const char *psv_path)
{
    FILE *dstFile;
    u8 *cbsData;
    u8 *compressed;
    u8 *decompressed;
    cbsHeader_t header;
    cbsEntry_t *entryHeader;
    unsigned long decompressedSize;
    size_t cbsLen;
    int i, numFiles = 0;
    u32 dataPos = 0, offset = 0;

    if(!isCBSFile(save))
    {
        LOG("Not a valid CodeBreaker file: %s", save);
        return 0;
    }

    if(read_buffer(save, &cbsData, &cbsLen) < 0)
        return 0;

    memcpy(&header, cbsData, sizeof(cbsHeader_t));
    dstFile = fopen(psv_path, "wb");

    if (!dstFile)
    {
        LOG("ERROR! Could not create PSV file: %s", psv_path);
        free(cbsData);
        return 0;
    }

    // Get data for file entries
    compressed = cbsData + sizeof(cbsHeader_t);
    // Some tools create .CBS saves with an incorrect compressed size in the header.
    // It can't be trusted!
    cbsCrypt(compressed, cbsLen - sizeof(cbsHeader_t));
    decompressedSize = header.decompressedSize;
    decompressed = malloc(decompressedSize);
    int z_ret = uncompress(decompressed, &decompressedSize, compressed, cbsLen - sizeof(cbsHeader_t));
    free(cbsData);
    
    if(z_ret != Z_OK)
    {
        // Compression failed.
        LOG("Decompression failed! (Z_ERR = %d)", z_ret);
        free(decompressed);
        return 0;
    }

    ps2_header_t ps2h;
    ps2_IconSys_t *ps2sys = NULL;
    // icon.sys is not always sizeof(ps2_IconSys_t). Point ps2sys at a zeroed
    // copy rather than into the buffer, so a short one cannot be read past.
    ps2_IconSys_t iconsys;
    ps2_MainDirInfo_t ps2md;

    memset(&ps2h, 0, sizeof(ps2_header_t));
    memset(&ps2md, 0, sizeof(ps2_MainDirInfo_t));
    memset(&iconsys, 0, sizeof(ps2_IconSys_t));

    ps2md.attribute = header.mode;
    memcpy(&ps2md.created, &header.created, sizeof(sceMcStDateTime));
    memcpy(&ps2md.modified, &header.modified, sizeof(sceMcStDateTime));
    memcpy(ps2md.filename, header.name, sizeof(ps2md.filename));

    write_psv_header(dstFile, 2);

    LOG("Save contents:\n");

    // Find the icon.sys (need to know the icons names)
    // decompressedSize is unsigned: the original condition wrapped to a huge
    // value for a save smaller than one entry header, walking off the buffer.
    while(decompressedSize > sizeof(cbsEntry_t) &&
          offset < (decompressedSize - sizeof(cbsEntry_t)))
    {
        numFiles++;

        entryHeader = (cbsEntry_t*) &decompressed[offset];
        offset += sizeof(cbsEntry_t);

        // Entry lengths come from the file. Without this the loops below index
        // the buffer by a figure nothing has checked.
        if(entryHeader->length > decompressedSize - offset)
        {
            LOG("ERROR! Corrupt save: '%s' claims %u bytes, %u remain.",
                entryHeader->name, entryHeader->length,
                (u32)(decompressedSize - offset));
            fclose(dstFile);
            remove(psv_path);
            free(decompressed);
            return 0;
        }

        if(strcmp(entryHeader->name, "icon.sys") == 0)
        {
            u32 want = (entryHeader->length < sizeof(ps2_IconSys_t))
                     ? entryHeader->length : sizeof(ps2_IconSys_t);

            memcpy(&iconsys, &decompressed[offset], want);
            ps2sys = &iconsys;
        }

        ps2h.displaySize += entryHeader->length;
        offset += entryHeader->length;

        LOG(" %8d bytes  : %s", entryHeader->length, entryHeader->name);
    }

    LOG(" %8d Total bytes", ps2h.displaySize);
    ps2h.displaySize = (ps2h.displaySize);
    ps2h.numberOfFiles = (numFiles);
    ps2md.numberOfFilesInDir = (numFiles+2);

    if (!ps2sys)
    {
        LOG("ERROR! Save has no icon.sys.");
        fclose(dstFile);
        remove(psv_path);
        free(decompressed);
        return 0;
    }

    // Calculate the start offset for the file's data
    dataPos = sizeof(psv_header_t) + sizeof(ps2_header_t) + sizeof(ps2_MainDirInfo_t) + sizeof(ps2_FileInfo_t)*numFiles;

    ps2_FileInfo_t *ps2fi = malloc(sizeof(ps2_FileInfo_t)*numFiles);

    // Build the PS2 FileInfo entries
    for(i = 0, offset = 0; i < numFiles; i++)
    {
        entryHeader = (cbsEntry_t*) &decompressed[offset];
        offset += sizeof(cbsEntry_t);

        ps2fi[i].attribute = entryHeader->mode;
        ps2fi[i].positionInFile = (dataPos);
        ps2fi[i].filesize = entryHeader->length;
        memcpy(&ps2fi[i].created, &entryHeader->created, sizeof(sceMcStDateTime));
        memcpy(&ps2fi[i].modified, &entryHeader->modified, sizeof(sceMcStDateTime));
        memcpy(ps2fi[i].filename, entryHeader->name, sizeof(ps2fi[i].filename));

        dataPos += entryHeader->length;

        set_ps2header_values(&ps2h, &ps2fi[i], ps2sys);

        offset += entryHeader->length;
    }

    fwrite(&ps2h, sizeof(ps2_header_t), 1, dstFile);
    fwrite(&ps2md, sizeof(ps2_MainDirInfo_t), 1, dstFile);
    fwrite(ps2fi, sizeof(ps2_FileInfo_t), numFiles, dstFile);

    free(ps2fi);

    // Write the file's data
    for(i = 0, offset = 0; i < numFiles; i++)
    {
        entryHeader = (cbsEntry_t*) &decompressed[offset];
        offset += sizeof(cbsEntry_t);

        fwrite(&decompressed[offset], 1, entryHeader->length, dstFile);
 
        offset += entryHeader->length;
    }

    fclose(dstFile);
    free(decompressed);

    return 1;
}

/*
 * The four bytes after the body are a checksum of it. Not every writer appends
 * them - a file that stops at the last byte of data is accepted - but when
 * they are there they have to agree, because nothing else in an .xps can catch
 * a corrupted byte: the container is uncompressed and the entry sizes are all
 * self-consistent, so a flipped bit inside a file just becomes a flipped bit
 * on the card. A stored zero means the writer left the field alone.
 */
static int xpsChecksumOk(FILE *f, long bodyStart, u32 bodySize)
{
    u8 buf[4096];
    u32 sum = 0, stored;
    long fileLen, end;
    size_t left = bodySize, n, i;

    if(fseek(f, 0, SEEK_END) != 0)
        return 0;

    fileLen = ftell(f);
    if(fileLen < 0 || bodyStart < 0)
        return 0;               /* cannot tell where anything is */

    end = bodyStart + (long) bodySize;

    if(end < bodyStart || end > fileLen)
        return 0;               /* the body overflows, or runs past the file */

    /* Not every writer appends the checksum, so a file that stops at the last
     * byte of data is accepted. Anything other than exactly four trailing
     * bytes is not a trailer this format defines, and there is nothing to
     * check it against. */
    if(fileLen - end != 4)
        return 1;

    if(fseek(f, bodyStart, SEEK_SET) != 0)
        return 0;

    while(left > 0)
    {
        n = fread(buf, 1, (left < sizeof(buf)) ? left : sizeof(buf), f);
        if(n == 0)
            return 0;

        for(i = 0; i < n; i++)
            sum += (u32) buf[i] << (sum % 24);

        left -= n;
    }

    if(fread(&stored, 1, sizeof(stored), f) != sizeof(stored))
        return 0;

    return (stored == 0) || (stored == sum);
}

int ps2_xps2psv(const char *save, const char *psv_path)
{
    u32 len, dataPos = 0;
    FILE *xpsFile, *psvFile;
    int numFiles, i;
    char tmp[100];
    long bodyStart;
    u8 *data;
    xpsEntry_t entry;

    xpsFile = fopen(save, "rb");
    if(!xpsFile)
        return 0;

    fread(&tmp, 1, 0x15, xpsFile);

    if (memcmp(&tmp[4], XPS_HEADER_MAGIC, 16) != 0)
    {
        LOG("Not a valid XPS file: %s", save);
        fclose(xpsFile);
        return 0;
    }

    // Skip the variable size header: three length-prefixed strings (title,
    // date and the writing tool's comment), then the size of the rest of the
    // file. The comment is empty in most saves, which makes its length look
    // like a spare zero word - but PS2SaveConverter fills it in, and reading
    // only two strings then skipping 8 bytes lands in the middle of it.
    // Seek past them instead of reading: these are longer than tmp[].
    for (i = 0; i < 3; i++)
    {
        if (fread(&len, 1, sizeof(uint32_t), xpsFile) != sizeof(uint32_t) ||
            fseek(xpsFile, len, SEEK_CUR) != 0)
        {
            LOG("Not a valid XPS file: %s", save);
            fclose(xpsFile);
            return 0;
        }
    }
    if (fread(&len, 1, sizeof(uint32_t), xpsFile) != sizeof(uint32_t))
    {
        LOG("Not a valid XPS file: %s", save);
        fclose(xpsFile);
        return 0;
    }

    bodyStart = ftell(xpsFile);
    if (bodyStart < 0)
    {
        LOG("Not a valid XPS file: %s", save);
        fclose(xpsFile);
        return 0;
    }

    if (!xpsChecksumOk(xpsFile, bodyStart, len))
    {
        LOG("ERROR! Damaged save: the file's checksum does not match its "
            "contents. Refusing to convert %s", save);
        fclose(xpsFile);
        return 0;
    }
    fseek(xpsFile, bodyStart, SEEK_SET);

    // Read main directory entry
    fread(&entry, 1, sizeof(xpsEntry_t), xpsFile);
    numFiles = entry.length - 2;

    // Keep the file position (start of file entries)
    len = ftell(xpsFile);

    psvFile = fopen(psv_path, "wb");
    if(!psvFile)
    {
        LOG("ERROR! Could not create PSV file: %s", psv_path);
        fclose(xpsFile);
        return 0;
    }

    ps2_header_t ps2h;
    ps2_IconSys_t ps2sys;
    ps2_MainDirInfo_t ps2md;

    memset(&ps2h, 0, sizeof(ps2_header_t));
    memset(&ps2md, 0, sizeof(ps2_MainDirInfo_t));
    // Only filled in if the save has an icon.sys; the icon name comparisons
    // below read it either way.
    memset(&ps2sys, 0, sizeof(ps2_IconSys_t));

    ps2h.numberOfFiles = numFiles;

    ps2md.attribute = xps_mode_swap(entry.mode);
    ps2md.numberOfFilesInDir = entry.length;
    memcpy(&ps2md.created, &entry.created, sizeof(sceMcStDateTime));
    memcpy(&ps2md.modified, &entry.modified, sizeof(sceMcStDateTime));
    memcpy(ps2md.filename, entry.name, sizeof(ps2md.filename));

    write_psv_header(psvFile, 2);

    // Find the icon.sys (need to know the icons names)
    for(i = 0; i < numFiles; i++)
    {
        fread(&entry, 1, sizeof(xpsEntry_t), xpsFile);

        if(strcmp(entry.name, "icon.sys") == 0)
        {
            // Real saves carry icon.sys files that are not exactly
            // sizeof(ps2_IconSys_t): read what fits, then seek past the rest
            // so the stream stays aligned with the entry.
            u32 want = (entry.length < sizeof(ps2_IconSys_t)) ? entry.length : sizeof(ps2_IconSys_t);

            fread(&ps2sys, 1, want, xpsFile);
            fseek(xpsFile, entry.length - want, SEEK_CUR);
        }
        else
            fseek(xpsFile, entry.length, SEEK_CUR);

        ps2h.displaySize += entry.length;

        LOG(" %8d bytes  : %s", entry.length, entry.name);
    }

    LOG(" %8d Total bytes", ps2h.displaySize);

    // Rewind
    fseek(xpsFile, len, SEEK_SET);

    // Calculate the start offset for the file's data
    dataPos = sizeof(psv_header_t) + sizeof(ps2_header_t) + sizeof(ps2_MainDirInfo_t) + sizeof(ps2_FileInfo_t)*numFiles;

    ps2_FileInfo_t *ps2fi = malloc(sizeof(ps2_FileInfo_t)*numFiles);

    // Build the PS2 FileInfo entries
    for(i = 0; i < numFiles; i++)
    {
        fread(&entry, 1, sizeof(xpsEntry_t), xpsFile);

        ps2fi[i].attribute = xps_mode_swap(entry.mode);
        ps2fi[i].positionInFile = dataPos;
        ps2fi[i].filesize = entry.length;
        memcpy(&ps2fi[i].created, &entry.created, sizeof(sceMcStDateTime));
        memcpy(&ps2fi[i].modified, &entry.modified, sizeof(sceMcStDateTime));
        memcpy(ps2fi[i].filename, entry.name, sizeof(ps2fi[i].filename));

        dataPos += entry.length;
        fseek(xpsFile, entry.length, SEEK_CUR);
        
        set_ps2header_values(&ps2h, &ps2fi[i], &ps2sys);
    }

    fwrite(&ps2h, sizeof(ps2_header_t), 1, psvFile);
    fwrite(&ps2md, sizeof(ps2_MainDirInfo_t), 1, psvFile);
    fwrite(ps2fi, sizeof(ps2_FileInfo_t), numFiles, psvFile);

    free(ps2fi);

    // Rewind
    fseek(xpsFile, len, SEEK_SET);

    // Copy each file entry
    for(i = 0; i < numFiles; i++)
    {
        fread(&entry, 1, sizeof(xpsEntry_t), xpsFile);
        
        data = malloc(entry.length);
        fread(data, 1, entry.length, xpsFile);
        fwrite(data, 1, entry.length, psvFile);

        free(data);
    }

    fclose(psvFile);
    fclose(xpsFile);

    return 1;
}
