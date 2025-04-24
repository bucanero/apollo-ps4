#ifndef SD_H
#define SD_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENC_SEALEDKEY_LEN 0x60
#define DEC_SEALEDKEY_LEN 0x20

#define UNUSED(x) (void)(x)

typedef struct {
    int blockSize;
    uint8_t idk[2];
} CreatePfsSaveDataOpt;

typedef struct {
    bool readOnly;
    char *budgetid;
} MountSaveDataOpt;

typedef struct {
    bool dummy;
} UmountSaveDataOpt;

int loadPrivLibs(void);
int generateSealedKey(uint8_t data[ENC_SEALEDKEY_LEN]);
int decryptSealedKey(const uint8_t enc_key[ENC_SEALEDKEY_LEN], uint8_t dec_key[DEC_SEALEDKEY_LEN]);
int decryptSealedKeyAtPath(const char *keyPath, uint8_t decryptedSealedKey[DEC_SEALEDKEY_LEN]);
int createSave(const char *folder, const char *saveName, int blocks);
int mountSave(const char *folder, const char *saveName, const char *mountPath);
int umountSave(const char *mountPath, int handle, bool ignoreErrors);
uint16_t getMaxKeySet(void);

#ifdef __cplusplus
}
#endif

#endif // SD_H
