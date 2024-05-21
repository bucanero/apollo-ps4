#ifndef SCALL_H
#define SCALL_H

#include <orbis/libkernel.h>

#ifdef __cplusplus
extern "C" {
#endif

int sys_open(const char *path, int flags, int mode);
int sys_mknod(const char *path, mode_t mode, dev_t dev);

#ifdef __cplusplus
}
#endif

#endif // SCALL_H