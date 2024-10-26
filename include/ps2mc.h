/*
*  PSV file format information from:
*  - https://github.com/PMStanley/PSV-Exporter
*
*  PSU, MAX, CBS file format information from:
*  - https://github.com/root670/CheatDevicePS2
*/

#include <inttypes.h>

#define PSV_MAGIC       "\x00VSP"
#define PSV_SALT        "www.bucanero.com.ar"

typedef struct _sceMcStDateTime
{
    uint8_t  Resv2;
    uint8_t  Sec;
    uint8_t  Min;
    uint8_t  Hour;
    uint8_t  Day;
    uint8_t  Month;
    uint16_t Year;
} sceMcStDateTime;

typedef struct                       // size = 512
{
    uint16_t mode;                   // 0
    uint16_t unused;                 // 2
    uint32_t length;                 // 4
    sceMcStDateTime created;         // 8
    uint32_t cluster;                // 16
    uint32_t dir_entry;              // 20
    sceMcStDateTime modified;        // 24
    uint32_t attr;                   // 32
    uint32_t unused2[7];             // 36
    char name[32];                   // 64
    uint8_t  unused3[416];           // 96
} McFsEntry;

typedef struct
{
	char magic[4];
	uint32_t padding1;               //always 0x00000000
	uint8_t salt[20];
	uint8_t signature[20];           //digital sig
	uint32_t padding2;               //always 0x00000000
	uint32_t padding3;               //always 0x00000000
	uint32_t headerSize;             //always 0x0000002C in PS2, 0x00000014 in PS1. 
	uint32_t saveType;               //0x00000002 PS2, 0x00000001 PS1
} psv_header_t;

typedef struct
{
	uint32_t displaySize;            //PS3 will just round this up to the neaest 1024 boundary so just make it as good as possible
	uint32_t sysPos;                 //location in file of icon.sys
	uint32_t sysSize;                //icon.sys size
	uint32_t icon1Pos;               //position of 1st icon
	uint32_t icon1Size;              //size of 1st icon
	uint32_t icon2Pos;               //position of 2nd icon
	uint32_t icon2Size;              //size of 2nd icon
	uint32_t icon3Pos;               //position of 3rd icon
	uint32_t icon3Size;              //size of 3rd icon
	uint32_t numberOfFiles;
} ps2_header_t;

typedef struct
{
	sceMcStDateTime created;
	sceMcStDateTime modified;
	uint32_t numberOfFilesInDir;     // this is likely to be number of files in dir + 2 ("." and "..")
	uint32_t attribute;              // (0x00008427 dir)
	char filename[32];
} ps2_MainDirInfo_t;

typedef struct
{
	sceMcStDateTime created;
	sceMcStDateTime modified;
	uint32_t filesize;
	uint32_t attribute;             // (0x00008497 file)
	char filename[32];              // 'Real' PSV files have junk in this after text.
	uint32_t positionInFile;
} ps2_FileInfo_t;

typedef struct
{
    char magic[4];
    uint16_t padding1;             // 0000
    uint16_t secondLineOffset;
    uint32_t padding2;             // 00000000
    uint32_t transparencyVal;      // 0x00 (clear) to 0x80 (opaque)
    uint8_t bgColourUpperLeft[16];
    uint8_t bgColourUpperRight[16];
    uint8_t bgColourLowerLeft[16];
    uint8_t bgColourLowerRight[16];
    uint8_t light1Direction[16];
    uint8_t light2Direction[16];
    uint8_t light3Direction[16];
    uint8_t light1RGB[16];
    uint8_t light2RGB[16];
    uint8_t light3RGB[16];
    uint8_t ambientLightRGB[16];
    char title[68];            // null terminated, S-JIS
    char IconName[64];         // null terminated
    char copyIconName[64];     // null terminated
    char deleteIconName[64];   // null terminated
    uint8_t padding3[512];
} ps2_IconSys_t;

typedef struct maxHeader
{
    char     magic[12];
    uint32_t crc;
    char     dirName[32];
    char     iconSysName[32];
    uint32_t compressedSize;
    uint32_t numFiles;
    uint32_t decompressedSize; // This is actually the start of the LZARI stream, but we need it to  allocate the buffer.
} maxHeader_t;

typedef struct maxEntry
{
    uint32_t length;
    char     name[32];
} maxEntry_t;

typedef struct cbsHeader
{
    char magic[4];
    uint32_t unk1;
    uint32_t dataOffset;
    uint32_t decompressedSize;
    uint32_t compressedSize;
    char name[32];
    sceMcStDateTime created;
    sceMcStDateTime modified;
    uint32_t unk2;
    uint32_t mode;
    char unk3[16];
    char title[72];
    char description[132];
} cbsHeader_t;

typedef struct cbsEntry
{
    sceMcStDateTime created;
    sceMcStDateTime modified;
    uint32_t length;
    uint32_t mode;
    char unk1[8];
    char name[32];
} cbsEntry_t;

typedef struct __attribute__((__packed__)) xpsEntry
{
    uint16_t entry_sz;
    char name[64];
    uint32_t length;
    uint32_t start;
    uint32_t end;
    uint32_t mode;
    sceMcStDateTime created;
    sceMcStDateTime modified;
    char unk1[4];
    char padding[12];
    char title_ascii[64];
    char title_sjis[64];
    char unk2[8];
} xpsEntry_t;
