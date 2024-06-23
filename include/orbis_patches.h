#include <stdint.h>

#define SYS_dynlib_get_info         593
#define SYS_dynlib_get_info_ex      608

#define SYS_PROC_ALLOC              1
#define SYS_PROC_FREE               2
#define SYS_PROC_PROTECT            3
#define SYS_PROC_VM_MAP             4
#define SYS_PROC_INSTALL            5
#define SYS_PROC_CALL               6
#define SYS_PROC_ELF                7
#define SYS_PROC_INFO               8
#define SYS_PROC_THRINFO            9

#define SYS_CONSOLE_CMD_REBOOT      1
#define SYS_CONSOLE_CMD_PRINT       2
#define SYS_CONSOLE_CMD_JAILBREAK   3
#define SYS_CONSOLE_CMD_ISLOADED    6

typedef void module_patch_cb_t(void* arg, uint8_t* base, uint64_t size);

// custom syscall 107
struct proc_list_entry {
    char p_comm[32];
    int pid;
}  __attribute__((packed));

struct proc_vm_map_entry {
    char name[32];
    uint64_t start;
    uint64_t end;
    uint64_t offset;
    uint16_t prot;
} __attribute__((packed));

// custom syscall 109
// SYS_PROC_VM_MAP
struct sys_proc_vm_map_args {
    struct proc_vm_map_entry *maps;
    uint64_t num;
} __attribute__((packed));

// SYS_PROC_ALLOC
struct sys_proc_alloc_args {
    uint64_t address;
    uint64_t length;
} __attribute__((packed));

// SYS_PROC_FREE
struct sys_proc_free_args {
    uint64_t address;
    uint64_t length;
} __attribute__((packed));

// SYS_PROC_PROTECT
struct sys_proc_protect_args {
    uint64_t address;
    uint64_t length;
    uint64_t prot;
} __attribute__((packed));

// SYS_PROC_INSTALL
struct sys_proc_install_args {
    uint64_t stubentryaddr;
} __attribute__((packed));

// SYS_PROC_CALL
struct sys_proc_call_args {
    uint32_t pid;
    uint64_t rpcstub;
    uint64_t rax;
    uint64_t rip;
    uint64_t rdi;
    uint64_t rsi;
    uint64_t rdx;
    uint64_t rcx;
    uint64_t r8;
    uint64_t r9;
} __attribute__((packed));

// SYS_PROC_ELF
struct sys_proc_elf_args {
    void *elf;
} __attribute__((packed));

// SYS_PROC_INFO
struct sys_proc_info_args {
    int pid;
    char name[40];
    char path[64];
    char titleid[16];
    char contentid[64];
} __attribute__((packed));

// SYS_PROC_THRINFO
struct sys_proc_thrinfo_args {
    uint32_t lwpid;
    uint32_t priority;
    char name[32];
} __attribute__((packed));

typedef struct orbis_patch {
    uint32_t offset;
    char* data;
    uint32_t size;
} orbis_patch_t;

// SAVEDATA LIBRARY PATCHES (libSceSaveData)
const orbis_patch_t scesavedata_patches_505[] = {
    {0x32998, "\x00", 1},        // 'sce_' patch
    {0x31699, "\x00", 1},        // 'sce_sdmemory' patch
    {0x01119, "\x30", 1},        // '_' patch
    {0, NULL, 0},
};

const orbis_patch_t scesavedata_patches_672[] = {
    {0x00038AE8, "\x00", 1},        // 'sce_' patch
    {0x000377D9, "\x00", 1},        // 'sce_sdmemory' patch
    {0x00000ED9, "\x30", 1},        // '_' patch
    {0, NULL, 0},
};

const orbis_patch_t scesavedata_patches_702[] = {
    {0x00036798, "\x00", 1},        // 'sce_' patch
    {0x00035479, "\x00", 1},        // 'sce_sdmemory' patch
    {0x00000E88, "\x30", 1},        // '_' patch
    {0, NULL, 0},
};

const orbis_patch_t scesavedata_patches_75x[] = {
    {0x000360E8, "\x00", 1},        // 'sce_' patch
    {0x00034989, "\x00", 1},        // 'sce_sdmemory' patch
    {0x00000E81, "\x30", 1},        // '_' patch
    {0, NULL, 0},
};

const orbis_patch_t scesavedata_patches_900[] = {
    {0x00035DA8, "\x00", 1},        // 'sce_' patch
    {0x00034679, "\x00", 1},        // 'sce_sdmemory' patch
    {0x00034609, "\x00", 1},        // by @Ctn
    {0x00036256, "\x00", 1},        // by @Ctn
    {0x00000FA1, "\x1F", 1},        // by GRModSave_Username
    {0, NULL, 0},
};

/* 11.00 WIP patches by LM and SocraticBliss */
const orbis_patch_t scesavedata_patches_1100[] = {
    {0x00355E8, "\x00", 1},        // 'sce_' patch
//  {0x0034679, "\x00", 1}, // patch commented out as idk WTF it does
    {0x0033E49, "\x00", 1},
    {0x0035AA6, "\x00", 1},
    {0x0000FB8, "\x1F", 1}, // sb
    {0, NULL, 0},
};


// SHELLCORE PATCHES (SceShellCore)

/* 5.05 patches below are taken from ChendoChap Save-Mounter */
const orbis_patch_t shellcore_patches_505[] = {
    {0xD42843, "\x00", 1},                        // 'sce_sdmemory' patch
    {0x7E4DC0, "\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x068BA0, "\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x0C54F0, "\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x06A349, "\x90\x90", 2},                    // ^
    {0x0686AE, "\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x067FCA, "\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x067798, "\x90\x90", 2},                    //nevah jump
    {0x0679D5, "\x90\xE9", 2},                    //always jump
    {0, NULL, 0}
};

/* 6.72 patches below are taken from GiantPluto Save-Mounter */
const orbis_patch_t shellcore_patches_672[] = {
    {0x01600060, "\x00", 1},                        // 'sce_sdmemory' patch
    {0x0087F840, "\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x00071130, "\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x000D6830, "\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x0007379E, "\x90\x90", 2},                    // ^
    {0x00070C38, "\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x00070855, "\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x00070054, "\x90\x90", 2},                    //nevah jump
    {0x00070260, "\x90\xE9", 2},                    //always jump
    {0, NULL, 0}
};

/* 7.02 patches below are taken from Joonie Save-Mounter */
const orbis_patch_t shellcore_patches_702[] = {
    {0x0130C060, "\x00", 1},                        // 'sce_sdmemory' patch
    {0x0083F4E0, "\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x0006D580, "\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x000CFAA0, "\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x0006FF5F, "\x90\x90", 2},                    // ^
    {0x0006D058, "\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x0006C971, "\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x0006C1A4, "\x90\x90", 2},                    //nevah jump
    {0x0006C40C, "\x90\xE9", 2},                    //always jump
    {0, NULL, 0}
};

/* 7.5x patches below are taken from Joonie Save-Mounter */
const orbis_patch_t shellcore_patches_75x[] = {
    {0x01334060, "\x00", 1},                        // 'sce_sdmemory' patch
    {0x0084A300, "\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x0006B860, "\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x000C7280, "\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x0006D26D, "\x90\x90", 2},                    // ^
    {0x0006B338, "\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x0006AC2D, "\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x0006A494, "\x90\x90", 2},                    //nevah jump
    {0x0006A6F0, "\x90\xE9", 2},                    //always jump
    {0, NULL, 0}
};

/* 9.00 patches below are taken from Graber Save-Mounter */
const orbis_patch_t shellcore_patches_900[] = {
    {0x00E351D9, "\x00", 1},                        // 'sce_sdmemory' patch
    {0x00E35218, "\x00", 1},                        // 'sce_sdmemory1' patch
    {0x00E35226, "\x00", 1},                        // 'sce_sdmemory2' patch
    {0x00E35234, "\x00", 1},                        // 'sce_sdmemory3' patch
    {0x008AEAE0, "\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x0006C560, "\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x000C9000, "\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x0006defe, "\x90\x90", 2},                    // ^ (thanks to GRModSave_Username)
    {0x0006C0A8, "\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x0006BA62, "\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x0006B2C4, "\x90\x90", 2},                    //nevah jump
    {0x0006B51E, "\x90\xE9", 2},                    //always jump
    {0, NULL, 0}
};

const orbis_patch_t shellcore_patches_960[] = {
    {0x0E1BBE9, "\x00", 1},                        // 'sce_sdmemory' patch 1
    {0x0E1BC28, "\x00", 1},                        // 'sce_sdmemory1' patch
    {0x0E1BC36, "\x00", 1},                        // 'sce_sdmemory2' patch
    {0x0E1BC44, "\x00", 1},                        // 'sce_sdmemory3' patch
    {0x08B0BC0, "\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x006B610, "\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x00C6F60, "\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x006CF85, "\x90\x90", 2},                    // ^ (thanks to GRModSave_Username)
    {0x006B157, "\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x006AB12, "\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x006A374, "\x90\x90", 2},                    //nevah jump
    {0x006A5CE, "\xE9\xC8\x00", 3},                //always jump
    {0, NULL, 0}
};

const orbis_patch_t shellcore_patches_1000[] = {
    {0x0E0FE39, "\x00", 1},                        // 'sce_sdmemory' patch 1
    {0x0E0FE78, "\x00", 1},                        // 'sce_sdmemory1' patch
    {0x0E0FE86, "\x00", 1},                        // 'sce_sdmemory2' patch
    {0x0E0FE94, "\x00", 1},                        // 'sce_sdmemory3' patch
    {0x08A7510, "\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x006B6A0, "\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x00C70A0, "\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x006D015, "\x90\x90", 2},                    // ^ (thanks to GRModSave_Username)
    {0x006B1E7, "\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x006AbA2, "\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x006A404, "\x90\x90", 2},                    //nevah jump
    {0x006A65E, "\xE9\xC8\x00", 3},                //always jump
    {0, NULL, 0}
};

const orbis_patch_t shellcore_patches_1001[] = {
    {0x0E0FE59, "\x00", 1},                        // 'sce_sdmemory' patch 1
    {0x0E0FE98, "\x00", 1},                        // 'sce_sdmemory1' patch
    {0x0E0FEA6, "\x00", 1},                        // 'sce_sdmemory2' patch
    {0x0E0FEB4, "\x00", 1},                        // 'sce_sdmemory3' patch
    {0x08A7520, "\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x006B6A0, "\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x00C70A0, "\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x006D015, "\x90\x90", 2},                    // ^ (thanks to GRModSave_Username)
    {0x006B1E7, "\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x006ABA2, "\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x006A404, "\x90\x90", 2},                    //nevah jump
    {0x006A65E, "\xE9\xC8\x00", 3},                //always jump
    {0, NULL, 0}
};

const orbis_patch_t shellcore_patches_1050[] = {
    {0x0E149B9, "\x00", 1},                        // 'sce_sdmemory' patch 1
    {0x0E149F8, "\x00", 1},                        // 'sce_sdmemory1' patch
    {0x0E14A06, "\x00", 1},                        // 'sce_sdmemory2' patch
    {0x0E14A14, "\x00", 1},                        // 'sce_sdmemory3' patch
    {0x08AAC00, "\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x006B630, "\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x00C7060, "\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x006CFA5, "\x90\x90", 2},                    // ^ (thanks to GRModSave_Username)
    {0x006B177, "\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x006AB32, "\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x006a394, "\x90\x90", 2},                    //nevah jump
    {0x006A5EE, "\xE9\xC8\x00", 3},                //always jump
    {0, NULL, 0}
};


/* 11.00 WIP patches by LM and SocraticBliss */
const orbis_patch_t shellcore_patches_1100[] = {
    {0x0E26439, "\x00", 1},                        // 'sce_sdmemory' patch 1
    {0x0E26478, "\x00", 1},                        // 'sce_sdmemory1' patch
    {0x0E26486, "\x00", 1},                        // 'sce_sdmemory2' patch
    {0x0E26494, "\x00", 1},                        // 'sce_sdmemory3' patch
    {0x08BAF40, "\x48\x31\xC0\xC3", 4},            //verify keystone patch
    {0x006B630, "\x31\xC0\xC3", 3},                //transfer mount permission patch eg mount foreign saves with write permission
    {0x00C7060, "\x31\xC0\xC3", 3},                //patch psn check to load saves saves foreign to current account
    {0x006CFA5, "\x90\x90", 2},                    // ^ (thanks to GRModSave_Username)
    {0x006B177, "\x90\x90\x90\x90\x90\x90", 6},    // something something patches...
    {0x006AB32, "\x90\x90\x90\x90\x90\x90", 6},    // don't even remember doing this
    {0x006A394, "\x90\x90", 2},                    //nevah jump
    {0x006A5EE, "\xE9\xC8\x00", 3},                //always jump
    {0, NULL, 0}
};
