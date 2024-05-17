#ifndef INIT_H
#define INIT_H

#include <unistd.h>
#include <sys/stat.h>
#include <orbis/libkernel.h>
#include <libjbc.h>

#include "sd.h"
#include "scall.h"

int init_cred(void);
int init_devices(void);

#endif // INIT_H