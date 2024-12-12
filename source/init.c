#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <libjbc.h>
#include <dbglogger.h>
#define LOG dbglogger_log

#include "init.h"
#include "sd.h"
#include "scall.h"

// cred must be set to invoke mount call, use before anything
int init_cred(void) {
    jbc_cred old_cred;
    jbc_cred cred;

    memset(&old_cred, 0, sizeof(jbc_cred));
    memset(&cred, 0, sizeof(jbc_cred));

    if (jbc_get_cred(&old_cred) != 0) {
        return -1;
    }
    old_cred.sonyCred = old_cred.sonyCred | 0x4000000000000000ULL;

    if (jbc_get_cred(&cred) != 0) {
        return -2;
    }
    cred.sonyCred = cred.sonyCred | 0x4000000000000000ULL;
    cred.sceProcType = 0x3801000000000013ULL;

    if (jbc_set_cred(&cred) != 0) {
        return -3;
    }
    setuid(0);

    return 0;
}

// can use before mount call after initializing everything
int setup_cred(void) {
    jbc_cred cred;

    memset(&cred, 0, sizeof(jbc_cred));
    if (jbc_get_cred(&cred) != 0) {
        return -1;
    }
    cred.sonyCred = cred.sonyCred | 0x4000000000000000ULL;
    cred.sceProcType = 0x3801000000000013ULL;

    if (jbc_set_cred(&cred) != 0) {
        return -2;
    }
    setuid(0);

    return 0;
}

// create devices, do once after setting cred and loading priv libs
int init_devices(void) {
    struct stat s;
    memset(&s, 0, sizeof(struct stat));

    // create devices
    if (stat("/dev/pfsctldev", &s) == -1) {
        LOG("err stat pfsctldev");
        return -2;
    }
    else {
        if (sys_mknod("/dev/pfsctldev", S_IFCHR | 0777, s.st_dev) == -1) {
            LOG("err mknod pfsctldev");
            return -2;
        }
    }

    memset(&s, 0, sizeof(struct stat));

    if (stat("/dev/lvdctl", &s) == -1) {
        LOG("err stat lvdctl");
        return -3;
    }
    else {
        if (sys_mknod("/dev/lvdctl", S_IFCHR | 0777, s.st_dev) == -1) {
            LOG("err mknod lvdctl");
            return -3;
        }
    }

    memset(&s, 0, sizeof(struct stat));

    if (stat("/dev/sbl_srv", &s) == -1) {
        LOG("err stat sbl_srv");
        return -4;
    }
    else {
        if (sys_mknod("/dev/sbl_srv", S_IFCHR | 0777, s.st_dev) == -1) {
            LOG("err mknod sbl_srv");
            return -4;
        }
    }

    // get max keyset that can be decrypted
    getMaxKeySet();

    return 0;
}
