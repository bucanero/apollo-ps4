#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <orbis/libkernel.h>
#include <libjbc.h>

#include "orbis_patches.h"
#include "util.h"

#define sys_proc_read_mem(p, a, d, l)       sys_proc_rw(p, a, d, l, 0)
#define sys_proc_write_mem(p, a, d, l)      sys_proc_rw(p, a, d, l, 1)


asm("cpu_enable_wp:\n"
    "mov %rax, %cr0\n"
    "or %rax, 0x10000\n"
    "mov %cr0, %rax\n"
    "ret");
void cpu_enable_wp();

asm("cpu_disable_wp:\n"
    "mov %rax, %cr0\n"
    "and %rax, ~0x10000\n"
    "mov %cr0, %rax\n"
    "ret");
void cpu_disable_wp();

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

static orbis_patch_t *shellcore_backup = NULL;
static int goldhen2 = 0;

static int _sceKernelGetModuleInfo(OrbisKernelModule handle, OrbisKernelModuleInfo* info)
{
    if (!info)
        return ORBIS_KERNEL_ERROR_EFAULT;

    memset(info, 0, sizeof(*info));
    info->size = sizeof(*info);

    return orbis_syscall(SYS_dynlib_get_info, handle, info);
}

int sceKernelGetModuleInfoByName(const char* name, OrbisKernelModuleInfo* info)
{
    OrbisKernelModuleInfo tmpInfo;
    OrbisKernelModule handles[256];
    size_t numModules;
    int ret;

    if (!name || !info)
        return ORBIS_KERNEL_ERROR_EFAULT;

    memset(handles, 0, sizeof(handles));

    ret = sceKernelGetModuleList(handles, countof(handles), &numModules);
    if (ret) {
        LOG("sceKernelGetModuleList (%X)", ret);
        return ret;
    }

    for (size_t i = 0; i < numModules; ++i)
    {
        ret = _sceKernelGetModuleInfo(handles[i], &tmpInfo);
        if (ret) {
            LOG("_sceKernelGetModuleInfo (%X)", ret);
            return ret;
        }

        if (strcmp(tmpInfo.name, name) == 0) {
            memcpy(info, &tmpInfo, sizeof(tmpInfo));
            return 0;
        }
    }

    return ORBIS_KERNEL_ERROR_ENOENT;
}

int get_module_base(const char* name, uint64_t* base, uint64_t* size)
{
    OrbisKernelModuleInfo moduleInfo;
    int ret;

    ret = sceKernelGetModuleInfoByName(name, &moduleInfo);
    if (ret)
    {
        LOG("[!] sceKernelGetModuleInfoByName('%s') failed: 0x%08X", name, ret);
        return 0;
    }

    if (base)
        *base = (uint64_t)moduleInfo.segmentInfo[0].address;

    if (size)
        *size = moduleInfo.segmentInfo[0].size;

    return 1;
}

int patch_module(const char* name, module_patch_cb_t* patch_cb, void* arg)
{
    uint64_t base, size;
    int ret;

    if (!get_module_base(name, &base, &size)) {
        LOG("[!] get_module_base return error");
        return 0;
    }
    LOG("[i] '%s' module base=0x%lX size=%ld", name, base, size);

    ret = sceKernelMprotect((void*)base, size, ORBIS_KERNEL_PROT_CPU_ALL);
    if (ret) {
        LOG("[!] sceKernelMprotect(%lX) failed: 0x%08X", base, ret);
        return 0;
    }

    LOG("[+] patching module '%s'...", name);
    if (patch_cb)
        patch_cb(arg, (uint8_t*)base, size);

    return 1;
}

static void sdmemory_patches_cb(void* patches, uint8_t* base, uint64_t size)
{
    for (const orbis_patch_t *patch = patches; patch->size > 0; patch++)
    {
        memcpy(base + patch->offset, patch->data, patch->size);

        LOG("(W) ptr=%p : %08X (%d bytes)", base, patch->offset, patch->size);
        dump_data((uint8_t*) patch->data, patch->size);
    }

}

static int patch_SceSaveData(const orbis_patch_t* savedata_patch)
{
    if(!patch_module("libSceSaveData.sprx", &sdmemory_patches_cb, (void*) savedata_patch))
        return 0;

    LOG("[+] Module patched!");
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

// custom syscall 112
int sys_console_cmd(uint64_t cmd, void *data)
{
    return orbis_syscall(goldhen2 ? 200 : 112, cmd, data);
}

// GoldHEN 2+ only
// custom syscall 200
static int check_syscalls(void)
{
    uint64_t tmp;

    if (orbis_syscall(200, SYS_CONSOLE_CMD_ISLOADED, NULL) == 1)
    {
        LOG("GoldHEN 2.x is loaded!");
        goldhen2 = 90;
        return 1;
    }
    else if (orbis_syscall(200, SYS_CONSOLE_CMD_PRINT, "apollo") == 0)
    {
        LOG("GoldHEN 1.x is loaded!");
        return 1;
    }
    else if (orbis_syscall(107, NULL, &tmp) == 0)
    {
        LOG("ps4debug is loaded!");
        return 1;
    }

    return 0;
}

// GoldHEN 2+ custom syscall 197
// (same as ps4debug syscall 107)
int sys_proc_list(struct proc_list_entry *procs, uint64_t *num)
{
    return orbis_syscall(107 + goldhen2, procs, num);
}

// GoldHEN 2+ custom syscall 198
// (same as ps4debug syscall 108)
int sys_proc_rw(uint64_t pid, uint64_t address, void *data, uint64_t length, uint64_t write)
{
    return orbis_syscall(108 + goldhen2, pid, address, data, length, write);
}

// GoldHEN 2+ custom syscall 199
// (same as ps4debug syscall 109)
int sys_proc_cmd(uint64_t pid, uint64_t cmd, void *data)
{
    return orbis_syscall(109 + goldhen2, pid, cmd, data);
}

/*
int goldhen_jailbreak()
{
    return sys_console_cmd(SYS_CONSOLE_CMD_JAILBREAK, NULL);
}
*/

int find_process_pid(const char* proc_name, int* pid)
{
    struct proc_list_entry *proc_list;
    uint64_t pnum;

    if (sys_proc_list(NULL, &pnum))
        return 0;

    proc_list = (struct proc_list_entry *) malloc(pnum * sizeof(struct proc_list_entry));

    if(!proc_list)
        return 0;

    if (sys_proc_list(proc_list, &pnum))
    {
        free(proc_list);
        return 0;
    }

    for (size_t i = 0; i < pnum; i++)
        if (strncmp(proc_list[i].p_comm, proc_name, 32) == 0)
        {
            LOG("[i] Found '%s' PID %d", proc_name, proc_list[i].pid);
            *pid = proc_list[i].pid;
            free(proc_list);
            return 1;
        }

    LOG("[!] '%s' Not Found", proc_name);
    free(proc_list);
    return 0;
}

int find_map_entry_start(int pid, const char* entry_name, uint64_t* start)
{
    struct sys_proc_vm_map_args proc_vm_map_args = {
        .maps = NULL,
        .num = 0
    };

    if(sys_proc_cmd(pid, SYS_PROC_VM_MAP, &proc_vm_map_args))
        return 0;

    proc_vm_map_args.maps = (struct proc_vm_map_entry *) malloc(proc_vm_map_args.num * sizeof(struct proc_vm_map_entry));

    if(!proc_vm_map_args.maps)
        return 0;

    if(sys_proc_cmd(pid, SYS_PROC_VM_MAP, &proc_vm_map_args))
    {
        free(proc_vm_map_args.maps);
        return 0;
    }

    for (size_t i = 0; i < proc_vm_map_args.num; i++)
        if (strncmp(proc_vm_map_args.maps[i].name, entry_name, 32) == 0)
        {
            LOG("Found '%s' Start addr %lX", entry_name, proc_vm_map_args.maps[i].start);
            *start = proc_vm_map_args.maps[i].start;
            free(proc_vm_map_args.maps);
            return 1;
        }

    LOG("[!] '%s' Not Found", entry_name);
    free(proc_vm_map_args.maps);
    return 0;
}

static void write_SceShellCore(int pid, uint64_t start, const orbis_patch_t* patches)
{
    void* data = aligned_alloc(0x10, 0x100);

    //SHELLCORE PATCHES
    for (const orbis_patch_t *patch = patches; patch->size > 0; patch++)
    {
        memcpy(data, patch->data, patch->size);
        sys_proc_write_mem(pid, start + patch->offset, data, patch->size);

        LOG("(W) %lX : %08X (size %d)", start, patch->offset, patch->size);
        dump_data((uint8_t*) patch->data, patch->size);
    }
    free(data);
}

// hack to disable unpatching on exit in case unmount failed (to avoid KP)
void disable_unpatch()
{
    shellcore_backup = NULL;
}

int unpatch_SceShellCore(void)
{
    int pid;
    uint64_t ex_start;

    if (!shellcore_backup || !find_process_pid("SceShellCore", &pid) || !find_map_entry_start(pid, "executable", &ex_start))
    {
        LOG("Error: Unable to unpatch SceShellCore");
        return 0;
    }

    write_SceShellCore(pid, ex_start, shellcore_backup);
    return 1;
}

static int patch_SceShellCore(const orbis_patch_t* shellcore_patch)
{
    int pid;
    uint64_t ex_start;

    if (!find_process_pid("SceShellCore", &pid) || !find_map_entry_start(pid, "executable", &ex_start))
    {
        LOG("Error: Unable to patch SceShellCore");
        return 0;
    }

    if (!shellcore_backup)
    {
        size_t i = 0;

        while (shellcore_patch[i++].size) {}
        shellcore_backup = calloc(1, i * sizeof(orbis_patch_t));

        for (i = 0; shellcore_patch[i].size > 0; i++)
        {
            shellcore_backup[i].offset = shellcore_patch[i].offset;
            shellcore_backup[i].size = shellcore_patch[i].size;
            shellcore_backup[i].data = malloc(shellcore_patch[i].size);
            sys_proc_read_mem(pid, ex_start + shellcore_backup[i].offset, shellcore_backup[i].data, shellcore_backup[i].size);

            LOG("(R) %lX : %08X (%d bytes)", ex_start, shellcore_backup[i].offset, shellcore_backup[i].size);
            dump_data((uint8_t*) shellcore_backup[i].data, shellcore_backup[i].size);
        }
    }

    write_SceShellCore(pid, ex_start, shellcore_patch);
    return 1;
}

int patch_save_libraries(void)
{
    const orbis_patch_t* shellcore_patch = NULL;
    const orbis_patch_t* savedata_patch = NULL;
    int version = get_firmware_version();

    switch (version)
    {
    case -1:
        notify_popup("cxml://psnotification/tex_icon_ban", "Error: Can't detect firmware version!");
        return 0;

    case 0x505:
    case 0x507:
        savedata_patch = scesavedata_patches_505;
        shellcore_patch = shellcore_patches_505;
        break;
    
    case 0x672:
        savedata_patch = scesavedata_patches_672;
        shellcore_patch = shellcore_patches_672;
        break;

    case 0x702:
        savedata_patch = scesavedata_patches_702;
        shellcore_patch = shellcore_patches_702;
        break;

    case 0x750:
    case 0x751:
    case 0x755:
        savedata_patch = scesavedata_patches_75x;
        shellcore_patch = shellcore_patches_75x;
        break;

    case 0x900:
        savedata_patch = scesavedata_patches_900;
        shellcore_patch = shellcore_patches_900;
        break;
	    
     case 0x1100:
        savedata_patch = scesavedata_patches_1100;
        shellcore_patch = shellcore_patches_1100;
        break;

    default:
        notify_popup("cxml://psnotification/tex_icon_ban", "Unsupported firmware version %X.%02X", version >> 8, version & 0xFF);
        return 0;
    }

    if (!check_syscalls())
    {
        notify_popup("cxml://psnotification/tex_icon_ban", "Missing %X.%02X GoldHEN or ps4debug payload!", version >> 8, version & 0xFF);
        return 0;
    }

    if (!patch_SceShellCore(shellcore_patch) || !patch_SceSaveData(savedata_patch))
    {
        notify_popup("cxml://psnotification/tex_icon_ban", "Error: Failed to apply %X.%02X Save patches!", version >> 8, version & 0xFF);
        return 0;
    }
    notify_popup("cxml://psnotification/tex_default_icon_notification", "PS4 %X.%02X Save patches applied", version >> 8, version & 0xFF);

    return 1;
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

char* get_fw_by_pfskey_ver(int key_ver)
{
	char* fw[] = {"???", "Any", "4.50+", "4.70+", "5.00+", "5.50+", "6.00+", "6.50+", "7.00+", "7.50+", "8.00+"};
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
        notify_popup("cxml://psnotification/tex_icon_ban", "Jailbreak failed!");
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

/*
struct vm_map_entry
{
    struct vm_map_entry *prev;
    uintptr_t next;                 // struct vm_map_entry *next;
    struct vm_map_entry *left;
    struct vm_map_entry *right;
    uint64_t start;
    uint64_t end;
    char unk1[32];
    uint64_t offset;
    uint32_t unk2;
    uint16_t prot;
    char unk3[47];
    char name[32];
} __attribute__((packed));

// 5.05 TYPE_FIELD(char p_comm[32], 0x44C);
// 6.72 TYPE_FIELD(char p_comm[32], 0x454);

static int resolve_process(const char* proc_name, int* proc_pid, uint64_t* exe_start)
{
    struct vm_map_entry entry;

restart:;
    uintptr_t td = jbc_krw_get_td();
    uintptr_t proc = jbc_krw_read64(td+8, KERNEL_HEAP);
    for(;;)
    {
        int pid;
        char p_comm[32];

        if(jbc_krw_memcpy((uintptr_t) p_comm, proc+0x454, sizeof(p_comm), KERNEL_HEAP) ||
           jbc_krw_memcpy((uintptr_t) &pid, proc+0xB0, sizeof(pid), KERNEL_HEAP))
            goto restart;

        if(strncmp(p_comm, proc_name, sizeof(p_comm)) == 0)
        {
            entry.next = jbc_krw_read64(proc+0x168, KERNEL_HEAP);
            LOG("Found '%s' PID (%d) %lX", p_comm, pid, entry.next);
            *proc_pid = pid;
            break;
        }
        
        if (pid == 1)
            return 0;

        uintptr_t proc2 = jbc_krw_read64(proc, KERNEL_HEAP);
        uintptr_t proc1 = jbc_krw_read64(proc2+8, KERNEL_HEAP);
        if(proc1 != proc)
            goto restart;
        proc = proc2;
    }

    do
    {
        if (jbc_krw_memcpy((uintptr_t) &entry, entry.next, sizeof(struct vm_map_entry), KERNEL_HEAP))
            return 0;

        if (strncmp(entry.name, "executable", sizeof(entry.name)) == 0)
        {
            LOG("Found '%s' start (%lX)", entry.name, entry.start);
            *exe_start = entry.start;
            return 1;
        }

    } while (entry.next);

    return 0;
}
*/
