#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <libjbc.h>

#include "util.h"
#include "sd.h"


asm("orbis_syscall:\n"
    "movq $0, %rax\n"
    "movq %rcx, %r10\n"
    "syscall\n"
    "jb err\n"
    "retq\n"
"err:\n"
    "pushq %rax\n"
    "callq __error\n"
    "popq %rcx\n"
    "movl %ecx, 0(%rax)\n"
    "movq $0xFFFFFFFFFFFFFFFF, %rax\n"
    "movq $0xFFFFFFFFFFFFFFFF, %rdx\n"
    "retq\n");
int orbis_syscall(int num, ...);

int sysKernelGetLowerLimitUpdVersion(int* unk);
int sysKernelGetUpdVersion(int* unk);


static int sys_mknod(const char *path, mode_t mode, dev_t dev)
{
    int result;
    int err;

    asm volatile(
    ".intel_syntax;"    // Switches the assembly syntax to Intel syntax
    "mov rax, 14;"      // Moves the value 14 into the RAX register (which typically holds the system call number)
    "mov r10, rcx;"     // Moves the value of the RCX register into the R10 register
    "syscall;"          // Executes a system call using the values in the registers
    : "=a"(result),     // Output constraint: Tells the compiler that the result of the operation will be stored in the RAX register
    "=@ccc"(err)        // Output constraint: Indicates that error information will be stored in the specified location
    );

    UNUSED(path);
    UNUSED(mode);
    UNUSED(dev);

    return result;
}

// cred must be set to invoke mount call, use before anything
static int init_cred(void)
{
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
/*
static int setup_cred(void) {
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
*/

// create devices, do once after setting cred and loading priv libs
static int init_devices(void)
{
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

int initVshDataMount(void)
{
    LOG("Loading libSceFsInternalForVsh.sprx...");

    // initialize cred
    if (init_cred() != 0) {
        LOG("Failed to init cred");
        return(0);
    }

    // load private libraries, do once after setting cred
    if (loadPrivLibs() != 0) {
        LOG("Failed to load priv libs");
        return(0);
    }

    // create devices
    if (init_devices() != 0) {
        LOG("Failed to create devices");
        return(0);
    }

    /*if (setup_cred() != 0) {
        LOG(0, "Failed to setup cred\n");
        return(0);
    }*/

    LOG("VSH data mount initialized!");

    return 1;
}

int get_firmware_version(void)
{
    int fw;

    // upd_version >> 16
    // 0x505  0x672  0x702  0x755  etc
    if (sysKernelGetUpdVersion(&fw) && sysKernelGetLowerLimitUpdVersion(&fw))
    {
        LOG("Error: can't detect firmware version!");
        return (-1);
    }

    LOG("[i] PS4 Firmware %X", fw >> 16);
    return (fw >> 16);
}

int get_max_pfskey_ver(void)
{
	int fw = get_firmware_version();

	if (fw >= 0x800) return 10;
	if (fw >= 0x750) return 9;
	if (fw >= 0x700) return 8;
	if (fw >= 0x650) return 7;
	if (fw >= 0x600) return 6;
	if (fw >= 0x550) return 5;
	if (fw >= 0x500) return 4;
	if (fw >= 0x470) return 3;
	if (fw >= 0x450) return 2;
	if (fw >= 0x100) return 1;
	return 0;
}

const char* get_fw_by_pfskey_ver(int key_ver)
{
	const char* fw[] = {"???", "Any", "4.50+", "4.70+", "5.00+", "5.50+", "6.00+", "6.50+", "7.00+", "7.50+", "8.00+"};
	if (key_ver > 10) key_ver = 0;

	return fw[key_ver];
}

// Variables for (un)jailbreaking
static jbc_cred g_Cred;
static jbc_cred g_RootCreds;

// Verify jailbreak
static int is_jailbroken(void)
{
    FILE *s_FilePointer = fopen("/user/.jailbreak", "w");

    if (!s_FilePointer)
        return 0;

    fclose(s_FilePointer);
    remove("/user/.jailbreak");
    return 1;
}

// Jailbreaks creds
static int jailbreak(void)
{
    if (is_jailbroken())
        return 1;

    jbc_get_cred(&g_Cred);
    g_RootCreds = g_Cred;
    jbc_jailbreak_cred(&g_RootCreds);
    jbc_set_cred(&g_RootCreds);

    return (is_jailbroken());
}

// Initialize jailbreak
int initialize_jbc(void)
{
    // Pop notification depending on jailbreak result
    if (!jailbreak())
    {
        LOG("Jailbreak failed!");
        notify_popup(NOTIFICATION_ICON_BAN, "Jailbreak failed!");
        return 0;
    }

    LOG("Jailbreak succeeded!");
    return 1;
}

// Unload libjbc libraries
void terminate_jbc(void)
{
    if (!is_jailbroken())
        return;

    // Restores original creds
    jbc_set_cred(&g_Cred);
    LOG("Jailbreak removed!");
}
