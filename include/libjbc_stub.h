// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#ifndef LIBJBC_H
#define LIBJBC_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <stdbool.h>
#include <orbis/libkernel.h>

#ifdef __cplusplus
extern "C" {
#endif

// jailbreak.h
typedef struct {
  uid_t uid;
  uid_t ruid;
  uid_t svuid;
  gid_t rgid;
  gid_t svgid;
  uintptr_t prison;
  uintptr_t cdir;
  uintptr_t rdir;
  uintptr_t jdir;
  uint64_t sceProcType;
  uint64_t sonyCred;
  uint64_t sceProcCap;
} jbc_cred;

// kernelrw.h
typedef enum {
  USERSPACE,
  KERNEL_HEAP,
  KERNEL_TEXT
} KmemKind;

// utils.h
enum {
  CWD_KEEP,
  CWD_ROOT,
  CWD_RESET,
};

// jailbreak.h
uintptr_t (*jbc_get_prison0)() = NULL;
uintptr_t (*jbc_get_rootvnode)() = NULL;
int (*jbc_get_cred)(jbc_cred *ans) = NULL;
int (*jbc_jailbreak_cred)(jbc_cred *ans) = NULL;
int (*jbc_set_cred)(const jbc_cred *ans) = NULL;

// kernelrw.h
uint64_t (*jbc_krw_kcall)(uint64_t fn, ...) = NULL;
uintptr_t (*jbc_krw_get_td)() = NULL;

int (*jbc_krw_memcpy)(uintptr_t dst, uintptr_t src, size_t sz, KmemKind kind) = NULL;
uint64_t (*jbc_krw_read64)(uintptr_t p, KmemKind kind) = NULL;
int (*jbc_krw_write64)(uintptr_t p, KmemKind kind, uintptr_t val) = NULL;

// utils.h
void (*jbc_run_as_root)(void (*fn)(void *arg), void *arg, int cwd_mode) = NULL;
int (*jbc_mount_in_sandbox)(const char *system_path, const char *mnt_name) = NULL;
int (*jbc_unmount_in_sandbox)(const char *mnt_name) = NULL;

int32_t jbcInitalize(int handle) {
  if (
      sceKernelDlsym(handle, "jbc_get_prison0", (void **)&jbc_get_prison0) != 0 ||
      sceKernelDlsym(handle, "jbc_get_rootvnode", (void **)&jbc_get_rootvnode) != 0 ||
      sceKernelDlsym(handle, "jbc_get_cred", (void **)&jbc_get_cred) != 0 ||
      sceKernelDlsym(handle, "jbc_jailbreak_cred", (void **)&jbc_jailbreak_cred) != 0 ||
      sceKernelDlsym(handle, "jbc_set_cred", (void **)&jbc_set_cred) != 0 ||

      sceKernelDlsym(handle, "jbc_krw_kcall", (void **)&jbc_krw_kcall) != 0 ||
      sceKernelDlsym(handle, "jbc_krw_get_td", (void **)&jbc_krw_get_td) != 0 ||

      sceKernelDlsym(handle, "jbc_krw_memcpy", (void **)&jbc_krw_memcpy) != 0 ||
      sceKernelDlsym(handle, "jbc_krw_read64", (void **)&jbc_krw_read64) != 0 ||
      sceKernelDlsym(handle, "jbc_krw_write64", (void **)&jbc_krw_write64) != 0 ||

      sceKernelDlsym(handle, "jbc_run_as_root", (void **)&jbc_run_as_root) != 0 ||
      sceKernelDlsym(handle, "jbc_mount_in_sandbox", (void **)&jbc_mount_in_sandbox) != 0 ||
      sceKernelDlsym(handle, "jbc_unmount_in_sandbox", (void **)&jbc_unmount_in_sandbox) != 0) {
    return -1;
  }

  return 0;
}

#ifdef __cplusplus
}
#endif

#endif
