#include "types.h"

#define FAKE_ACCOUNT_ID(uid)			(0x6F6C6C6F70610000 + ((uid & 0xFFFF) ^ 0xFFFF))

#define XREGISTRY_FILE 					"/dev_flash2/etc/xRegistry.sys"
#define XREG_SETTING_AUTOSIGN 			"/setting/user/%08d/npaccount/autoSignInEnable"
#define XREG_SETTING_ACCOUNTID 			"/setting/user/%08d/npaccount/accountid"

struct rif
{
    u32 version;
    u32 licenseType;
    u64 accountid;
    char titleid[0x30]; //Content ID
    u8 padding[0xC]; //Padding for randomness
    u32 actDatIndex; //Key index on act.dat between 0x00 and 0x7F
    u8 key[0x10]; //encrypted klicensee
    u64 timestamp; //timestamp??
    u64 expiration; //Always 0
    u8 r[0x14];
    u8 s[0x14];
} __attribute__ ((packed));

struct actdat
{
    u32 version;
    u32 licenseType;
    u64 accountId;
    u8 keyTable[0x800]; //Key Table
    u8 unk2[0x800];
    u8 sig_r[0x14];
    u8 sig_s[0x14];
} __attribute__ ((packed));

typedef struct rif rif_t;
typedef struct actdat actdat_t;

int ecdsa_set_curve(u32 type);
void ecdsa_set_pub(u8 *Q);
void ecdsa_set_priv(u8 *k);
void ecdsa_sign(u8 *hash, u8 *R, u8 *S);

void get_rand(u8 *dst, u32 len);
