#ifndef INIT_H
#define INIT_H

#ifdef __cplusplus
extern "C" {
#endif

int init_cred(void);
int setup_cred(void);
int init_devices(void);

#ifdef __cplusplus
}
#endif

#endif // INIT_H