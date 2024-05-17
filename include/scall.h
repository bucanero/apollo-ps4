#ifndef SCALL_H
#define SCALL_H

#include <orbis/libkernel.h>

int sys_open(const char *path, int flags, int mode);
int sys_mknod(const char *path, mode_t mode, dev_t dev);

#endif // SCALL_H