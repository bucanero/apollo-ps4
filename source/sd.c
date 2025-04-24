#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <orbis/libkernel.h>
#include <libjbc.h>
#include <dbglogger.h>
#define LOG dbglogger_log

#include "sd.h"

int (*sceFsUfsAllocateSaveData)(int fd, uint64_t imageSize, uint64_t imageFlags, int ext);
int (*sceFsInitCreatePfsSaveDataOpt)(CreatePfsSaveDataOpt *opt);
int (*sceFsCreatePfsSaveDataImage)(CreatePfsSaveDataOpt *opt, const char *volumePath, int idk, uint64_t volumeSize, uint8_t decryptedSealedKey[DEC_SEALEDKEY_LEN]);
int (*sceFsInitMountSaveDataOpt)(MountSaveDataOpt *opt);
int (*sceFsMountSaveData)(MountSaveDataOpt *opt, const char *volumePath, const char *mountPath, uint8_t decryptedSealedKey[DEC_SEALEDKEY_LEN]);
int (*sceFsInitUmountSaveDataOpt)(UmountSaveDataOpt *opt);
int (*sceFsUmountSaveData)(UmountSaveDataOpt *opt, const char *mountPath, int handle, bool ignoreErrors);
void (*statfs)(void);

// must be loaded upon setup
int loadPrivLibs(void) {
    int sys;
    int kernel_sys;

    sys = sceKernelLoadStartModule("/system/priv/lib/libSceFsInternalForVsh.sprx", 0, NULL, 0, NULL, NULL);
    if (sys >= 0) {
        sceKernelDlsym(sys, "sceFsInitCreatePfsSaveDataOpt",    (void **)&sceFsInitCreatePfsSaveDataOpt);
        sceKernelDlsym(sys, "sceFsCreatePfsSaveDataImage",      (void **)&sceFsCreatePfsSaveDataImage);
        sceKernelDlsym(sys, "sceFsUfsAllocateSaveData",         (void **)&sceFsUfsAllocateSaveData);
        sceKernelDlsym(sys, "sceFsInitMountSaveDataOpt",        (void **)&sceFsInitMountSaveDataOpt);
        sceKernelDlsym(sys, "sceFsMountSaveData",               (void **)&sceFsMountSaveData);
        sceKernelDlsym(sys, "sceFsInitUmountSaveDataOpt",       (void **)&sceFsInitUmountSaveDataOpt);
        sceKernelDlsym(sys, "sceFsUmountSaveData",              (void **)&sceFsUmountSaveData);
    }
    else {
        LOG("Failed to load libSceFsInternalForVsh.sprx");
        return -1;
    }

    kernel_sys = sceKernelLoadStartModule("/system/common/lib/libkernel_sys.sprx", 0, NULL, 0, NULL, NULL);
    if (kernel_sys >= 0) {
        sceKernelDlsym(kernel_sys, "statfs", (void **)&statfs);
    }
    else {
        LOG("Failed to load libkernel_sys.sprx");
        return -2;
    }

    return 0;
}

int generateSealedKey(uint8_t data[ENC_SEALEDKEY_LEN]) {
    uint8_t dummy[0x30];
    uint8_t sealedKey[ENC_SEALEDKEY_LEN];
    int fd;

    UNUSED(dummy);
    memset(sealedKey, 0, sizeof(sealedKey));

    fd = open("/dev/sbl_srv", O_RDWR);
    if (fd == -1) {
        LOG("sbl_srv open fail!");
        return -1;
    }

    if (ioctl(fd, 0x40845303, sealedKey) == -1) {
        close(fd);
        return -2;
    }

    memcpy(data, sealedKey, sizeof(sealedKey));
    close(fd);

    return 0;
}

int decryptSealedKey(const uint8_t enc_key[ENC_SEALEDKEY_LEN], uint8_t dec_key[DEC_SEALEDKEY_LEN]) {
    uint8_t dummy[0x10];
    uint8_t data[ENC_SEALEDKEY_LEN + DEC_SEALEDKEY_LEN];
    int fd;

    UNUSED(dummy);
    memset(data, 0, sizeof(data));

    fd = open("/dev/sbl_srv", O_RDWR);
    if (fd == -1) {
        LOG("sbl_srv open fail!");
        return -1;
    }

    memcpy(data, enc_key, ENC_SEALEDKEY_LEN);

    if (ioctl(fd, 0xc0845302, data) == -1) {
        close(fd);
        return -2;
    }

    memcpy(dec_key, &data[ENC_SEALEDKEY_LEN], DEC_SEALEDKEY_LEN);
    close(fd);

    return 0;
}

int decryptSealedKeyAtPath(const char *keyPath, uint8_t decryptedSealedKey[DEC_SEALEDKEY_LEN]) {
    uint8_t sealedKey[ENC_SEALEDKEY_LEN];
    int fd;

    fd = open(keyPath, O_RDONLY);
    if (fd == -1) {
        return -1;
    }

    if (read(fd, sealedKey, ENC_SEALEDKEY_LEN) != ENC_SEALEDKEY_LEN) {
        close(fd);
        return -2;
    }
    close(fd);

    if (decryptSealedKey(sealedKey, decryptedSealedKey) != 0) {
        return -3;
    }

    return 0;
}

int createSave(const char *volumePath, const char *volumeKeyPath, int blocks) {
    uint8_t sealedKey[ENC_SEALEDKEY_LEN];
    uint8_t decryptedSealedKey[DEC_SEALEDKEY_LEN];
    uint64_t volumeSize;
    int fd;
    CreatePfsSaveDataOpt opt;

    memset(&opt, 0, sizeof(CreatePfsSaveDataOpt));

    // generate a key
    if (generateSealedKey(sealedKey) != 0) {
        return -1;
    }

    // decrypt the generated key
    if (decryptSealedKey(sealedKey, decryptedSealedKey) != 0) {
        return -2;
    }

    fd = sceKernelOpen(volumeKeyPath, O_CREAT | O_TRUNC | O_WRONLY, 0777);
    if (fd == -1) {
        return -3;
    }

    // write sealed key
    if (sceKernelWrite(fd, sealedKey, sizeof(sealedKey)) != sizeof(sealedKey)) {
        sceKernelClose(fd);
        return -4;
    }
    sceKernelClose(fd);

    fd = sceKernelOpen(volumePath, O_CREAT | O_TRUNC | O_WRONLY, 0777);
    if (fd == -1) {
        return -5;
    }

    volumeSize = blocks << 15;

    if (sceFsUfsAllocateSaveData(fd, volumeSize, 0 << 7, 0) < 0) {
        sceKernelClose(fd);
        return -6;
    }
    sceKernelClose(fd);

    if (sceFsInitCreatePfsSaveDataOpt(&opt) < 0) {
        return -7;
    }

    if (sceFsCreatePfsSaveDataImage(&opt, volumePath, 0, volumeSize, decryptedSealedKey) < 0) {
        return -8;
    }

    // finalize
    fd = sceKernelOpen(volumePath, O_RDONLY, 0);
    sceKernelFsync(fd);
    sceKernelClose(fd);
    
    return 0;
}

int mountSave(const char *volumePath, const char *volumeKeyPath, const char *mountPath) {
    int ret;
    uint8_t decryptedSealedKey[DEC_SEALEDKEY_LEN];
    MountSaveDataOpt opt;

    memset(&opt, 0, sizeof(MountSaveDataOpt));

    ret = decryptSealedKeyAtPath(volumeKeyPath, decryptedSealedKey);
    if (ret < 0) {
        return ret;
    }

    sceFsInitMountSaveDataOpt(&opt);
    opt.budgetid = "system";

    ret = sceFsMountSaveData(&opt, volumePath, mountPath, decryptedSealedKey);
    if (ret < 0) {
        return ret;
    }

    return 0;
}

int umountSave(const char *mountPath, int handle, bool ignoreErrors) {
    UmountSaveDataOpt opt;

    memset(&opt, 0, sizeof(UmountSaveDataOpt));
    sceFsInitUmountSaveDataOpt(&opt);
    return sceFsUmountSaveData(&opt, mountPath, handle, ignoreErrors);
}

static uint16_t maxKeyset = 0;

uint16_t getMaxKeySet(void) {
    if (maxKeyset > 0) {
        return maxKeyset;
    }

    uint8_t sampleSealedKey[ENC_SEALEDKEY_LEN];
    if (generateSealedKey(sampleSealedKey) != 0) {
        return 0;
    }

    maxKeyset = (sampleSealedKey[9] << 8) + sampleSealedKey[8];
    return maxKeyset;
}
