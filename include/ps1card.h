#include <inttypes.h>

#define PS1CARD_MAX_SLOTS       15
#define PS1CARD_HEADER_SIZE     128
#define PS1CARD_BLOCK_SIZE      8192
#define PS1CARD_SIZE            131072

enum ps1card_type
{
    PS1CARD_NULL,
    PS1CARD_RAW,
    PS1CARD_GME,
    PS1CARD_VGS,
    PS1CARD_VMP,
    PS1CARD_MCX,
};

enum ps1save_type
{
    PS1SAVE_NULL,
    PS1SAVE_AR,
    PS1SAVE_MCS,
    PS1SAVE_RAW,
    PS1SAVE_PSV,
};

enum ps1block_type
{
    PS1BLOCK_FORMATTED,
    PS1BLOCK_INITIAL,
    PS1BLOCK_MIDDLELINK,
    PS1BLOCK_ENDLINK,
    PS1BLOCK_DELETED_INITIAL,
    PS1BLOCK_DELETED_MIDDLELINK,
    PS1BLOCK_DELETED_ENDLINK,
    PS1BLOCK_CORRUPTED,
};

typedef struct ps1McData
{
    uint8_t* headerData;                //Memory Card header data (128 bytes each)
    uint8_t* saveData;                  //Memory Card save data (8192 bytes each)
    uint32_t iconPalette[16];           //Memory Card icon palette data
    uint8_t iconData[3][256];           //Memory Card icon data, 3 icons per slot, (16*16px icons)
    uint8_t iconFrames;                 //Number of icon frames
    uint8_t saveType;                   //Type of the save (0 - formatted, 1 - initial, 2 - middle link, 3 - end link, 4 - deleted initial, 5 - deleted middle link, 6 - deleted end link, 7 - corrupted)
    uint16_t saveRegion;                //Region of the save (16 bit value, ASCII representation: BA - America, BE - Europe, BI - Japan)
    char saveProdCode[11];              //Product code of the save
    char saveIdentifier[9];             //Identifier string of the save
    int saveSize;                       //Size of the save in Bytes
    char saveName[21];                  //Name of the save
    char saveTitle[65];                 //Title of the save data (in Shift-JIS format)
} ps1mcData_t;

//Open Memory Card from the given filename (return error message if operation is not sucessful)
int openMemoryCard(const char* fileName, int fixData);

//Open memory card from the given byte stream
int openMemoryCardStream(const uint8_t* memCardData, int dataSize, int fixData);

//Save (export) Memory Card to a given byte stream
uint8_t* saveMemoryCardStream(int fixData);

//Save Memory Card to the given filename
int saveMemoryCard(const char* fileName, int memoryCardType, int fixData);

//Format a complete Memory Card
void formatMemoryCard(void);

//Import single save to the Memory Card
int openSingleSave(const char* fileName, int* requiredSlots);

//Save single save to the given filename
int saveSingleSave(const char* fileName, int slotNumber, int singleSaveType);

//Get icon data as bytes
uint8_t* getIconRGBA(int slotNumber, int frame);

//Set icon data to saveData
void setIconBytes(int slotNumber, uint8_t* iconBytes);

//Return all bytes of the specified save
uint8_t* getSaveBytes(int slotNumber, uint32_t* saveLen);

//Toggle deleted/undeleted status
void toggleDeleteSave(int slotNumber);

//Format save
int formatSave(int slotNumber);

//Get Memory Card data
ps1mcData_t* getMemoryCardData(void);
