// PS1 Memory Card management by Bucanero
//
//based on PS1 Memory Card class
//by Shendo 2009-2021 (MemcardRex)
// https://github.com/ShendoXT/memcardrex/blob/master/MemcardRex/ps1card.cs

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <polarssl/aes.h>
#include <polarssl/sha256.h>

#include "ps1card.h"
#include "util.h"
#include "saves.h"


//Memory Card's type (0 - unset, 1 - raw, 2 - gme, 3 - vgs, 4 - vmp);
static uint8_t cardType = PS1CARD_NULL;

//Flag used to determine if the card has been edited since the last saving
static bool changedFlag = false;

//Complete Memory Card in the raw format (131072 bytes)
static uint8_t rawMemoryCard[PS1CARD_SIZE];
static ps1mcData_t ps1saves[PS1CARD_MAX_SLOTS];

//Save comments (supported by .gme files only), 255 characters allowed
static char saveComments[PS1CARD_MAX_SLOTS][256];

//AES-CBC key and IV for MCX memory cards
static const uint8_t mcxKey[] = { 0x81, 0xD9, 0xCC, 0xE9, 0x71, 0xA9, 0x49, 0x9B, 0x04, 0xAD, 0xDC, 0x48, 0x30, 0x7F, 0x07, 0x92 };
static const uint8_t mcxIv[]  = { 0x13, 0xC2, 0xE7, 0x69, 0x4B, 0xEC, 0x69, 0x6D, 0x52, 0xCF, 0x00, 0x09, 0x2A, 0xC1, 0xF2, 0x72 };


//Encrypts a buffer using AES CBC 128
static void AesCbcEncrypt(uint8_t* toEncrypt, size_t len, const uint8_t* key, const uint8_t* aes_iv)
{
    uint8_t iv[16];
    aes_context ctx;

    memcpy(iv, aes_iv, 16);

    aes_init(&ctx);
    aes_setkey_enc(&ctx, key, 128);
    aes_crypt_cbc(&ctx, AES_ENCRYPT, len, iv, toEncrypt, toEncrypt);
    aes_free(&ctx);
}

//Decrypts a buffer using AES CBC 128
static void AesCbcDecrypt(uint8_t* toDecrypt, size_t len, const uint8_t* key, const uint8_t* aes_iv)
{
    uint8_t iv[16];
    aes_context ctx;

    memcpy(iv, aes_iv, 16);

    aes_init(&ctx);
    aes_setkey_dec(&ctx, key, 128);
    aes_crypt_cbc(&ctx, AES_DECRYPT, len, iv, toDecrypt, toDecrypt);
    aes_free(&ctx);
}

//Load Data from raw Memory Card
static void loadDataFromRawCard(void)
{
    for (int slotNumber = 0; slotNumber < PS1CARD_MAX_SLOTS; slotNumber++)
    {
        //Load header data
        ps1saves[slotNumber].headerData = &rawMemoryCard[PS1CARD_HEADER_SIZE * (slotNumber + 1)];

        //Load save 
        ps1saves[slotNumber].saveData = &rawMemoryCard[PS1CARD_BLOCK_SIZE * (slotNumber + 1)];
    }
}

//Recreate raw Memory Card
static void loadDataToRawCard(bool fixData)
{
    uint8_t* tmpMemoryCard;

    //Skip fixing data if it's not needed
    if (!fixData) return;

    //Clear existing data
    tmpMemoryCard = calloc(1, PS1CARD_SIZE);

    //Recreate the signature
    tmpMemoryCard[0] = 0x4D;        //M
    tmpMemoryCard[1] = 0x43;        //C
    tmpMemoryCard[127] = 0x0E;      //XOR (precalculated)

    tmpMemoryCard[8064] = 0x4D;     //M
    tmpMemoryCard[8065] = 0x43;     //C
    tmpMemoryCard[8191] = 0x0E;     //XOR (precalculated)

    //This can be copied freely without fixing
    for (int slotNumber = 0; slotNumber < PS1CARD_MAX_SLOTS; slotNumber++)
    {
        //Load header data
        memcpy(&tmpMemoryCard[PS1CARD_HEADER_SIZE * (slotNumber + 1)], ps1saves[slotNumber].headerData, PS1CARD_HEADER_SIZE);

        //Load save data
        memcpy(&tmpMemoryCard[PS1CARD_BLOCK_SIZE * (slotNumber + 1)], ps1saves[slotNumber].saveData, PS1CARD_BLOCK_SIZE);
    }

    //Create authentic data (just for completeness)
    for (int i = 0; i < 20; i++)
    {
        //Reserved slot typed
        tmpMemoryCard[2048 + (i * PS1CARD_HEADER_SIZE)] = 0xFF;
        tmpMemoryCard[2048 + (i * PS1CARD_HEADER_SIZE) + 1] = 0xFF;
        tmpMemoryCard[2048 + (i * PS1CARD_HEADER_SIZE) + 2] = 0xFF;
        tmpMemoryCard[2048 + (i * PS1CARD_HEADER_SIZE) + 3] = 0xFF;

        //Next slot pointer doesn't point to anything
        tmpMemoryCard[2048 + (i * PS1CARD_HEADER_SIZE) + 8] = 0xFF;
        tmpMemoryCard[2048 + (i * PS1CARD_HEADER_SIZE) + 9] = 0xFF;
    }

    memcpy(rawMemoryCard, tmpMemoryCard, PS1CARD_SIZE);
    free(tmpMemoryCard);
    return;
}

//Recreate GME header(add signature, slot description and comments)
static void fillGmeHeader(FILE* fp)
{
    uint8_t gmeHeader[3904];

    //Clear existing data
    memset(gmeHeader, 0, sizeof(gmeHeader));

    //Fill in the signature
    gmeHeader[0] = 0x31;        //1
    gmeHeader[1] = 0x32;        //2
    gmeHeader[2] = 0x33;        //3
    gmeHeader[3] = 0x2D;        //-
    gmeHeader[4] = 0x34;        //4
    gmeHeader[5] = 0x35;        //5
    gmeHeader[6] = 0x36;        //6
    gmeHeader[7] = 0x2D;        //-
    gmeHeader[8] = 0x53;        //S
    gmeHeader[9] = 0x54;        //T
    gmeHeader[10] = 0x44;       //D

    gmeHeader[18] = 0x1;
    gmeHeader[20] = 0x1;
    gmeHeader[21] = 0x4D;       //M

    for (int slotNumber = 0; slotNumber < PS1CARD_MAX_SLOTS; slotNumber++)
    {
        gmeHeader[22 + slotNumber] = ps1saves[slotNumber].headerData[0];
        gmeHeader[38 + slotNumber] = ps1saves[slotNumber].headerData[8];

        //Inject comments to GME header
        memcpy(&gmeHeader[64 + (256 * slotNumber)], saveComments[slotNumber], 256);
    }

    fwrite(gmeHeader, 1, sizeof(gmeHeader), fp);
    return;
}

// Check if a given card is a MCX image
static bool IsMcxCard(uint8_t* mcxCard)
{
    AesCbcDecrypt(mcxCard, 0x200A0, mcxKey, mcxIv);

    // Check for "MC" header 0x80 bytes in
    if (mcxCard[0x80] == 'M' && mcxCard[0x81] == 'C') 
        return true;
    else
        return false;
}

//Generate encrypted MCX Memory Card
static void MakeMcxCard(const uint8_t* rawCard, FILE* fp)
{
    uint8_t* mcxCard = malloc(0x200A0);

    memset(mcxCard, 0, 0x80);
    memcpy(mcxCard + 0x80, rawCard, PS1CARD_SIZE);
    sha256(mcxCard, 0x20080, mcxCard + 0x20080, 0);

    AesCbcEncrypt(mcxCard, 0x200A0, mcxKey, mcxIv);
    fwrite(mcxCard, 1, 0x200A0, fp);
    free(mcxCard);

    return;
}

//Generate unsigned VMP Memory Card
static void setVmpCardHeader(FILE* fp)
{
    uint8_t vmpCard[0x80];

    memset(vmpCard, 0, sizeof(vmpCard));
    vmpCard[1] = 0x50;
    vmpCard[2] = 0x4D;
    vmpCard[3] = 0x56;
    vmpCard[4] = 0x80; //header length 

    fwrite(vmpCard, 1, sizeof(vmpCard), fp);
    return;
}

//Generate unsigned PSV save
static void setPsvHeader(const char* saveFilename, uint32_t saveLength, FILE* fp)
{
    uint8_t psvSave[0x84];

    memset(psvSave, 0, sizeof(psvSave));
    psvSave[1] = 0x56;
    psvSave[2] = 0x53;
    psvSave[3] = 0x50;
    psvSave[0x38] = 0x14;
    psvSave[0x3C] = 1;
    psvSave[0x44] = 0x84;
    psvSave[0x49] = 2;
    psvSave[0x60] = 3;
    psvSave[0x61] = 0x90;

    memcpy(&psvSave[0x08], "www.bucanero.com.ar", 20);
    memcpy(&psvSave[0x64], saveFilename, 20);
    memcpy(&psvSave[0x40], &saveLength, sizeof(uint32_t));
    memcpy(&psvSave[0x5C], &saveLength, sizeof(uint32_t));

    fwrite(psvSave, 1, sizeof(psvSave), fp);
    return;
}

static void setArHeader(const char* saveName, FILE* fp)
{
    uint8_t arHeader[54];

    //Copy header data to arHeader
    memset(arHeader, 0, sizeof(arHeader));
    strncpy((char*) arHeader, saveName, 20);

    fwrite(arHeader, 1, sizeof(arHeader), fp);
    return;
}
//Calculate XOR checksum
static void calculateXOR(void)
{
    uint8_t XORchecksum = 0;

    //Cycle through each slot
    for (int slotNumber = 0; slotNumber < PS1CARD_MAX_SLOTS; slotNumber++)
    {
        //Set default value
        XORchecksum = 0;

        //Count 127 bytes
        for (int byteCount = 0; byteCount < 126; byteCount++)
            XORchecksum ^= ps1saves[slotNumber].headerData[byteCount];

        //Store checksum in 128th byte
        ps1saves[slotNumber].headerData[127] = XORchecksum;
    }
}

//Load region of the saves
static void loadRegion(void)
{
    //Cycle trough each slot
    for (int slotNumber = 0; slotNumber < PS1CARD_MAX_SLOTS; slotNumber++)
    {
        //Store save region
        ps1saves[slotNumber].saveRegion = (uint16_t)((ps1saves[slotNumber].headerData[11] << 8) | ps1saves[slotNumber].headerData[10]);
    }
}

//Load palette
static void loadPalette(void)
{
    int redChannel = 0;
    int greenChannel = 0;
    int blueChannel = 0;
    int colorCounter = 0;
    int blackFlag = 0;

    //Cycle through each slot on the Memory Card
    for (int slotNumber = 0; slotNumber < PS1CARD_MAX_SLOTS; slotNumber++)
    {
        //Clear existing data
        memset(ps1saves[slotNumber].iconPalette, 0, sizeof(uint32_t)*16);

        //Reset color counter
        colorCounter = 0;

        //Fetch two bytes at a time
        for (int byteCount = 0; byteCount < 32; byteCount += 2)
        {
            redChannel = (ps1saves[slotNumber].saveData[byteCount + 96] & 0x1F) << 3;
            greenChannel = ((ps1saves[slotNumber].saveData[byteCount + 97] & 0x3) << 6) | ((ps1saves[slotNumber].saveData[byteCount + 96] & 0xE0) >> 2);
            blueChannel = ((ps1saves[slotNumber].saveData[byteCount + 97] & 0x7C) << 1);
            blackFlag = (ps1saves[slotNumber].saveData[byteCount + 97] & 0x80);
            
            //Get the color value
            if ((redChannel | greenChannel | blueChannel | blackFlag) == 0)
                ps1saves[slotNumber].iconPalette[colorCounter] = 0x00000000;
            else
                ps1saves[slotNumber].iconPalette[colorCounter] = (redChannel << 24) | (greenChannel << 16) | (blueChannel << 8) | 0xFF;

            colorCounter++;
        }
    }
}

//Load the icons
static void loadIcons(void)
{
    int byteCount = 0;

    //Cycle through each slot
    for (int slotNumber = 0; slotNumber < PS1CARD_MAX_SLOTS; slotNumber++)
    {
        //Clear existing data
        memset(ps1saves[slotNumber].iconData, 0, 256*3);

        //Each save has 3 icons (some are data but those will not be shown)
        for (int iconNumber = 0; iconNumber < ps1saves[slotNumber].iconFrames; iconNumber++)
        {
            byteCount = 128 * (1 + iconNumber);

            for (int y = 0; y < 16; y++)
            {
                for (int x = 0; x < 16; x += 2)
                {
                    ps1saves[slotNumber].iconData[iconNumber][y * 16 + x] = ps1saves[slotNumber].saveData[byteCount] & 0xF;
                    ps1saves[slotNumber].iconData[iconNumber][y * 16 + x + 1] = ps1saves[slotNumber].saveData[byteCount] >> 4;
                    byteCount++;
                }
            }
        }
    }
}

//Load icon frames
static void loadIconFrames(void)
{
    //Cycle through each slot
    for (int slotNumber = 0; slotNumber < PS1CARD_MAX_SLOTS; slotNumber++)
    {
        switch(ps1saves[slotNumber].saveData[2])
        {
            case 0x11:      //1 frame
                ps1saves[slotNumber].iconFrames = 1;
                break;

            case 0x12:      //2 frames
                ps1saves[slotNumber].iconFrames = 2;
                break;

            case 0x13:      //3 frames
                ps1saves[slotNumber].iconFrames = 3;
                break;

            default:        //No frames (save data is probably clean)
                ps1saves[slotNumber].iconFrames = 0;
                break;
        }
    }
}

//Recreate VGS header
static void setVGSheader(FILE* fp)
{
    uint8_t vgsHeader[64];

    memset(vgsHeader, 0, sizeof(vgsHeader));
    //Fill in the signature
    vgsHeader[0] = 0x56;       //V
    vgsHeader[1] = 0x67;       //g
    vgsHeader[2] = 0x73;       //s
    vgsHeader[3] = 0x4D;       //M

    vgsHeader[4] = 0x1;
    vgsHeader[8] = 0x1;
    vgsHeader[12] = 0x1;
    vgsHeader[17] = 0x2;

    fwrite(vgsHeader, 1, sizeof(vgsHeader), fp);
    return;
}

//Get the type of the save slots
static void loadSlotTypes(void)
{
    for (int slotNumber = 0; slotNumber < PS1CARD_MAX_SLOTS; slotNumber++)
    {
        //Clear existing data
        ps1saves[slotNumber].saveType = 0;

        switch(ps1saves[slotNumber].headerData[0])
        {
            case 0xA0:      //Formatted
                ps1saves[slotNumber].saveType = PS1BLOCK_FORMATTED;
                break;

            case 0x51:      //Initial
                ps1saves[slotNumber].saveType = PS1BLOCK_INITIAL;
                break;

            case 0x52:      //Middle link
                ps1saves[slotNumber].saveType = PS1BLOCK_MIDDLELINK;
                break;

            case 0x53:      //End link
                ps1saves[slotNumber].saveType = PS1BLOCK_ENDLINK;
                break;

            case 0xA1:      //Initial deleted
                ps1saves[slotNumber].saveType = PS1BLOCK_DELETED_INITIAL;
                break;

            case 0xA2:      //Middle link deleted
                ps1saves[slotNumber].saveType = PS1BLOCK_DELETED_MIDDLELINK;
                break;

            case 0xA3:      //End link deleted
                ps1saves[slotNumber].saveType = PS1BLOCK_DELETED_ENDLINK;
                break;

            default:        //Regular values have not been found, save is corrupted
                ps1saves[slotNumber].saveType = PS1BLOCK_CORRUPTED;
                break;
        }
    }
}

//Load Save name, Product code and Identifier from the header data
static void loadStringData(void)
{
    for (int slotNumber = 0; slotNumber < PS1CARD_MAX_SLOTS; slotNumber++)
    {
        //Clear existing data
        memset(ps1saves[slotNumber].saveProdCode, 0, 11);
        memset(ps1saves[slotNumber].saveIdentifier, 0, 9);
        memset(ps1saves[slotNumber].saveName, 0, 21);
        memset(ps1saves[slotNumber].saveTitle, 0, 65);

        //Copy Product code
        strncpy(ps1saves[slotNumber].saveProdCode, (char*) &ps1saves[slotNumber].headerData[12], 10);

        //Copy Identifier
        strncpy(ps1saves[slotNumber].saveIdentifier, (char*) &ps1saves[slotNumber].headerData[22], 8);

        //Copy Save filename
        strncpy(ps1saves[slotNumber].saveName, (char*) &ps1saves[slotNumber].headerData[10], 20);

        //Copy save title from Shift-JIS
        memcpy(ps1saves[slotNumber].saveTitle, &ps1saves[slotNumber].saveData[4], 64);
    }
}

//Load size of each slot in Bytes
static void loadSaveSize(void)
{
    //Fill data for each slot
    for (int slotNumber = 0; slotNumber < PS1CARD_MAX_SLOTS; slotNumber++)
        ps1saves[slotNumber].saveSize = (ps1saves[slotNumber].headerData[4] | (ps1saves[slotNumber].headerData[5]<<8) | (ps1saves[slotNumber].headerData[6]<<16));
}

//Find and return all save links
static int findSaveLinks(int initialSlotNumber, int* tempSlotList)
{
    int j = 0;
    int currentSlot = initialSlotNumber;

    //Maximum number of cycles is 15
    for (int i = 0; i < PS1CARD_MAX_SLOTS; i++)
    {
        //Add current slot to the list
        tempSlotList[j++] = currentSlot;

        //Check if next slot pointer overflows
        if (currentSlot > PS1CARD_MAX_SLOTS) break;

        //Check if current slot is corrupted
        if (ps1saves[currentSlot].saveType == PS1BLOCK_CORRUPTED) break;

        //Check if pointer points to the next save
        if (ps1saves[currentSlot].headerData[8] == 0xFF) break;
        else currentSlot = ps1saves[currentSlot].headerData[8];
    }

    //Return int array
    return j;
}

//Toggle deleted/undeleted status
void toggleDeleteSave(int slotNumber)
{
    //Get all linked saves
    int saveSlots_Length;
    int saveSlots[PS1CARD_MAX_SLOTS];
    
    saveSlots_Length = findSaveLinks(slotNumber, saveSlots);

    //Cycle through each slot
    for (int i = 0; i < saveSlots_Length; i++)
    {
        //Check the save type
        switch (ps1saves[saveSlots[i]].saveType)
        {
            //Regular save
            case PS1BLOCK_INITIAL:
                ps1saves[saveSlots[i]].headerData[0] = 0xA1;
                break;

            //Middle link
            case PS1BLOCK_MIDDLELINK:
                ps1saves[saveSlots[i]].headerData[0] = 0xA2;
                break;

            //End link
            case PS1BLOCK_ENDLINK:
                ps1saves[saveSlots[i]].headerData[0] = 0xA3;
                break;

            //Regular deleted save
            case PS1BLOCK_DELETED_INITIAL:
                ps1saves[saveSlots[i]].headerData[0] = 0x51;
                break;

            //Middle link deleted
            case PS1BLOCK_DELETED_MIDDLELINK:
                ps1saves[saveSlots[i]].headerData[0] = 0x52;
                break;

            //End link deleted
            case PS1BLOCK_DELETED_ENDLINK:
                ps1saves[saveSlots[i]].headerData[0] = 0x53;
                break;

            //Slot should not be deleted
            default:
                break;
        }
    }

    //Reload data
    calculateXOR();
    loadSlotTypes();

    //Memory Card is changed
    changedFlag = true;
}

//Format a specified slot (Data MUST be reloaded after the use of this function)
static void formatSlot(int slotNumber)
{
    //Clear headerData
    memset(ps1saves[slotNumber].headerData, 0, PS1CARD_HEADER_SIZE);

    //Clear saveData
    memset(ps1saves[slotNumber].saveData, 0, PS1CARD_BLOCK_SIZE);

    //Clear GME comment for selected slot
    memset(saveComments[slotNumber], 0, 256);

    //Place default values in headerData
    ps1saves[slotNumber].headerData[0] = 0xA0;
    ps1saves[slotNumber].headerData[8] = 0xFF;
    ps1saves[slotNumber].headerData[9] = 0xFF;
}

//Format save
int formatSave(int slotNumber)
{
    //Get all linked saves
    int saveSlots_Length;
    int saveSlots[PS1CARD_MAX_SLOTS];
    
    saveSlots_Length = findSaveLinks(slotNumber, saveSlots);

    if (!saveSlots_Length)
        return false;

    //Cycle through each slot
    for (int i = 0; i < saveSlots_Length; i++)
    {
        formatSlot(saveSlots[i]);
    }

    //Reload data
    calculateXOR();
    loadStringData();
    loadSlotTypes();
    loadRegion();
    loadSaveSize();
    loadPalette();
    loadIconFrames();
    loadIcons();

    //Set changedFlag to edited
    changedFlag = true;
    return true;
}

//Find and return continuous free slots
static int findFreeSlots(int slotCount, int* tempSlotList)
{
    //Cycle through available slots
    for (int j, slotNumber = 0; slotNumber < PS1CARD_MAX_SLOTS; slotNumber++)
    {
        j = 0;
        for (int i = slotNumber; i < (slotNumber + slotCount); i++)
        {
            if (ps1saves[i].saveType == PS1BLOCK_FORMATTED) tempSlotList[j++]=i;
            else break;

            //Exit if next save would be over the limit of 15
            if (slotNumber + slotCount > PS1CARD_MAX_SLOTS) break;
        }

        if (j >= slotCount)
            return j;
    }

    return 0;
}

//Return all bytes of the specified save
uint8_t* getSaveBytes(int slotNumber, uint32_t* saveLen)
{
    //Get all linked saves
    int saveSlots_Length;
    int saveSlots[PS1CARD_MAX_SLOTS];
    uint8_t* saveBytes;

    saveSlots_Length = findSaveLinks(slotNumber, saveSlots);

    //Calculate the number of bytes needed to store the save
    *saveLen = PS1CARD_HEADER_SIZE + (saveSlots_Length * PS1CARD_BLOCK_SIZE);
    saveBytes = malloc(*saveLen);

    //Copy save header
    memcpy(saveBytes, ps1saves[saveSlots[0]].headerData, PS1CARD_HEADER_SIZE);

    //Copy save data
    for (int sNumber = 0; sNumber < saveSlots_Length; sNumber++)
        memcpy(&saveBytes[PS1CARD_HEADER_SIZE + (sNumber * PS1CARD_BLOCK_SIZE)], ps1saves[saveSlots[sNumber]].saveData, PS1CARD_BLOCK_SIZE);

    //Return save bytes
    return saveBytes;
}

//Input given bytes back to the Memory Card
int setSaveBytes(const uint8_t* saveBytes, int saveBytes_Length, int* reqSlots)
{
    //Number of slots to set
    int freeSlots_Length;
    int freeSlots[PS1CARD_MAX_SLOTS];
    int slotCount = (saveBytes_Length - PS1CARD_HEADER_SIZE) / PS1CARD_BLOCK_SIZE;
    int numberOfBytes = slotCount * PS1CARD_BLOCK_SIZE;

    *reqSlots = slotCount;
    freeSlots_Length = findFreeSlots(slotCount, freeSlots);

    //Check if there are enough free slots for the operation
    if (freeSlots_Length < slotCount) return false;

    //Place header data
    memcpy(ps1saves[freeSlots[0]].headerData, saveBytes, PS1CARD_HEADER_SIZE);

    //Place save size in the header
    ps1saves[freeSlots[0]].headerData[4] = (uint8_t)(numberOfBytes & 0xFF);
    ps1saves[freeSlots[0]].headerData[5] = (uint8_t)((numberOfBytes & 0xFF00) >> 8);
    ps1saves[freeSlots[0]].headerData[6] = (uint8_t)((numberOfBytes & 0xFF0000) >> 16);

    //Place save data(cycle through each save)
    for (int i = 0; i < slotCount; i++)
        //Set all bytes
        memcpy(ps1saves[freeSlots[i]].saveData, &saveBytes[PS1CARD_HEADER_SIZE + (i * PS1CARD_BLOCK_SIZE)], PS1CARD_BLOCK_SIZE);

    //Recreate header data
    //Set pointer to all slots except the last
    for (int i = 0; i < (freeSlots_Length - 1); i++)
    {
        ps1saves[freeSlots[i]].headerData[0] = 0x52;
        ps1saves[freeSlots[i]].headerData[8] = (uint8_t)freeSlots[i + 1];
        ps1saves[freeSlots[i]].headerData[9] = 0x00;
    }

    //Add final slot pointer to the last slot in the link
    ps1saves[freeSlots[freeSlots_Length - 1]].headerData[0] = 0x53;
    ps1saves[freeSlots[freeSlots_Length - 1]].headerData[8] = 0xFF;
    ps1saves[freeSlots[freeSlots_Length - 1]].headerData[9] = 0xFF;

    //Add initial saveType to the first slot
    ps1saves[freeSlots[0]].headerData[0] = 0x51;

    //Reload data
    calculateXOR();
    loadStringData();
    loadSlotTypes();
    loadRegion();
    loadSaveSize();
    loadPalette();
    loadIconFrames();
    loadIcons();

    //Set changedFlag to edited
    changedFlag = true;

    return true;
}

//Get icon data as bytes
uint8_t* getIconRGBA(int slotNumber, int frame)
{
    uint32_t* iconBytes;

    if (frame >= ps1saves[slotNumber].iconFrames)
        return NULL;

    iconBytes = malloc(16 * 16 * sizeof(uint32_t));

    //Copy bytes from the given slot
    for (int i = 0; i < 256; i++)
        iconBytes[i] = ps1saves[slotNumber].iconPalette[ps1saves[slotNumber].iconData[frame][i]];

    return (uint8_t*) iconBytes;
}

//Set icon data to saveData
void setIconBytes(int slotNumber, uint8_t* iconBytes)
{
    //Set bytes from the given slot
    memcpy(&ps1saves[slotNumber].saveData[96], iconBytes, 416);

    //Reload data
    loadPalette();
    loadIcons();

    //Set changedFlag to edited
    changedFlag = true;
}

//Format a complete Memory Card
void formatMemoryCard(void)
{
    //Format each slot in Memory Card
    for (int slotNumber = 0; slotNumber < PS1CARD_MAX_SLOTS; slotNumber++)
        formatSlot(slotNumber);

    //Reload data
    calculateXOR();
    loadStringData();
    loadSlotTypes();
    loadRegion();
    loadSaveSize();
    loadPalette();
    loadIconFrames();
    loadIcons();

    //Set changedFlag to edited
    changedFlag = true;
}

//Save single save to the given filename
int saveSingleSave(const char* fileName, int slotNumber, int singleSaveType)
{
    char psvName[0x100 + 4];
    FILE* binWriter;
    uint32_t outputData_Length;
    uint8_t* outputData;

    if (singleSaveType == PS1SAVE_PSV)
    {
        snprintf(psvName, sizeof(psvName), "%s%c%c%s", fileName, ps1saves[slotNumber].saveRegion & 0xFF, ps1saves[slotNumber].saveRegion >> 8, ps1saves[slotNumber].saveProdCode);
        for(char *c = ps1saves[slotNumber].saveIdentifier; *c; c++)
        {
            snprintf(&psvName[0x100], 4, "%02X", *c);
            strcat(psvName, &psvName[0x100]);
        }
        strcat(psvName, ".PSV");
        fileName = psvName;
    }

    //Check if the file is allowed to be opened for writing
    binWriter = fopen(fileName, "wb");
    if (!binWriter)
        return false;

    outputData = getSaveBytes(slotNumber, &outputData_Length);

    //Check what kind of file to output according to singleSaveType
    switch (singleSaveType)
    {
        //Action Replay single save
        case PS1SAVE_AR:
            setArHeader(ps1saves[slotNumber].saveName, binWriter);
            break;

        //MCS single save
        case PS1SAVE_MCS:
            fwrite(outputData, 1, PS1CARD_HEADER_SIZE, binWriter);
            break;

        //RAW single save
        case PS1SAVE_RAW:
            break;

        //PS3 unsigned save
        case PS1SAVE_PSV:
            setPsvHeader(ps1saves[slotNumber].saveName, outputData_Length - PS1CARD_HEADER_SIZE, binWriter);
            break;
    }

    fwrite(outputData + PS1CARD_HEADER_SIZE, 1, outputData_Length - PS1CARD_HEADER_SIZE, binWriter);

    //File is sucesfully saved, close the stream
    fclose(binWriter);
    free(outputData);

    if (singleSaveType == PS1SAVE_PSV)
        return psv_resign(fileName);

    return true;
}

//Import single save to the Memory Card
int openSingleSave(const char* fileName, int* requiredSlots)
{
    uint8_t* inputData;
    uint8_t* finalData;
    int finalData_Length;
    size_t inputData_Length;

    *requiredSlots = -1;

    //Check if the file is allowed to be opened
    //Put data into temp array
    if (read_buffer(fileName, &inputData, &inputData_Length) < 0)
    {
        return false;
    }

    //Check the format of the save and if it's supported load it
    //MCS single save
    if (inputData[0] == 'Q')
    {
        finalData_Length = inputData_Length;
        finalData = malloc(finalData_Length);

        memcpy(finalData, inputData, finalData_Length);
    }
    //RAW single save
    //Also valid as seen with MMX4 save
    else if (toupper(inputData[0]) == 'S' && toupper(inputData[1]) == 'C')
    {
        finalData_Length = inputData_Length + PS1CARD_HEADER_SIZE;
        finalData = calloc(1, finalData_Length);

        const char* singleSaveName = strrchr(fileName, '/');
        singleSaveName = singleSaveName ? singleSaveName + 1 : fileName;

        //Recreate save header
        finalData[0] = 0x51;        //Q
        strncpy((char*) &finalData[10], singleSaveName, 20);

        //Copy save data
        memcpy(&finalData[PS1CARD_HEADER_SIZE], inputData, inputData_Length);
    }
    //PSV single save (PS3 virtual save)
    else if (memcmp(inputData, "\0VSP", 4) == 0 && inputData[60] == 1)
    {
        // Check if this is a PS1 type save
        finalData_Length = inputData_Length - 4;
        finalData = calloc(1, finalData_Length);

        //Recreate save header
        finalData[0] = 0x51;        //Q
        memcpy(&finalData[10], &inputData[100], 20);

        //Copy save data
        memcpy(&finalData[PS1CARD_HEADER_SIZE], &inputData[132], inputData_Length - 132);
    }
    //Action Replay single save
    //Check if this is really an AR save (check for SC magic)
    else if (inputData[0x36] == 'S' && inputData[0x37] == 'C')
    {
        finalData_Length = inputData_Length + 74;
        finalData = calloc(1, finalData_Length);

        //Recreate save header
        finalData[0] = 0x51;        //Q
        memcpy(&finalData[10], inputData, 20);

        //Copy save data
        memcpy(&finalData[PS1CARD_HEADER_SIZE], &inputData[54], inputData_Length - 54);
    }
    else
    {
        free(inputData);
        return false;
    }

    //If save already exists, remove it (overwrite)
    for (int i = 0; i < PS1CARD_MAX_SLOTS; i++)
        if (strncmp(ps1saves[i].saveName, (char*) &finalData[10], 20) == 0)
        {
            formatSave(i);
            break;
        }

    //Import the save to Memory Card
    if (setSaveBytes(finalData, finalData_Length, requiredSlots))
    {
        free(finalData);
        free(inputData);
        return true;
    }

    free(finalData);
    free(inputData);
    return (false);
}

//Save Memory Card to the given filename
int saveMemoryCard(const char* fileName, int memoryCardType, int fixData)
{
    FILE* binWriter = NULL;

    if (!changedFlag && !memoryCardType)
        return false;

    binWriter = fopen(fileName, "wb");
    //Check if the file is allowed to be opened for writing
    if (!binWriter)
        return false;

    //Save as original format if memoryCardType is not set
    if (!memoryCardType)
        memoryCardType = cardType;

    //Prepare data for saving
    loadDataToRawCard(fixData);

    //Check what kind of file to output according to memoryCardType
    switch (memoryCardType)
    {
        //GME Memory Card
        case PS1CARD_GME:
            fillGmeHeader(binWriter);
            fwrite(rawMemoryCard, 1, PS1CARD_SIZE, binWriter);
            break;

        //VGS Memory Card
        case PS1CARD_VGS:
            setVGSheader(binWriter);
            fwrite(rawMemoryCard, 1, PS1CARD_SIZE, binWriter);
            break;

        //VMP Memory Card
        case PS1CARD_VMP:
            setVmpCardHeader(binWriter);
            fwrite(rawMemoryCard, 1, PS1CARD_SIZE, binWriter);
            break;

        //MCX Memory Card
        case PS1CARD_MCX:
            MakeMcxCard(rawMemoryCard, binWriter);
            break;

        //Raw Memory Card
        default:
            fwrite(rawMemoryCard, 1, PS1CARD_SIZE, binWriter);
            break;
    }

    //Set changedFlag to saved
    changedFlag = false;

    //File is sucesfully saved, close the stream
    fclose(binWriter);

    if (memoryCardType == PS1CARD_VMP)
        return vmp_resign(fileName);

    return true;
}

//Save (export) Memory Card to a given byte stream
uint8_t* saveMemoryCardStream(int fixData)
{
    //Prepare data for saving
    loadDataToRawCard(fixData);

    //Return complete Memory Card data
    return rawMemoryCard;
}

//Get Memory Card data and free slots
ps1mcData_t* getMemoryCardData(void)
{
    if (cardType == PS1CARD_NULL)
        return NULL;

    //Return Memory Card data
    return ps1saves;
}

//Open memory card from the given byte stream
int openMemoryCardStream(const uint8_t* memCardData, int dataSize, int fixData)
{
    if (!memCardData || (dataSize != PS1CARD_SIZE))
        return false;

    //Set the reference for the recieved data
    memcpy(rawMemoryCard, memCardData, sizeof(rawMemoryCard));

    //Load Memory Card data from raw card
    loadDataFromRawCard();

    if(fixData) calculateXOR();
    loadStringData();
    loadSlotTypes();
    loadRegion();
    loadSaveSize();
    loadPalette();
    loadIconFrames();
    loadIcons();

    //Since the stream is of the unknown origin Memory Card is treated as edited
    changedFlag = true;

    return true;
}

//Open Memory Card from the given filename
int openMemoryCard(const char* fileName, int fixData)
{
    cardType = PS1CARD_NULL;
    changedFlag = fixData;

    //Check if the Memory Card should be opened or created
    if (fileName != NULL)
    {
        uint8_t *tempData;
        int startOffset;
        size_t fileSize;

        //File cannot be opened, return error message
        if (read_buffer(fileName, &tempData, &fileSize) < 0)
            return false;

        if (fileSize < PS1CARD_SIZE)
        {
            free(tempData);
            return false;
        }

        //Check the format of the card and if it's supported load it
        //Standard raw Memory Card
        if (memcmp(tempData, "MC", 2) == 0)
        {
            startOffset = 0;
            cardType = PS1CARD_RAW;
        }
        //DexDrive GME Memory Card
        else if (fileSize == 0x20F40 && memcmp(tempData, "123-456-STD", 11) == 0)
        {
            startOffset = 3904;
            cardType = PS1CARD_GME;

            //Copy comments from GME header
            for (int i = 0; i < PS1CARD_MAX_SLOTS; i++)
                memcpy(saveComments[i], &tempData[64 + (256 * i)], 256);
        }
        //VGS Memory Card
        else if (fileSize == 0x20040 && memcmp(tempData, "VgsM", 4) == 0)
        {
            startOffset = 64;
            cardType = PS1CARD_VGS;
        }
        //PSP virtual Memory Card
        else if (fileSize == 0x20080 && memcmp(tempData, "\0PMV", 4) == 0)
        {
            startOffset = 128;
            cardType = PS1CARD_VMP;
        }
        //PS Vita MCX PocketStation Memory Card
        else if (fileSize == 0x200A0 && IsMcxCard(tempData))
        {
            startOffset = 128;
            cardType = PS1CARD_MCX;
        }
        //File type is not supported
        else
        {
            free(tempData);
            return false;
        }

        //Copy data to rawMemoryCard array with offset from input data
        memcpy(rawMemoryCard, tempData + startOffset, PS1CARD_SIZE);
        free(tempData);

        //Load Memory Card data from raw card
        loadDataFromRawCard();
    }
    // Memory Card should be created
    else
    {
        loadDataToRawCard(true);
        formatMemoryCard();
    }

    //Calculate XOR checksum (in case if any of the saveHeaders have corrputed XOR)
    if(fixData) calculateXOR();

    //Convert various Memory Card data to strings
    loadStringData();

    //Load slot descriptions (types)
    loadSlotTypes();

    //Load region data
    loadRegion();

    //Load size data
    loadSaveSize();

    //Load icon palette data as Color values
    loadPalette();

    //Load number of frames
    loadIconFrames();

    //Load icon data to bitmaps
    loadIcons();

    //Everything went well, no error messages
    return cardType;
}
