/*
*
* Trophy data structures used in TROPUSR.DAT, TROPTRANS.DAT
*  - based on: https://github.com/darkautism/TROPHYParser/
*
*/

#define TROP_STATE_SYNCED       0x010000
#define TROP_STATE_UNSYNC       0x001000
#define TROP_ACCOUNT_ID_SIZE    16

typedef struct {
    uint64_t magic;
    uint32_t recordCount;
    char padding[36];
} tropHeader_t;

typedef struct {
    uint32_t id;
    uint32_t size;
    uint32_t _unknown3;
    uint32_t usedTimes;
    uint64_t offset;
    uint32_t _unknown6;
} tropRecord_t;

typedef struct {
    int type;
    int blocksize;
    int sequenceNumber; // if have more than same type block, it will be used
    int unknown;
} tropBlockHeader_t;

typedef struct {
    tropBlockHeader_t header;

    uint32_t sequenceNumber;
    uint32_t type;
    char unknown[16];
    char padding[56];
} tropType_t;

typedef struct {
    uint64_t dt1;
    uint64_t dt2;
} tropDateTime_t;

typedef struct {
    tropBlockHeader_t header;

    char padding[16];
    tropDateTime_t listCreateTime;
    tropDateTime_t listLastUpdateTime;
    char padding2[16];
    tropDateTime_t listLastGetTrophyTime;
    char padding3[32];
    uint32_t getTrophyNumber;
    char padding4[12];
    uint32_t AchievementRate[4];
    char padding5[16];
    uint8_t hash[16];
    char padding6[32];
} tropListInfo_t;

typedef struct {
    tropBlockHeader_t header;

    uint32_t sequenceNumber;
    uint32_t unlocked;
    uint32_t syncState;
    uint32_t unknown2;
    tropDateTime_t unlockTime;
    char padding[64];
} tropTimeInfo_t;

typedef struct {
    tropBlockHeader_t header;

    uint32_t getTrophyCount;
    uint32_t syncTrophyCount;
    uint32_t u3;
    uint32_t u4;
    uint64_t lastSyncTime;
    char padding[8];
    char padding2[48];
} tropUnkType7_t;

typedef struct {
    tropBlockHeader_t header;

    uint32_t sequenceNumber;    // 0
    uint32_t isExist;
    uint32_t syncState;
    uint32_t _unknowInt1;       // 0
    char padding[16];
    uint32_t trophyID;
    uint32_t trophyType;
    uint32_t _unknowInt2;       // 0x00001000
    uint32_t _unknowInt3;       // 0
    tropDateTime_t getTime;
    char padding2[96];
} trnsTrophyInfo_t;

typedef struct {
    tropBlockHeader_t header;

    uint32_t u1;
    uint32_t u2;
    uint32_t u3;
    uint32_t u4;
    char padding[16];
    tropDateTime_t initTime;
    char padding2[112];
} trnsInitTime_t;

typedef struct {
    char* account_id;
    char* npcomm_id;
    uint32_t* allTrophyNumber;
    uint32_t* achievementRate;
    tropType_t* trophyTypeTable;
    tropListInfo_t* trophyListInfo;
    tropTimeInfo_t* trophyTimeInfoTable;
    tropUnkType7_t* unknownType7;
    char* unknownHash;
} usrTrophy_t;

typedef struct {
    char* account_id;
    char* npcomm_id;
    uint32_t* allGetTrophysCount;
    uint32_t* allSyncPSNTrophyCount;
    trnsInitTime_t* trophyInitTime;
    trnsTrophyInfo_t* trophyInfoTable;

} trnsTrophy_t;

tropTimeInfo_t* getTrophyTimeInfo(const uint8_t* data);
