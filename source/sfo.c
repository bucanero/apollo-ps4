#include <apollo.h>
#include "sfo.h"
#include "util.h"

#define PKG_MAGIC   0x544E437Fu
#define SFO_MAGIC   0x46535000u
#define SFO_VERSION 0x0101u /* 1.1 */

typedef struct sfo_header_s {
	u32 magic;
	u32 version;
	u32 key_table_offset;
	u32 data_table_offset;
	u32 num_entries;
} sfo_header_t;

typedef struct sfo_index_table_s {
	u16 key_offset;
	u16 param_format;
	u32 param_length;
	u32 param_max_length;
	u32 data_offset;
} sfo_index_table_t;

typedef struct sfo_param_params_s {
	u32 unk1;
	u32 user_id;
	u8 unk2[32];
	u32 unk3;
	char title_id_1[0x10];
	char title_id_2[0x10];
	u32 unk4;
	u8 chunk[0x3B0];
} sfo_param_params_t;

typedef struct sfo_context_param_s {
	char *key;
	u16 format;
	u32 length;
	u32 max_length;
	size_t actual_length;
	u8 *value;
} sfo_context_param_t;

typedef struct pkg_table_entry {
	uint32_t id;
	uint32_t filename_offset;
	uint32_t flags1;
	uint32_t flags2;
	uint32_t offset;
	uint32_t size;
	uint64_t padding;
} pkg_table_entry_t;

struct sfo_context_s {
	list_t *params;
};

sfo_context_t * sfo_alloc(void) {
	sfo_context_t *context;
	context = (sfo_context_t *)malloc(sizeof(sfo_context_t));
	if (context) {
		memset(context, 0, sizeof(sfo_context_t));
		context->params = list_alloc();
	}
	return context;
}

void sfo_free(sfo_context_t *context) {
	if (!context)
		return;

	if (context->params) {
		list_node_t *node;
		sfo_context_param_t *param;

		node = list_head(context->params);
		while (node) {
			param = (sfo_context_param_t *)node->value;
			if (param) {
				if (param->key)
					free(param->key);
				if (param->value)
					free(param->value);
				free(param);
			}
			node = node->next;
		}

		list_free(context->params);
	}

	free(context);
}

// Finds the param.sfo's offset inside a PS4 PKG file
// https://github.com/hippie68/sfo/blob/main/sfo.c#L732
static int read_sfo_from_pkg(const char* pkg_path, uint8_t** sfo_buffer, size_t* sfo_size) {
	FILE* file;
	uint32_t pkg_table_offset;
	uint32_t pkg_file_count;
	pkg_table_entry_t pkg_table_entry;

	file = fopen(pkg_path, "rb");
	if (!file)
		return (-1);

  	fread(&pkg_file_count, sizeof(uint32_t), 1, file);
	if (pkg_file_count != PKG_MAGIC)
	{
		fclose(file);
		return (-1);
	}

	fseek(file, 0x00C, SEEK_SET);
	fread(&pkg_file_count, sizeof(uint32_t), 1, file);
	fseek(file, 0x018, SEEK_SET);
	fread(&pkg_table_offset, sizeof(uint32_t), 1, file);

	pkg_file_count = ES32(pkg_file_count);
	pkg_table_offset = ES32(pkg_table_offset);
	fseek(file, pkg_table_offset, SEEK_SET);

	for (int i = 0; i < pkg_file_count; i++) {
		fread(&pkg_table_entry, sizeof (pkg_table_entry_t), 1, file);

		// param.sfo ID
		if (pkg_table_entry.id == 1048576) {
			*sfo_size = ES32(pkg_table_entry.size);
			*sfo_buffer = (uint8_t*)malloc(*sfo_size);

			fseek(file, ES32(pkg_table_entry.offset), SEEK_SET);
			fread(*sfo_buffer, *sfo_size, 1, file);
			fclose(file);

			return 0;
		}
	}

	LOG("Could not find a param.sfo file inside the PKG.");
	fclose(file);
	return(-1);
}

int sfo_read(sfo_context_t *context, const char *file_path) {
	int ret;
	u8 *sfo;
	size_t sfo_size;
	sfo_header_t *header;
	sfo_index_table_t *index_table;
	sfo_context_param_t *param;
	size_t i;

	ret = 0;

	if (strcasecmp(file_path + strlen(file_path) - 4, ".pkg") == 0) {
		if ((ret = read_sfo_from_pkg(file_path, &sfo, &sfo_size)) < 0)
			goto error;
	} else {
		if ((ret = read_buffer(file_path, &sfo, &sfo_size)) < 0)
			goto error;
	}

	if (sfo_size < sizeof(sfo_header_t)) {
		ret = -1;
		goto error;
	}

	header = (sfo_header_t *)sfo;

	if (header->magic != SFO_MAGIC) {
		ret = -1;
		goto error;
	}

	for (i = 0; i < header->num_entries; ++i) {
		index_table = (sfo_index_table_t *)(sfo + sizeof(sfo_header_t) + i * sizeof(sfo_index_table_t));

		param = (sfo_context_param_t *)malloc(sizeof(sfo_context_param_t));
		if	(param) {
			memset(param, 0, sizeof(sfo_context_param_t));
			param->key = strdup((char *)(sfo + header->key_table_offset + index_table->key_offset));
			param->format = index_table->param_format;
			param->length = index_table->param_length;
			param->max_length = index_table->param_max_length;
			param->value = (u8 *)malloc(index_table->param_max_length);
			param->actual_length = index_table->param_max_length;
			if (param->value) {
				memcpy(param->value, (u8 *)(sfo + header->data_table_offset + index_table->data_offset), param->actual_length);
			} else {
				/* TODO */
				assert(0);
			}
		} else {
			/* TODO */
			assert(0);
		}

		list_append(context->params, param);
	}

	free(sfo);

error:
	return ret;
}

int sfo_write(sfo_context_t *context, const char *file_path) {
	int ret;
	list_node_t *node;
	sfo_context_param_t *param;
	u8 *sfo;
	sfo_header_t *header;
	sfo_index_table_t *index_table;
	size_t num_params;
	size_t sfo_size;
	size_t key_table_size, data_table_size;
	size_t key_offset, data_offset;
	size_t i;

	ret = 0;

	if (!context) {
		ret = -1;
		goto error;
	}
	if (!context->params) {
		ret = -1;
		goto error;
	}

	num_params = list_count(context->params);

	key_table_size = 0;
	data_table_size = 0;

	for (node = list_head(context->params); node; node = node->next) {
		param = (sfo_context_param_t *)node->value;
		assert(param != NULL);

		key_table_size += strlen(param->key) + 1;
		data_table_size += param->actual_length;
	}
	sfo_size = sizeof(sfo_header_t) + num_params * sizeof(sfo_index_table_t) + key_table_size + data_table_size;
	key_table_size += ALIGN(sfo_size, 8) - sfo_size;
	sfo_size = ALIGN(sfo_size, 8);

	sfo = (u8 *)malloc(sfo_size);
	if (!sfo) {
		ret = -1;
		goto error;
	}

	memset(sfo, 0, sfo_size);

	header = (sfo_header_t *)sfo;
	header->magic = SFO_MAGIC;
	header->version = SFO_VERSION;
	header->key_table_offset = sizeof(sfo_header_t) + num_params * sizeof(sfo_index_table_t);
	header->data_table_offset = sizeof(sfo_header_t) + num_params * sizeof(sfo_index_table_t) + key_table_size;
	header->num_entries = num_params;

	for (node = list_head(context->params), key_offset = 0, data_offset = 0, i = 0; node; node = node->next, ++i) {
		param = (sfo_context_param_t *)node->value;
		assert(param != NULL);

		index_table = (sfo_index_table_t *)(sfo + sizeof(sfo_header_t) + i * sizeof(sfo_index_table_t));
		index_table->key_offset = key_offset;
		index_table->param_format = param->format;
		index_table->param_length = param->length;
		index_table->param_max_length = param->max_length;
		index_table->data_offset = data_offset;

		key_offset += strlen(param->key) + 1;
		data_offset += param->actual_length;
	}

	for (node = list_head(context->params), i = 0; node; node = node->next, ++i) {
		param = (sfo_context_param_t *)node->value;
		assert(param != NULL);

		index_table = (sfo_index_table_t *)(sfo + sizeof(sfo_header_t) + i * sizeof(sfo_index_table_t));
		memcpy(sfo + header->key_table_offset + index_table->key_offset, param->key, strlen(param->key) + 1);
		memcpy(sfo + header->data_table_offset + index_table->data_offset, param->value, param->actual_length);
	}

	if ((ret = write_buffer(file_path, sfo, sfo_size)) < 0)
		goto error;

	free(sfo);

error:
	return ret;
}

static sfo_context_param_t * sfo_context_get_param(sfo_context_t *context, const char *key) {
	list_node_t *node;
	sfo_context_param_t *param;

	if (!context || !key)
		return NULL;

	node = list_head(context->params);
	while (node) {
		param = (sfo_context_param_t *)node->value;
		if (param && strcmp(param->key, key) == 0)
			return param;
		node = node->next;
	}

	return NULL;
}

void sfo_grab(sfo_context_t *inout, sfo_context_t *tpl, int num_keys, const sfo_key_pair_t *keys) {
	sfo_context_param_t *p1;
	sfo_context_param_t *p2;
	int i;

	for (i = 0; i < num_keys; ++i) {
		if (!keys[i].flag)
			continue;

		p1 = sfo_context_get_param(inout, keys[i].name);
		p2 = sfo_context_get_param(tpl, keys[i].name);

		if (p1 && p2) {
			if (strcmp(keys[i].name, "PARAMS") != 0) {
				if (p1->actual_length != p2->actual_length) {
					p1->value = (u8 *)realloc(p1->value, p2->actual_length);
					memset(p1->value, 0, p2->actual_length);
				}

				memcpy(p1->value, p2->value, p2->actual_length);
				p1->format = p2->format;
				p1->length = p2->length;
				p1->max_length = p2->max_length;
				p1->actual_length = p2->actual_length;
			} else {
				sfo_param_params_t *params1;
				sfo_param_params_t *params2;

				assert(p1->actual_length == p2->actual_length);

				params1 = (sfo_param_params_t *)p1->value;
				params2 = (sfo_param_params_t *)p2->value;

				params1->user_id = params2->user_id;
			}
		}
	}
}

static void sfo_patch_titleid(sfo_context_t *inout) {
	sfo_context_param_t *p;

	p = sfo_context_get_param(inout, "PARAMS");
	if (p != NULL) {
		sfo_param_params_t *params = (sfo_param_params_t *)p->value;
		memcpy(params->title_id_2, params->title_id_1, sizeof(params->title_id_1));
	}
}

static void sfo_patch_account(sfo_context_t *inout, u64 account) {
	sfo_context_param_t *p;

	if (!account)
		return;

	p = sfo_context_get_param(inout, "ACCOUNT_ID");
	if (p != NULL && p->actual_length == SFO_ACCOUNT_ID_SIZE) {
		memcpy(p->value, &account, SFO_ACCOUNT_ID_SIZE);
	}
/*
	p = sfo_context_get_param(inout, "PARAMS");
	if (p != NULL) {
		sfo_param_params_t *params = (sfo_param_params_t *)p->value;
		memcpy(params->account_id, account, SFO_ACCOUNT_ID_SIZE);
	}
*/
}

static void sfo_patch_user_id(sfo_context_t *inout, u32 userid) {
	sfo_context_param_t *p;

	if (userid == 0)
		return;

	p = sfo_context_get_param(inout, "PARAMS");
	if (p != NULL) {
		sfo_param_params_t *params = (sfo_param_params_t *)p->value;
		params->user_id = userid;
	}
}

/*
void sfo_patch_psid(sfo_context_t *inout, u8* psid) {
	sfo_context_param_t *p;

	if (!psid)
		return;

	p = sfo_context_get_param(inout, "PARAMS");
	if (p != NULL) {
		sfo_param_params_t *params = (sfo_param_params_t *)p->value;
		memcpy(params->psid, psid, SFO_PSID_SIZE);
	}
}

void sfo_patch_directory(sfo_context_t *inout, const char* save_dir) {
	sfo_context_param_t *p;

	if (!save_dir)
		return;

	p = sfo_context_get_param(inout, "SAVEDATA_DIRECTORY");
	if (p != NULL && p->actual_length == SFO_DIRECTORY_SIZE) {
		memset(p->value, 0, SFO_DIRECTORY_SIZE);
		memcpy(p->value, save_dir, strlen(save_dir));
	}
}
*/

u8* sfo_get_param_value(sfo_context_t *in, const char* param) {
	sfo_context_param_t *p;

	p = sfo_context_get_param(in, param);
	if (p != NULL) {
		return (p->value);
	}

	return NULL;
}

int patch_sfo(const char *in_file_path, sfo_patch_t* patches) {
	sfo_context_t *sfo;

	sfo = sfo_alloc();
	if (sfo_read(sfo, in_file_path) < 0) {
		sfo_free(sfo);
		LOG("Unable to read from '%s'", in_file_path);
		return -1;
	}

	sfo_patch_titleid(sfo);
	sfo_patch_account(sfo, patches->account_id);
	sfo_patch_user_id(sfo, patches->user_id);
//	sfo_patch_psid(sfo, patches->psid);
//	sfo_patch_directory(sfo, patches->directory);

	if (sfo_write(sfo, in_file_path) < 0) {
		LOG("Unable to write to '%s'", in_file_path);
		sfo_free(sfo);
		return -1;
	}

	if (sfo)
		sfo_free(sfo);

	LOG("PARAM.SFO was patched successfully");
	return 0;
}

int build_sfo(const char *in_file_path, const char *out_file_path, const char *tpl_file_path, int num_keys, const sfo_key_pair_t *keys) {
	sfo_context_t *sfo1;
	sfo_context_t *sfo2;

	sfo1 = sfo_alloc();
	if (sfo_read(sfo1, in_file_path) < 0) {
		sfo_free(sfo1);
		LOG("Unable to read from '%s'", in_file_path);
		return -1;
	}

	sfo2 = sfo_alloc();
	if (sfo_read(sfo2, tpl_file_path) < 0) {
		sfo_free(sfo2);
		LOG("Unable to read from '%s'", tpl_file_path);
		return -1;
	}

	sfo_grab(sfo1, sfo2, num_keys, keys);
	if (sfo_write(sfo1, out_file_path) < 0) {
		LOG("Unable to write to '%s'", out_file_path);
		return -1;
	}

	if (sfo1)
		sfo_free(sfo1);
	if (sfo2)
		sfo_free(sfo2);

	LOG("PARAM.SFO was built successfully");
	return 0;
}

/*
int patch_sfo_trophy(const char *in_file_path, const char* account) {
	sfo_context_t *sfo;
	sfo_context_param_t *p;

	if (!account)
		return 0;

	sfo = sfo_alloc();
	if (sfo_read(sfo, in_file_path) < 0) {
		sfo_free(sfo);
		LOG("Unable to read from '%s'", in_file_path);
		return -1;
	}

	p = sfo_context_get_param(sfo, "ACCOUNTID");
	if (p != NULL && p->actual_length == SFO_ACCOUNT_ID_SIZE) {
		memcpy(p->value, account, SFO_ACCOUNT_ID_SIZE);
	}

	if (sfo_write(sfo, in_file_path) < 0) {
		LOG("Unable to write to '%s'", in_file_path);
		return -1;
	}

	if (sfo)
		sfo_free(sfo);

	LOG("PARAM.SFO was patched successfully");
	return 0;
}
*/
