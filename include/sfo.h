#ifndef _SFO_H_
#define _SFO_H_

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SFO_PATCH_FLAG_REMOVE_COPY_PROTECTION (1 << 0)
#define SFO_ACCOUNT_ID_SIZE 8
#define SFO_PSID_SIZE 16
#define SFO_DIRECTORY_SIZE 32

typedef struct sfo_context_s sfo_context_t;

typedef struct sfo_key_pair_s {
	const char *name;
	int flag;
} sfo_key_pair_t;

typedef struct {
	u32 flags;
	u32 user_id;
	u64 account_id;
	u8* psid;
	char* directory;
} sfo_patch_t;

typedef struct {
	uint32_t unk1;
	uint32_t user_id;
} sfo_params_ids_t;

sfo_context_t * sfo_alloc(void);
void sfo_free(sfo_context_t *context);

int sfo_read(sfo_context_t *context, const char *file_path);
int sfo_write(sfo_context_t *context, const char *file_path);

void sfo_grab(sfo_context_t *inout, sfo_context_t *tpl, int num_keys, const sfo_key_pair_t *keys);
void sfo_patch(sfo_context_t *inout, unsigned int flags);

u8* sfo_get_param_value(sfo_context_t *in, const char* param);

int patch_sfo(const char *in_file_path, sfo_patch_t* patches);
int build_sfo(const char *in_file_path, const char *out_file_path, const char *tpl_file_path, int num_keys, const sfo_key_pair_t *keys);

int patch_sfo_trophy(const char *in_file_path, const char* account);

#ifdef __cplusplus
}
#endif

#endif /* !_SFO_H_ */
