/*
* original .MAX, .CBS, .PSU file decoding from Cheat Device PS2 by root670
* https://github.com/root670/CheatDevicePS2
*/

#include <stdio.h>
#include <zlib.h>
#include <polarssl/arc4.h>

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

static int roundUp(int i, int j)
{
    return (i + j - 1) / j * j;
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

    psv = fopen(psv_path, "wb");
    if (!psv)
    {
        LOG("ERROR! Could not create PSV file: %s", psv_path);
        fclose(f);
        return 0;
    }

    // Get compressed file entries
    u8 *compressed = malloc(header.compressedSize);

    fseek(f, sizeof(maxHeader_t) - 4, SEEK_SET); // Seek to beginning of LZARI stream.
    u32 ret = fread(compressed, 1, header.compressedSize, f);
    if(ret != header.compressedSize)
    {
        LOG("Compressed size: actual=%d, expected=%d\n", ret, header.compressedSize);
        header.compressedSize = ret;
    }

    fclose(f);
    u8 *decompressed = malloc(header.decompressedSize);

    ret = unlzari(compressed, header.compressedSize, decompressed, header.decompressedSize);
    free(compressed);
    // As with other save formats, decompressedSize isn't acccurate.
    if(ret == 0)
    {
        LOG("Decompression failed.");
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

        offset = roundUp(offset + entry->length + 8, 16) - 8;
        ps2h.displaySize += entry->length;

        LOG(" %8d bytes  : %s", entry->length, entry->name);
    }

    LOG(" %8d Total bytes", ps2h.displaySize);

    if (!ps2sys)
        return 0;

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

        offset = roundUp(offset + entry->length + 8, 16) - 8;
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
 
        offset = roundUp(offset + entry->length + 8, 16) - 8;
    }

    fclose(psv);
    free(decompressed);

    return 1;
}

static void cbsCrypt(uint8_t *buf, size_t bufLen)
{
    arc4_context ctx;

    memset(&ctx, 0, sizeof(arc4_context));
    memcpy(ctx.m, cbsKey, sizeof(cbsKey));
    arc4_crypt(&ctx, bufLen, buf, buf);
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
    ps2_MainDirInfo_t ps2md;
    
    memset(&ps2h, 0, sizeof(ps2_header_t));
    memset(&ps2md, 0, sizeof(ps2_MainDirInfo_t));

    ps2md.attribute = header.mode;
    memcpy(&ps2md.created, &header.created, sizeof(sceMcStDateTime));
    memcpy(&ps2md.modified, &header.modified, sizeof(sceMcStDateTime));
    memcpy(ps2md.filename, header.name, sizeof(ps2md.filename));

    write_psv_header(dstFile, 2);

    LOG("Save contents:\n");

    // Find the icon.sys (need to know the icons names)
    while(offset < (decompressedSize - sizeof(cbsEntry_t)))
    {
        numFiles++;

        entryHeader = (cbsEntry_t*) &decompressed[offset];
        offset += sizeof(cbsEntry_t);

        if(strcmp(entryHeader->name, "icon.sys") == 0)
            ps2sys = (ps2_IconSys_t*) &decompressed[offset];

        ps2h.displaySize += entryHeader->length;
        offset += entryHeader->length;

        LOG(" %8d bytes  : %s", entryHeader->length, entryHeader->name);
    }

    LOG(" %8d Total bytes", ps2h.displaySize);
    ps2h.displaySize = (ps2h.displaySize);
    ps2h.numberOfFiles = (numFiles);
    ps2md.numberOfFilesInDir = (numFiles+2);

    if (!ps2sys)
        return 0;

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

int ps2_xps2psv(const char *save, const char *psv_path)
{
    u32 len, dataPos = 0;
    FILE *xpsFile, *psvFile;
    int numFiles, i;
    char tmp[100];
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

    // Skip the variable size header
    fread(&len, 1, sizeof(uint32_t), xpsFile);
    fread(&tmp, 1, len, xpsFile);
    fread(&len, 1, sizeof(uint32_t), xpsFile);
    fread(&tmp, 1, len, xpsFile);
    fread(&len, 1, sizeof(uint32_t), xpsFile);
    fread(&len, 1, sizeof(uint32_t), xpsFile);

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
            fread(&ps2sys, 1, sizeof(ps2_IconSys_t), xpsFile);
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
