#include <unistd.h>
#include "sd.h"

int (*sceFsUfsAllocateSaveData)(int fd, uint64_t imageSize, uint64_t imageFlags, int ext);
int (*sceFsInitCreatePfsSaveDataOpt)(CreatePfsSaveDataOpt *opt);
int (*sceFsCreatePfsSaveDataImage)(CreatePfsSaveDataOpt *opt, const char *volumePath, int idk, uint64_t volumeSize, uint8_t decryptedSealedKey[DEC_SEALEDKEY_LEN]);
int (*sceFsInitMountSaveDataOpt)(MountSaveDataOpt *opt);
int (*sceFsMountSaveData)(MountSaveDataOpt *opt, const char *volumePath, const char *mountPath, uint8_t decryptedSealedKey[DEC_SEALEDKEY_LEN]);
int (*sceFsInitUmountSaveDataOpt)(UmountSaveDataOpt *opt);
int (*sceFsUmountSaveData)(UmountSaveDataOpt *opt, const char *mountPath, int handle, bool ignoreErrors);
void (*statfs)();

// must be loaded upon setup
int loadPrivLibs(void) {
    const char privDir[] = "/system/priv/lib";
    const char commonDir[] = "/system/common/lib";
    int sys;
    int kernel_sys;

    if (jbc_mount_in_sandbox(privDir, "priv") != 0) {
        sceKernelDebugOutText(0, "Failed to mount system/priv/lib directory\n");
        return -1;
    }

    sys = sceKernelLoadStartModule("/priv/libSceFsInternalForVsh.sprx", 0, NULL, 0, NULL, NULL);
    if (jbc_unmount_in_sandbox("priv") != 0) {
        sceKernelDebugOutText(0, "Failed to unmount priv\n");
    }

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
        sceKernelDebugOutText(0, "Failed to load libSceFsInternalForVsh.sprx\n");
        return -2;
    }

    if (jbc_mount_in_sandbox(commonDir, "common") != 0) {
        sceKernelDebugOutText(0, "Failed to mount /system/common/lib directory\n");
        return -3;
    }
    kernel_sys = sceKernelLoadStartModule("/common/libkernel_sys.sprx", 0, NULL, 0, NULL, NULL);
    if (jbc_unmount_in_sandbox("common") != 0) {
        sceKernelDebugOutText(0, "Failed to unmount common\n");
    }

    if (kernel_sys >= 0) {
        sceKernelDlsym(kernel_sys, "statfs", (void **)&statfs);
    }
    else {
        sceKernelDebugOutText(0, "Failed to load libkernel_sys.sprx\n");
        return -4;
    }

    return 0;
}

int generateSealedKey(uint8_t data[ENC_SEALEDKEY_LEN]) {
    uint8_t dummy[0x30];
    uint8_t sealedKey[ENC_SEALEDKEY_LEN];
    int fd;

    UNUSED(dummy);

    memset(sealedKey, 0, sizeof(sealedKey));

    if ((fd = open("/dev/sbl_srv", O_RDWR)) == -1) {
        sceKernelDebugOutText(0, "sbl_srv open fail!\n");
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

int decryptSealedKey(uint8_t enc_key[ENC_SEALEDKEY_LEN], uint8_t dec_key[DEC_SEALEDKEY_LEN]) {
    uint8_t dummy[0x10];
    uint8_t data[ENC_SEALEDKEY_LEN + DEC_SEALEDKEY_LEN];
    int fd;

    memset(data, 0, sizeof(data));

    UNUSED(dummy);

    if ((fd = open("/dev/sbl_srv", O_RDWR)) == -1) {
        sceKernelDebugOutText(0, "sbl_srv open fail!\n");
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
//    ssize_t bytesRead;
    int fd;

    if ((fd = sys_open(keyPath, O_RDONLY, 0)) == -1) {
        return -1;
    }

    if (read(fd, sealedKey, ENC_SEALEDKEY_LEN) != ENC_SEALEDKEY_LEN) {
        return -2;
        close(fd);
    }
    close(fd);

    if (decryptSealedKey(sealedKey, decryptedSealedKey) == -1) {
        return -3;
    }

    return 0;
}

int mountSave(const char *folder, const char *saveName, const char *mountPath) {
    char volumeKeyPath[MAX_PATH_LEN];
    char volumePath[MAX_PATH_LEN];
    char bid[] = "system";
    int ret;
    uint8_t decryptedSealedKey[DEC_SEALEDKEY_LEN];
    MountSaveDataOpt opt;

    memset(&opt, 0, sizeof(MountSaveDataOpt));

    sprintf(volumeKeyPath, "%s/%s.bin", folder, saveName);
    sprintf(volumePath, "%s/%s", folder, saveName);

    if ((ret = decryptSealedKeyAtPath(volumeKeyPath, decryptedSealedKey)) < 0) {
        return ret;
    }

    sceFsInitMountSaveDataOpt(&opt);
    opt.budgetid = bid;

    if ((ret = sceFsMountSaveData(&opt, volumePath, mountPath, decryptedSealedKey)) < 0) {
        return ret;
    }

    return 0;
}

int umountSave(const char *mountPath, int handle, bool ignoreErrors) {
    UmountSaveDataOpt opt;
    sceFsInitUmountSaveDataOpt(&opt);
    return sceFsUmountSaveData(&opt, mountPath, handle, ignoreErrors);
}

uint16_t maxKeyset = 0;
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