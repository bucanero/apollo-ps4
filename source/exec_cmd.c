#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <orbis/NetCtl.h>
#include <orbis/SaveData.h>
#include <orbis/SystemService.h>
#include <orbis/UserService.h>
#include <orbis/SystemService.h>
#include <polarssl/md5.h>
#include <mini18n.h>

#include "saves.h"
#include "menu.h"
#include "common.h"
#include "util.h"
#include "sfo.h"
#include "mcio.h"
#include "ps1card.h"
#include "svpng.h"

static char host_buf[256];

static void _set_dest_path(char* path, int dest, const char* folder)
{
	switch (dest)
	{
	case STORAGE_USB0:
		sprintf(path, "%s%s", USB0_PATH, folder);
		break;

	case STORAGE_USB1:
		sprintf(path, "%s%s", USB1_PATH, folder);
		break;

	case STORAGE_HDD:
		sprintf(path, "%s%s", FAKE_USB_PATH, folder);
		break;

	default:
		path[0] = 0;
	}
}

static void downloadSave(const save_entry_t* entry, const char* file, int dst)
{
	char path[256];

	_set_dest_path(path, dst, (entry->flags & SAVE_FLAG_PS4) ? PS4_SAVES_PATH_USB : PSV_SAVES_PATH_USB);
	if (mkdirs(path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), path);
		return;
	}

	if (!http_download(entry->path, file, APOLLO_LOCAL_CACHE "tmpsave.zip", 1))
	{
		show_message("%s\n%s%s", _("Error downloading save game from:"), entry->path, file);
		return;
	}

	if (extract_zip(APOLLO_LOCAL_CACHE "tmpsave.zip", path))
		show_message("%s\n%s", _("Save game successfully downloaded to:"), path);
	else
		show_message("%s", _("Error extracting save game!"));

	unlink_secure(APOLLO_LOCAL_CACHE "tmpsave.zip");
}

static struct tm get_local_time(void)
{
	int32_t tz_offset = 0;
	int32_t tz_dst = 0;
	int32_t ret = 0;

	if ((ret = sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_TIME_ZONE, &tz_offset)) < 0)
	{
		LOG("Failed to obtain ORBIS_SYSTEM_SERVICE_PARAM_ID_TIME_ZONE! Setting timezone offset to 0");
		LOG("sceSystemServiceParamGetInt: 0x%08X", ret);
		tz_offset = 0;
	}

	if ((ret = sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_SUMMERTIME, &tz_dst)) < 0)
	{
		LOG("Failed to obtain ORBIS_SYSTEM_SERVICE_PARAM_ID_SUMMERTIME! Setting timezone daylight time savings to 0");
		LOG("sceSystemServiceParamGetInt: 0x%08X", ret);
		tz_dst = 0;
	}

	time_t modifiedTime = time(NULL) + ((tz_offset + (tz_dst * 60)) * 60);
	return (*gmtime(&modifiedTime));
}

static void zipSave(const save_entry_t* entry, const char* exp_path)
{
	char export_file[256];
	char zip_file[256];
	struct tm t = get_local_time();
	char* tmp;
	int ret;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), exp_path);
		return;
	}

	init_loading_screen(_("Exporting save game..."));

	snprintf(zip_file, sizeof(zip_file), "%s%s-%s_%d-%02d-%02d_%02d%02d%02d.zip", exp_path, entry->title_id, entry->dir_name, t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

	tmp = strdup(entry->path);
	*strrchr(tmp, '/') = 0;
	*strrchr(tmp, '/') = 0;

	ret = zip_directory(tmp, entry->path, zip_file);
	free(tmp);

	if (ret)
	{
		snprintf(export_file, sizeof(export_file), "%s%08x.txt", exp_path, apollo_config.user_id);
		FILE* f = fopen(export_file, "a");
		if (f)
		{
			fprintf(f, "%s=[%s] %s\n", zip_file, entry->title_id, entry->name);
			fclose(f);
		}

		snprintf(export_file, sizeof(export_file), "%s%08x.xml", exp_path, apollo_config.user_id);
		save_xml_owner(export_file);
	}

	stop_loading_screen();
	if (!ret)
	{
		show_message("%s\n%s", _("Error! Can't export save game to:"), exp_path);
		return;
	}

	show_message("%s\n%s", _("Zip file successfully saved to:"), zip_file);
}

static void copySave(const save_entry_t* save, const char* exp_path)
{
	int ret;
	char copy_path[256];

	if (strncmp(save->path, exp_path, strlen(exp_path)) == 0)
	{
		show_message("%s\n%s", _("Copy operation cancelled!"), _("Same source and destination."));
		return;
	}

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), exp_path);
		return;
	}

	init_loading_screen(_("Copying files..."));

	snprintf(copy_path, sizeof(copy_path), "%s%08x_%s_%s/", exp_path, apollo_config.user_id, save->title_id, save->dir_name);
	LOG("Copying <%s> to %s...", save->path, copy_path);

	ret = copy_directory(save->path, save->path, copy_path);
	stop_loading_screen();

	if (ret == SUCCESS)
		show_message("%s\n%s", _("Files successfully copied to:"), exp_path);
	else
		show_message("%s\n%s", _("Error! Can't copy save game to:"), exp_path);
}

static int _update_save_details(const char* sys_path, const save_entry_t* save)
{
	char file_path[256];
	uint8_t* iconBuf;
	size_t iconSize;

	snprintf(file_path, sizeof(file_path), "%s" "param.sfo", sys_path);
	LOG("Update Save Details :: Reading %s...", file_path);

	sfo_context_t* sfo = sfo_alloc();
	if ((sfo_read(sfo, file_path) < 0) || !orbis_UpdateSaveParams(save,
		(char*) sfo_get_param_value(sfo, "MAINTITLE"), (char*) sfo_get_param_value(sfo, "SUBTITLE"),
		(char*) sfo_get_param_value(sfo, "DETAIL"), *(uint32_t*) sfo_get_param_value(sfo, "SAVEDATA_LIST_PARAM")))
	{
		LOG("Unable to read from '%s'", file_path);
		sfo_free(sfo);
		return 0;
	}

	sfo_free(sfo);

	snprintf(file_path, sizeof(file_path), "%s" "icon0.png", sys_path);
	if (read_buffer(file_path, &iconBuf, &iconSize) == SUCCESS)
	{
		snprintf(file_path, sizeof(file_path), SAVE_ICON_PATH_HDD "%s/%s_icon0.png", apollo_config.user_id, save->title_id, save->dir_name);
		mkdirs(file_path);

		if (write_buffer(file_path, iconBuf, iconSize) < 0)
		{
			// Error handling
			LOG("ERROR sceSaveDataSaveIcon");
		}

		free(iconBuf);
	}

	return 1;
}

static void downloadSaveHDD(const save_entry_t* entry, const char* file)
{
	save_entry_t save;
	char path[256];
	char mount[ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE];
	char titleid[0x10];
	char dirname[0x30];
	sfo_context_t* sfo;

	if (!http_download(entry->path, file, APOLLO_LOCAL_CACHE "tmpsave.zip", 1))
	{
		show_message("%s\n%s%s", _("Error downloading save game from:"), entry->path, file);
		return;
	}

	sfo = sfo_alloc();
	if (!extract_sfo(APOLLO_LOCAL_CACHE "tmpsave.zip", APOLLO_LOCAL_CACHE) ||
		sfo_read(sfo, APOLLO_LOCAL_CACHE "param.sfo") < 0)
	{
		LOG("Unable to read from '%s'", APOLLO_LOCAL_CACHE);
		sfo_free(sfo);

		show_message("%s", _("Error extracting save game!"));
		return;
	}

	memset(&save, 0, sizeof(save_entry_t));
	strncpy(titleid, (char*) sfo_get_param_value(sfo, "TITLE_ID"), sizeof(titleid));
	strncpy(dirname, (char*) sfo_get_param_value(sfo, "SAVEDATA_DIRECTORY"), sizeof(dirname));
	save.blocks = *((uint32_t*) sfo_get_param_value(sfo, "SAVEDATA_BLOCKS"));
	save.title_id = titleid;
	save.dir_name = dirname;
	sfo_free(sfo);

	snprintf(path, sizeof(path), SAVES_PATH_HDD "%s/%s.bin", apollo_config.user_id, save.title_id, save.dir_name);
	if (file_exists(path) == SUCCESS)
	{
		if (!show_dialog(DIALOG_TYPE_YESNO, "%s\n%s/%s\n\n%s", _("Save game already exists:"), save.title_id, save.dir_name, _("Overwrite?")))
			return;

		if (!orbis_SaveMount(&save, ORBIS_SAVE_DATA_MOUNT_MODE_RDWR, mount))
		{
			show_message(_("Error mounting save game folder!"));
			return;
		}
	}
	else if (!orbis_SaveMount(&save, ORBIS_SAVE_DATA_MOUNT_MODE_RDWR | ORBIS_SAVE_DATA_MOUNT_MODE_CREATE2 | ORBIS_SAVE_DATA_MOUNT_MODE_COPY_ICON, mount))
	{
		show_message(_("Error creating save game folder!"));
		return;
	}

	snprintf(path, sizeof(path), APOLLO_SANDBOX_PATH, "~");
	*strrchr(path, '~') = 0;

	if (!extract_zip(APOLLO_LOCAL_CACHE "tmpsave.zip", path))
	{
		show_message(_("Error extracting save game!"));
		orbis_SaveUmount(mount);
		return;
	}

	unlink_secure(APOLLO_LOCAL_CACHE "tmpsave.zip");

	snprintf(path, sizeof(path), APOLLO_SANDBOX_PATH "sce_sys/", mount);
	if (_update_save_details(path, &save))
		show_message("%s\n%s/%s", _("Save game successfully downloaded to:"), save.title_id, save.dir_name);
	else
		show_message("%s\n%s/%s", _("Error! Can't update save game:"), save.title_id, save.dir_name);

	orbis_SaveUmount(mount);
}

static int _copy_save_hdd(const save_entry_t* save)
{
	int ret;
	char copy_path[256];
	char mount[ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE];
	sfo_patch_t patch = {
		.user_id = apollo_config.user_id,
		.account_id = apollo_config.account_id,
	};

	if (!orbis_SaveMount(save, ORBIS_SAVE_DATA_MOUNT_MODE_RDWR | ORBIS_SAVE_DATA_MOUNT_MODE_CREATE2 | ORBIS_SAVE_DATA_MOUNT_MODE_COPY_ICON, mount))
		return 0;

	snprintf(copy_path, sizeof(copy_path), APOLLO_SANDBOX_PATH, mount);

	LOG("Copying <%s> to %s...", save->path, copy_path);
	ret = copy_directory(save->path, save->path, copy_path);

	snprintf(copy_path, sizeof(copy_path), "%s" "sce_sys/", save->path);
	_update_save_details(copy_path, save);

	snprintf(copy_path, sizeof(copy_path), APOLLO_SANDBOX_PATH "sce_sys/param.sfo", mount);
	patch_sfo(copy_path, &patch);
	orbis_SaveUmount(mount);

	return (ret == SUCCESS);
}

static int _copy_save_pfs(const save_entry_t* save)
{
	char src_path[256];
	char hdd_path[256];
	char mount[ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE];
	sfo_patch_t patch = {
		.user_id = apollo_config.user_id,
		.account_id = apollo_config.account_id,
	};

	snprintf(src_path, sizeof(src_path), "%s%s.bin", save->path, save->dir_name);
	if ((read_file(src_path, (uint8_t*) mount, 0x10) < 0) || get_max_pfskey_ver() < mount[8])
	{
		LOG("Error: Encrypted save Required firmware: %s", get_fw_by_pfskey_ver(mount[8]));
		return (0x80001000 | mount[8]);
	}

	if (!orbis_SaveMount(save, ORBIS_SAVE_DATA_MOUNT_MODE_RDWR | ORBIS_SAVE_DATA_MOUNT_MODE_CREATE2 | ORBIS_SAVE_DATA_MOUNT_MODE_COPY_ICON, mount))
	{
		LOG("Error: can't create HDD save");
		return -1;
	}
	orbis_SaveUmount(mount);

	// Copy the sdimg file
	snprintf(src_path, sizeof(src_path), "%s%s", save->path, save->dir_name);
	snprintf(hdd_path, sizeof(hdd_path), SAVES_PATH_HDD "%s/sdimg_%s", apollo_config.user_id, save->title_id, save->dir_name);
	LOG("Copying <%s> to %s...", src_path, hdd_path);

	if (copy_file(src_path, hdd_path) != SUCCESS)
	{
		LOG("Error: can't copy %s", hdd_path);
		return -2;
	}

	// Copy the .bin file
	snprintf(src_path, sizeof(src_path), "%s%s.bin", save->path, save->dir_name);
	snprintf(hdd_path, sizeof(hdd_path), SAVES_PATH_HDD "%s/%s.bin", apollo_config.user_id, save->title_id, save->dir_name);
	LOG("Copying <%s> to %s...", src_path, hdd_path);

	if (copy_file(src_path, hdd_path) != SUCCESS)
	{
		LOG("Error: can't copy %s", hdd_path);
		return -3;
	}

	// Now remount from HDD to patch SFO
	if (!orbis_SaveMount(save, ORBIS_SAVE_DATA_MOUNT_MODE_RDWR, mount))
	{
		LOG("Error! Can't mount encrypted save.");
		return -4;
	}

	snprintf(hdd_path, sizeof(hdd_path), APOLLO_SANDBOX_PATH "sce_sys/param.sfo", mount);
	patch_sfo(hdd_path, &patch);

	*strrchr(hdd_path, 'p') = 0;
	_update_save_details(hdd_path, save);
	orbis_SaveUmount(mount);

	LOG("Encrypted save copied: %s/%s", save->title_id, save->dir_name);
	return SUCCESS;
}

static void copySaveHDD(const save_entry_t* save)
{
	//source save is already on HDD
	if (save->flags & SAVE_FLAG_HDD)
	{
		show_message("%s\n%s", _("Copy operation cancelled!"), _("Same source and destination."));
		return;
	}

	init_loading_screen(_("Copying save game..."));
	int ret = _copy_save_hdd(save);
	stop_loading_screen();

	if (ret)
		show_message("%s\n%s/%s", _("Files successfully copied to:"), save->title_id, save->dir_name);
	else
		show_message("%s\n%s/%s", _("Error! Can't copy Save-game folder:"), save->title_id, save->dir_name);
}

static void copyAllSavesHDD(const save_entry_t* save, int all)
{
	int done = 0, err_count = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ((void**)save->dir_name)[0];

	init_progress_bar(_("Copying all saves..."));

	LOG("Copying all saves from '%s' to HDD...", save->path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);

		if (item->type != FILE_TYPE_PS4 || !(all || (item->flags & SAVE_FLAG_SELECTED)))
			continue;

		if (item->flags & SAVE_FLAG_LOCKED)
			(_copy_save_pfs(item) == SUCCESS) ? done++ : err_count++;
		else
			_copy_save_hdd(item) ? done++ : err_count++;
	}

	end_progress_bar();

	show_message("%d/%d %s", done, done+err_count, _("Saves copied to HDD"));
}

void extractArchive(const char* file_path)
{
	int ret = 0;
	char exp_path[256];

	strncpy(exp_path, file_path, sizeof(exp_path));
	*strrchr(exp_path, '.') = 0;

	switch (strrchr(file_path, '.')[1])
	{
	case 'z':
	case 'Z':
		/* ZIP */
		strcat(exp_path, "/");
		ret = extract_zip(file_path, exp_path);
		break;

	case 'r':
	case 'R':
		/* RAR */
		ret = extract_rar(file_path, exp_path);
		break;

	case '7':
		/* 7-Zip */
		ret = extract_7zip(file_path, exp_path);
		break;

	default:
		break;
	}

	if (ret)
		show_message("%s\n%s", _("All files extracted to:"), exp_path);
	else
		show_message(_("Error: %s couldn't be extracted"), file_path);
}

static void exportFingerprint(const save_entry_t* save, int silent)
{
	char fpath[256];
	uint8_t buffer[0x60];

	snprintf(fpath, sizeof(fpath), "%ssce_sys/keystone", save->path);
	LOG("Reading '%s' ...", fpath);

	if (read_file(fpath, buffer, sizeof(buffer)) != SUCCESS)
	{
		if (!silent) show_message("%s\n%s", _("Error! Keystone file is not available:"), fpath);
		return;
	}

	for (int i = 0; i < 0x20; i++)
		snprintf(((char*)buffer) + (i * 2), 3, "%02x", buffer[i + 0x20]);

	if (!silent)
	{
		show_message("%s %s\n%s", save->title_id, _("keystone fingerprint:"), buffer);
		return;
	}

	snprintf(fpath, sizeof(fpath), APOLLO_PATH "fingerprints.txt");
	FILE *fp = fopen(fpath, "a");
	if (!fp)
		return;

	fprintf(fp, "%s=%s\n", save->title_id, buffer);
	fclose(fp);
}

static void toggleTrophy(const save_entry_t* entry)
{
	int ret = 1;
	int *trophy_id;
	code_entry_t* code;
	list_node_t* node;

	init_loading_screen(_("Applying changes..."));

	for (node = list_head(entry->codes); (code = list_get(node)); node = list_next(node))
	{
		if (!code->activated || (code->type != PATCH_TROP_UNLOCK && code->type != PATCH_TROP_LOCK))
			continue;

		trophy_id = (int*)(code->file);
		LOG("Active code: [%d] '%s'", trophy_id[0], code->name+2);

		if (code->type == PATCH_TROP_UNLOCK)
		{
			ret &= trophy_unlock(entry, trophy_id[0], trophy_id[1], code->name[0]);
			code->type = PATCH_TROP_LOCK;
			code->name[1] = ' ';
		}
		else
		{
			ret &= trophy_lock(entry, trophy_id[0], trophy_id[1], code->name[0]);
			code->type = PATCH_TROP_UNLOCK;
			code->name[1] = CHAR_TAG_LOCKED;
		}

		code->activated = 0;
	}

	stop_loading_screen();

	if(ret)
		show_message(_("Trophies successfully updated!"));
	else
		show_message(_("Error! Couldn't update trophies"));
}

static void exportTrophiesZip(const char* exp_path)
{
	char* export_file;
	char* trp_path;
	char* tmp;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), exp_path);
		return;
	}

	init_loading_screen(_("Exporting Trophies..."));

	asprintf(&export_file, "%s" "trophies_%08x.zip", exp_path, apollo_config.user_id);
	asprintf(&trp_path, TROPHY_PATH_HDD, apollo_config.user_id);

	tmp = strdup(trp_path);
	*strrchr(tmp, '/') = 0;

	zip_directory(tmp, trp_path, export_file);

	sprintf(export_file, "%s%08x.xml", exp_path, apollo_config.user_id);
	save_xml_owner(export_file);

	free(export_file);
	free(trp_path);
	free(tmp);

	stop_loading_screen();
	show_message("%s\n%strophies_%08d.zip", _("Trophies successfully saved to:"), exp_path, apollo_config.user_id);
}

static void dumpAllFingerprints(const save_entry_t* save)
{
	char mount[ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE];
	uint64_t progress = 0;
	list_node_t *node;
	save_entry_t *item;
	list_t *list = ((void**)save->dir_name)[0];

	init_progress_bar(_("Dumping all fingerprints..."));

	LOG("Dumping all fingerprints from '%s'...", save->path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (item->type != FILE_TYPE_PS4 || (item->flags & SAVE_FLAG_LOCKED))
			continue;

		if (item->flags & SAVE_FLAG_HDD)
		{
			if (!orbis_SaveMount(item, ORBIS_SAVE_DATA_MOUNT_MODE_RDONLY, mount))
				continue;

			free(item->path);
			asprintf(&item->path, APOLLO_SANDBOX_PATH, mount);
		}

		exportFingerprint(item, 1);

		if (item->flags & SAVE_FLAG_HDD)
			orbis_SaveUmount(mount);
	}

	end_progress_bar();
	show_message("%s\n%sfingerprints.txt", _("All fingerprints dumped to:"), APOLLO_PATH);
}

static void activateAccount(int user)
{
	uint64_t account = 0;
	char value[SFO_ACCOUNT_ID_SIZE*2+1];

	snprintf(value, sizeof(value), "%016lx", 0x6F6C6C6F70610000 + (~apollo_config.user_id & 0xFFFF));
	if (!osk_dialog_get_text(_("Enter the Account ID"), value, sizeof(value)))
		return;

	if (!sscanf(value, "%lx", &account))
	{
		show_message(_("Error! Account ID is not valid"));
		return;
	};

	LOG("Activating user=%d (%lx)...", user, account);
	if (regMgr_SetAccountId(user, &account) != SUCCESS)
	{
		show_message(_("Error! Couldn't activate user account"));
		return;
	}

	show_message("%s\n%s", _("Account successfully activated!"), _("A system reboot is required"));
}

static void copySavePFS(const save_entry_t* save)
{
	char hdd_path[256];

	snprintf(hdd_path, sizeof(hdd_path), SAVES_PATH_HDD "%s/%s.bin", apollo_config.user_id, save->title_id, save->dir_name);
	if (file_exists(hdd_path) == SUCCESS && !show_dialog(DIALOG_TYPE_YESNO,
		"%s\n%s/%s\n\n%s", _("Save game already exists:"), save->title_id, save->dir_name, _("Overwrite?")))
		return;

	switch (_copy_save_pfs(save))
	{
	case SUCCESS:
		show_message("%s\n%s/%s", _("Encrypted save copied successfully!"), save->title_id, save->dir_name);
		return;
	
	case -1:
		show_message(_("Error: can't create HDD save"));
		return;

	case -2:
		show_message("%s\n%s%s", _("Error: can't copy file"), save->path, save->dir_name);
		return;

	case -3:
		show_message("%s\n%s%s.bin", _("Error: can't copy file"), save->path, save->dir_name);
		return;

	case -4:
		show_message("%s\n%s", _("Error! Can't mount encrypted save."), _("(incompatible save-game firmware version)"));
		return;

	default:
		show_message(_("Error: can't copy save %s"), save->title_id);
		return;
	}
}

static void copyKeystone(const save_entry_t* entry, int import)
{
	char path_data[256];
	char path_save[256];

	snprintf(path_save, sizeof(path_save), "%ssce_sys/keystone", entry->path);
	snprintf(path_data, sizeof(path_data), APOLLO_USER_PATH "%s/keystone", apollo_config.user_id, entry->title_id);
	mkdirs(path_data);

	// try to import keystone from data folder
	if (import && file_exists(path_data) != SUCCESS)
		snprintf(path_data, sizeof(path_data), APOLLO_DATA_PATH "%s.keystone", entry->title_id);

	LOG("Copy '%s' <-> '%s'...", path_save, path_data);

	if (copy_file(import ? path_data : path_save, import ? path_save : path_data) == SUCCESS)
		show_message("%s\n%s", _("Keystone successfully copied to:"), import ? path_save : path_data);
	else
		show_message(_("Error! Keystone couldn't be copied"));
}

static int webReqHandler(dWebRequest_t* req, dWebResponse_t* res, void* list)
{
	list_node_t *node;
	save_entry_t *item;

	// http://ps3-ip:8080/
	if (strcmp(req->resource, "/") == 0)
	{
		uint64_t hash[2];
		md5_context ctx;

		md5_starts(&ctx);
		for (node = list_head(list); (item = list_get(node)); node = list_next(node))
			md5_update(&ctx, (uint8_t*) item->name, strlen(item->name));

		md5_finish(&ctx, (uint8_t*) hash);
		asprintf(&res->data, APOLLO_LOCAL_CACHE "web%016lx%016lx.html", hash[0], hash[1]);

		if (file_exists(res->data) == SUCCESS)
			return 1;

		FILE* f = fopen(res->data, "w");
		if (!f)
			return 0;

		fprintf(f, "<html><head><meta charset=\"UTF-8\"><style>h1, h2 { font-family: arial; } img { display: none; } table { border-collapse: collapse; margin: 25px 0; font-size: 0.9em; font-family: sans-serif; min-width: 400px; box-shadow: 0 0 20px rgba(0, 0, 0, 0.15); } table thead tr { background-color: #009879; color: #ffffff; text-align: left; } table th, td { padding: 12px 15px; } table tbody tr { border-bottom: 1px solid #dddddd; } table tbody tr:nth-of-type(even) { background-color: #f3f3f3; } table tbody tr:last-of-type { border-bottom: 2px solid #009879; }</style>");
		fprintf(f, "<script language=\"javascript\">function show(sid,src){var im=document.getElementById('img'+sid);im.src=src;im.style.display='block';document.getElementById('btn'+sid).style.display='none';}</script>");
		fprintf(f, "<title>Apollo Save Tool</title></head><body><h1>.:: Apollo Save Tool</h1><h2>Index of %s</h2><table><thead><tr><th>Name</th><th>Icon</th><th>Title ID</th><th>Folder</th><th>Location</th></tr></thead><tbody>", selected_entry->path);

		int i = 0;
		for (node = list_head(list); (item = list_get(node)); node = list_next(node), i++)
		{
			if (item->type == FILE_TYPE_MENU || !(item->flags & SAVE_FLAG_PS4) || item->flags & SAVE_FLAG_LOCKED)
				continue;

			fprintf(f, "<tr><td><a href=\"/zip/%08d/%s_%s.zip\">%s</a></td>", i, item->title_id, item->dir_name, item->name);
			fprintf(f, "<td><button type=\"button\" id=\"btn%d\" onclick=\"show(%d,'", i, i);

			if (item->flags & SAVE_FLAG_HDD)
				fprintf(f, "/icon/%s/%s_icon0.png", item->title_id, item->dir_name);
			else
				fprintf(f, "/icon%ssce_sys/icon0.png", strchr(item->path +20, '/'));

			fprintf(f, "')\">Show Icon</button><img id=\"img%d\" alt=\"%s\" width=\"228\" height=\"128\"></td>", i, item->name);
			fprintf(f, "<td>%s</td>", item->title_id);
			fprintf(f, "<td>%s</td>", item->dir_name);
			fprintf(f, "<td>%s</td></tr>", (item->flags & SAVE_FLAG_HDD) ? "HDD" : "USB");
		}

		fprintf(f, "</tbody></table></body></html>");
		fclose(f);
		return 1;
	}

	// http://ps4-ip:8080/PS4/games.txt
	if (wildcard_match(req->resource, "/PS4/games.txt"))
	{
		asprintf(&res->data, "%s%s", APOLLO_LOCAL_CACHE, "ps4_games.txt");

		char *last = "";
		FILE* f = fopen(res->data, "w");
		if (!f)
			return 0;

		for (node = list_head(list); (item = list_get(node)); node = list_next(node))
		{
			if (item->type == FILE_TYPE_MENU || !(item->flags & SAVE_FLAG_PS4) || item->flags & SAVE_FLAG_LOCKED ||
				strcmp(last, item->title_id) == 0)
				continue;

			fprintf(f, "%s=%s\n", item->title_id, item->name);
			last = item->title_id;
		}

		fclose(f);
		return 1;
	}

	// http://ps4-ip:8080/PS4/BLUS12345/saves.txt
	if (wildcard_match(req->resource, "/PS4/\?\?\?\?\?\?\?\?\?/saves.txt"))
	{
		asprintf(&res->data, "%sweb%.9s_saves.txt", APOLLO_LOCAL_CACHE, req->resource + 5);

		FILE* f = fopen(res->data, "w");
		if (!f)
			return 0;

		int i = 0;
		for (node = list_head(list); (item = list_get(node)); node = list_next(node), i++)
		{
			if (item->type == FILE_TYPE_MENU || !(item->flags & SAVE_FLAG_PS4) || strncmp(item->title_id, req->resource + 5, 9))
				continue;

			fprintf(f, "%08d.zip=(%s) %s\n", i, item->dir_name, item->name);
		}

		fclose(f);
		return 1;
	}

	// http://ps3-ip:8080/zip/00000000/CUSA12345_DIR-NAME.zip
	// http://ps4-ip:8080/PS4/BLUS12345/00000000.zip
	if (wildcard_match(req->resource, "/zip/\?\?\?\?\?\?\?\?/\?\?\?\?\?\?\?\?\?_*.zip") ||
		wildcard_match(req->resource, "/PS4/\?\?\?\?\?\?\?\?\?/*.zip"))
	{
		char mount[ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE];
		char *base, *path;
		int id = 0;

		sscanf(req->resource + (strncmp(req->resource, "/PS4", 4) == 0 ? 15 : 5), "%08d", &id);
		item = list_get_item(list, id);
		asprintf(&res->data, "%s%s_%s.zip", APOLLO_LOCAL_CACHE, item->title_id, item->dir_name);

		if (item->flags & SAVE_FLAG_HDD)
		{
			if (!orbis_SaveMount(item, ORBIS_SAVE_DATA_MOUNT_MODE_RDONLY, mount))
				return 0;

			asprintf(&path, APOLLO_SANDBOX_PATH, mount);
			asprintf(&base, APOLLO_SANDBOX_PATH, "");
		}
		else
		{
			base = strdup(item->path);
			path = strdup(item->path);
		}
		*strrchr(base, '/') = 0;
		*strrchr(base, '/') = 0;

		id = zip_directory(base, path, res->data);
		if (item->flags & SAVE_FLAG_HDD)
			orbis_SaveUmount(mount);

		free(base);
		free(path);
		return id;
	}

	// http://ps4-ip:8080/PS4/BLUS12345/icon0.png
	if (wildcard_match(req->resource, "/PS4/\?\?\?\?\?\?\?\?\?/icon0.png"))
	{
		for (node = list_head(list); (item = list_get(node)); node = list_next(node))
		{
			if (item->type == FILE_TYPE_MENU || !(item->flags & SAVE_FLAG_PS4) || strncmp(item->title_id, req->resource + 5, 9))
				continue;

			if (item->flags & SAVE_FLAG_HDD)
				asprintf(&res->data, SAVE_ICON_PATH_HDD "%s/%s_icon0.png", apollo_config.user_id, item->title_id, item->dir_name);
			else
				asprintf(&res->data, "%ssce_sys/icon0.png", item->path);

			return (file_exists(res->data) == SUCCESS);
		}

		return 0;
	}

	// http://ps3-ip:8080/icon/CUSA12345-DIR-NAME/sce_sys/icon0.png
	if (wildcard_match(req->resource, "/icon/*/sce_sys/icon0.png"))
	{
		asprintf(&res->data, "%sPS4/APOLLO/%s", selected_entry->path, req->resource + 6);
		return (file_exists(res->data) == SUCCESS);
	}

	// http://ps3-ip:8080/icon/CUSA12345/DIR-NAME_icon0.png
	if (wildcard_match(req->resource, "/icon/\?\?\?\?\?\?\?\?\?/*_icon0.png"))
	{
		asprintf(&res->data, SAVE_ICON_PATH_HDD "%s", apollo_config.user_id, req->resource + 6);
		return (file_exists(res->data) == SUCCESS);
	}

	return 0;
}

static void enableWebServer(dWebReqHandler_t handler, void* data, int port)
{
	OrbisNetCtlInfo info;

	memset(&info, 0, sizeof(OrbisNetCtlInfo));
	sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_IP_ADDRESS, &info);
	LOG("Starting local web server %s:%d ...", info.ip_address, port);

	if (dbg_webserver_start(port, handler, data))
	{
		show_message("%s http://%s:%d\n%s", _("Web Server listening on:"), info.ip_address, port, _("Press OK to stop the Server."));
		dbg_webserver_stop();
	}
	else show_message(_("Error starting Web Server!"));
}

static void copyAllSavesUSB(const save_entry_t* save, const char* dst_path, int all)
{
	int done = 0, err_count = 0;
	char copy_path[256];
	char save_path[256];
	char mount[ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE];
	uint64_t progress = 0;
	list_node_t *node;
	save_entry_t *item;
	list_t *list = ((void**)save->dir_name)[0];

	if (!list || mkdirs(dst_path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), dst_path);
		return;
	}

	init_progress_bar(_("Copying all saves..."));

	LOG("Copying all saves to '%s'...", dst_path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (!(item->type == FILE_TYPE_PS4 || item->type == FILE_TYPE_TRP) ||
			!(all || item->flags & SAVE_FLAG_SELECTED) ||
			!orbis_SaveMount(item, (item->flags & SAVE_FLAG_TROPHY), mount))
			continue;

		snprintf(save_path, sizeof(save_path), APOLLO_SANDBOX_PATH, mount);
		snprintf(copy_path, sizeof(copy_path), "%s%08x_%s_%s/", dst_path, apollo_config.user_id, item->title_id, item->dir_name);

		LOG("Copying <%s> to %s...", save_path, copy_path);
		(copy_directory(save_path, save_path, copy_path) == SUCCESS) ? done++ : err_count++;

		orbis_SaveUmount(mount);
	}

	end_progress_bar();
	show_message("%d/%d %s\n%s", done, done+err_count, _("Saves copied to:"), dst_path);
}

static void exportAllSavesVMC(const save_entry_t* save, int dev, int all)
{
	char outPath[256];
	int done = 0, err_count = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ((void**)save->dir_name)[0];

	init_progress_bar(_("Exporting all VMC saves..."));
	_set_dest_path(outPath, dev, PS1_SAVES_PATH_USB);
	mkdirs(outPath);

	LOG("Exporting all saves from '%s' to %s...", save->path, outPath);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (!all && !(item->flags & SAVE_FLAG_SELECTED))
			continue;

		if (item->type == FILE_TYPE_PS1)
			(saveSingleSave(outPath, save->blocks, PS1SAVE_PSV) ? done++ : err_count++);

		if (item->type == FILE_TYPE_PS2)
			(vmc_export_psv(item->dir_name, outPath) ? done++ : err_count++);
	}

	end_progress_bar();

	show_message("%d/%d %s\n%s", done, done+err_count, _("Saves exported to:"), outPath);
}

static void exportVmcSave(const save_entry_t* save, int type, int dst_id)
{
	char outPath[256];
	struct tm t;

	_set_dest_path(outPath, dst_id, (type == PS1SAVE_PSV) ? PSV_SAVES_PATH_USB : PS1_SAVES_PATH_USB);
	mkdirs(outPath);
	if (type != PS1SAVE_PSV)
	{
		// build file path
		gmtime_r(&(time_t){time(NULL)}, &t);
		sprintf(strrchr(outPath, '/'), "/%s_%d-%02d-%02d_%02d%02d%02d.%s", save->title_id,
			t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec,
			(type == PS1SAVE_MCS) ? "mcs" : "psx");
	}

	if (saveSingleSave(outPath, save->blocks, type))
		show_message("%s\n%s", _("Save successfully exported to:"), outPath);
	else
		show_message("%s\n%s", _("Error exporting save:"), save->path);
}

static void export_ps1vmc(const char* vm1_file, int dst, int vmp)
{
	char dstfile[256];
	char dst_path[256];

	_set_dest_path(dst_path, dst, VMC_PS1_PATH_USB);
	if (mkdirs(dst_path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), dst_path);
		return;
	}

	snprintf(dstfile, sizeof(dstfile), "%s%s.%s", dst_path, vm1_file, vmp ? "VMP" : "VM1");

	if (saveMemoryCard(dstfile, vmp ? PS1CARD_VMP : PS1CARD_RAW, 0))
		show_message("%s\n%s", _("Memory card successfully exported to:"), dstfile);
	else
		show_message(_("Error! Failed to export PS1 memory card"));
}

static void export_vmc2save(const save_entry_t* save, int type, int dst_id)
{
	int ret = 0;
	char outPath[256];
	struct tm t;

	_set_dest_path(outPath, dst_id, (type == FILE_TYPE_PSV) ? PSV_SAVES_PATH_USB : PS2_SAVES_PATH_USB);
	mkdirs(outPath);
	if (type != FILE_TYPE_PSV)
	{
		// build file path
		gmtime_r(&(time_t){time(NULL)}, &t);
		sprintf(strrchr(outPath, '/'), "/%s_%d-%02d-%02d_%02d%02d%02d.psu", save->title_id,
			t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	}

	switch (type)
	{
	case FILE_TYPE_PSV:
		ret = vmc_export_psv(save->dir_name, outPath);
		break;

	case FILE_TYPE_PSU:
		ret = vmc_export_psu(save->dir_name, outPath);
		break;

	default:
		break;
	}

	if (ret)
		show_message("%s\n%s", _("Save successfully exported to:"), outPath);
	else
		show_message("%s\n%s", _("Error exporting save:"), save->path);
}

static void import_save2vmc(const char* src, int type)
{
	int ret = 0;

	init_loading_screen(_("Importing PS2 save..."));
	switch (type)
	{
	case FILE_TYPE_PSV:
		ret = vmc_import_psv(src);
		break;

	case FILE_TYPE_PSU:
		ret = vmc_import_psu(src);
		break;

	case FILE_TYPE_XPS:
		ret = ps2_xps2psv(src, APOLLO_LOCAL_CACHE "TEMP.PSV") && vmc_import_psv(APOLLO_LOCAL_CACHE "TEMP.PSV");
		unlink_secure(APOLLO_LOCAL_CACHE "TEMP.PSV");
		break;

	case FILE_TYPE_CBS:
		ret = ps2_cbs2psv(src, APOLLO_LOCAL_CACHE "TEMP.PSV") && vmc_import_psv(APOLLO_LOCAL_CACHE "TEMP.PSV");
		unlink_secure(APOLLO_LOCAL_CACHE "TEMP.PSV");
		break;

	case FILE_TYPE_MAX:
		ret = ps2_max2psv(src, APOLLO_LOCAL_CACHE "TEMP.PSV") && vmc_import_psv(APOLLO_LOCAL_CACHE "TEMP.PSV");
		unlink_secure(APOLLO_LOCAL_CACHE "TEMP.PSV");
		break;

	default:
		break;
	}
	stop_loading_screen();

	if (ret)
		show_message("%s\n%s", _("Successfully imported to VMC:"), src);
	else
		show_message("%s\n%s", _("Error importing save:"), src);
}

static int deleteSave(const save_entry_t* save)
{
	int ret = 0;
	char fpath[256];

	if (!show_dialog(DIALOG_TYPE_YESNO, _("Do you want to delete %s?"), save->dir_name))
		return 0;

	if (save->flags & SAVE_FLAG_PS1)
		ret = formatSave(save->blocks);
	
	else if (save->flags & SAVE_FLAG_PS2)
		ret = vmc_delete_save(save->dir_name);

	else if (save->flags & SAVE_FLAG_TROPHY)
		ret = trophySet_delete(save);

	else if (save->flags & SAVE_FLAG_PS4)
	{
		if (save->flags & SAVE_FLAG_HDD)
			ret = orbis_SaveDelete(save);

		else if (save->flags & SAVE_FLAG_LOCKED)
		{
			snprintf(fpath, sizeof(fpath), "%s%s.bin", save->path, save->dir_name);
			ret = (unlink_secure(fpath) == SUCCESS);

			snprintf(fpath, sizeof(fpath), "%s%s", save->path, save->dir_name);
			ret &= (unlink_secure(fpath) == SUCCESS);
		}
		else
			ret = (clean_directory(save->path, "") == SUCCESS);
	}

	if (ret)
		show_message("%s\n%s", _("Save successfully deleted:"), save->dir_name);
	else
		show_message("%s\n%s", _("Error! Couldn't delete save:"), save->dir_name);

	return ret;
}

static char* get_json_title_name(const char *fname)
{
	char *ptr, *ret = NULL;
	char *json = readTextFile(fname);

	if (!json)
		return NULL;

	ptr = strstr(json, "\"name\":");
	if (ptr && (ret = strchr(ptr + 8, '"')) != NULL)
	{
		*ret = 0;
		ret = strdup(ptr + 8);
	}

	free(json);
	return ret;
}

static char* get_title_name_icon(const save_entry_t* item)
{
	char *ret = NULL;
	char tmdb_url[256];
	char local_file[256];

	LOG("Getting data for '%s'...", item->title_id);

	if (get_name_title_id(item->title_id, tmdb_url))
		ret = strdup(tmdb_url);
	else if (!snprintf(local_file, sizeof(local_file), APOLLO_LOCAL_CACHE "json.ftp") ||
		!snprintf(tmdb_url, sizeof(tmdb_url), "https://bucanero.github.io/psndb/%s/%s_00.json", item->title_id, item->title_id) ||
		!http_download(tmdb_url, "", local_file, 0) || (ret = get_json_title_name(local_file)) == NULL)
	{
		sfo_context_t* sfo = sfo_alloc();
		snprintf(local_file, sizeof(local_file), "%ssce_sys/param.sfo", item->path);

		// get the title name from the SFO
		ret = strdup((sfo_read(sfo, local_file) < 0) ? item->name : (char*) sfo_get_param_value(sfo, "MAINTITLE"));
		sfo_free(sfo);
	}

	LOG("Get PS%d icon %s (%s)", item->type, item->title_id, ret);
	snprintf(local_file, sizeof(local_file), APOLLO_LOCAL_CACHE "%.9s.PNG", item->title_id);
	if (file_exists(local_file) == SUCCESS)
		return ret;

	snprintf(tmdb_url, sizeof(tmdb_url), SAVE_ICON_PATH_HDD "%s/%s_icon0.png", apollo_config.user_id, item->title_id, item->dir_name);
	copy_file(tmdb_url, local_file);

	return ret;
}

static char* get_title_icon_psx(const save_entry_t* entry)
{
	FILE* fp;
	uint8_t* icon = NULL;
	char *ret = NULL;
	char path[256];

	LOG("Getting data for '%s'...", entry->title_id);
	snprintf(path, sizeof(path), APOLLO_DATA_PATH "ps%dtitleid.txt", entry->type);
	fp = fopen(path, "r");
	if (fp)
	{
		while(!ret && fgets(path, sizeof(path), fp))
		{
			if (strncmp(path, entry->title_id, 9) != 0)
				continue;

			path[strlen(path)-1] = 0;
			ret = strdup(path+10);
		}
		fclose(fp);
	}

	if (!ret)
		ret = strdup(entry->name);

	LOG("Get PS%d icon %s (%s)", entry->type, entry->title_id, ret);
	snprintf(path, sizeof(path), APOLLO_LOCAL_CACHE "%.9s.PNG", entry->title_id);
	if (file_exists(path) == SUCCESS)
		return ret;

	fp = fopen(path, "wb");
	if (entry->type == FILE_TYPE_PS1)
	{
		icon = getIconRGBA(entry->blocks, 0);
		svpng(fp, 16, 16, icon, 1);
	}
	else
	{
		icon = getIconPS2(entry->dir_name, strrchr(entry->path, '\n')+1);
		svpng(fp, 128, 128, icon, 1);
	}
	free(icon);
	fclose(fp);

	return ret;
}

static void uploadSaveFTP(const save_entry_t* save)
{
	FILE* fp;
	char *tmp;
	char remote[256];
	char local[256];
	int ret = 0;
	struct tm t = get_local_time();

	if (!show_dialog(DIALOG_TYPE_YESNO, _("Do you want to upload %s?"), save->dir_name))
		return;

	init_loading_screen(_("Sync with FTP Server..."));

	snprintf(remote, sizeof(remote), "%s%016" PRIX64 "/PS%d/", apollo_config.ftp_url, apollo_config.account_id, save->type);
	http_download(remote, "games.txt", APOLLO_LOCAL_CACHE "games.ftp", 0);

	snprintf(remote, sizeof(remote), "%s%016" PRIX64 "/PS%d/%s/", apollo_config.ftp_url, apollo_config.account_id, save->type, save->title_id);
	http_download(remote, "saves.txt", APOLLO_LOCAL_CACHE "saves.ftp", 0);
	http_download(remote, "checksum.sfv", APOLLO_LOCAL_CACHE "sfv.ftp", 0);

	snprintf(local, sizeof(local), APOLLO_LOCAL_CACHE "%s_%d-%02d-%02d-%02d%02d%02d.zip",
			(save->type == FILE_TYPE_PS4) ? save->dir_name : save->title_id,
			t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);

	if (save->type == FILE_TYPE_PS4)
	{
		tmp = strdup(save->path);
		*strrchr(tmp, '/') = 0;
		*strrchr(tmp, '/') = 0;
	
		ret = zip_directory(tmp, save->path, local);
		free(tmp);
	}
	else
	{
		tmp = malloc(256);
		if (save->type == FILE_TYPE_PS2)
			ret = vmc_export_psv(save->dir_name, APOLLO_LOCAL_CACHE);
		else
			ret = saveSingleSave(APOLLO_LOCAL_CACHE, save->blocks, PS1SAVE_PSV);

		get_psv_filename(tmp, APOLLO_LOCAL_CACHE, save->dir_name);
		ret &= zip_file(tmp, local);
		unlink_secure(tmp);
		free(tmp);
	}

	stop_loading_screen();
	if (!ret)
	{
		show_message("%s\n%s", _("Error! Couldn't zip save:"), save->dir_name);
		return;
	}

	tmp = strrchr(local, '/')+1;
	uint32_t crc = file_crc32(local);

	LOG("Updating %s save index...", save->title_id);
	fp = fopen(APOLLO_LOCAL_CACHE "saves.ftp", "a");
	if (fp)
	{
		fprintf(fp, "%s=[%s] %d-%02d-%02d %02d:%02d:%02d %s (CRC: %08X)\r\n", tmp, save->dir_name,
				t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec, save->name, crc);
		fclose(fp);
	}

	LOG("Updating .sfv CRC32: %08X", crc);
	fp = fopen(APOLLO_LOCAL_CACHE "sfv.ftp", "a");
	if (fp)
	{
		fprintf(fp, "%s %08X\n", tmp, crc);
		fclose(fp);
	}

	ret = ftp_upload(local, remote, tmp, 1);
	ret &= ftp_upload(APOLLO_LOCAL_CACHE "saves.ftp", remote, "saves.txt", 1);
	ret &= ftp_upload(APOLLO_LOCAL_CACHE "sfv.ftp", remote, "checksum.sfv", 1);

	unlink_secure(local);
	tmp = readTextFile(APOLLO_LOCAL_CACHE "games.ftp");
	if (!tmp)
		tmp = strdup("");

	if (strstr(tmp, save->title_id) == NULL)
	{
		init_loading_screen(_("Updating games index..."));
		free(tmp);
		tmp = (save->type == FILE_TYPE_PS4) ? get_title_name_icon(save) : get_title_icon_psx(save);
		stop_loading_screen();

		snprintf(local, sizeof(local), APOLLO_LOCAL_CACHE "%.9s.PNG", save->title_id);
		ret &= ftp_upload(local, remote, "icon0.png", 1);

		fp = fopen(APOLLO_LOCAL_CACHE "games.ftp", "a");
		if (fp)
		{
			fprintf(fp, "%s=%s\r\n", save->title_id, tmp);
			fclose(fp);
		}

		snprintf(remote, sizeof(remote), "%s%016" PRIX64 "/PS%d/", apollo_config.ftp_url, apollo_config.account_id, save->type);
		ret &= ftp_upload(APOLLO_LOCAL_CACHE "games.ftp", remote, "games.txt", 1);
	}
	free(tmp);
	clean_directory(APOLLO_LOCAL_CACHE, ".ftp");

	if (ret)
		show_message("%s\n%s", _("Save successfully uploaded:"), save->dir_name);
	else
		show_message("%s\n%s", _("Error! Couldn't upload save:"), save->dir_name);
}

static void exportVM2raw(const char* vm2_file, int dst, int ecc)
{
	int ret;
	char dstfile[256];
	char dst_path[256];

	_set_dest_path(dst_path, dst, VMC_PS2_PATH_USB);
	if (mkdirs(dst_path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), dst_path);
		return;
	}

	snprintf(dstfile, sizeof(dstfile), "%s%s.%s", dst_path, vm2_file, ecc ? "VM2" : "VMC");

	init_loading_screen(_("Exporting PS2 memory card..."));
	ret = mcio_vmcExportImage(dstfile, ecc);
	stop_loading_screen();

	if (ret == sceMcResSucceed)
		show_message("%s\n%s", _("File successfully saved to:"), dstfile);
	else
		show_message(_("Error! Failed to export PS2 memory card"));
}

static void rebuildAppDB(const char* path)
{
	int error = 0;
	char name[ORBIS_USER_SERVICE_MAX_USER_NAME_LENGTH+1];
	OrbisUserServiceRegisteredUserIdList userIdList;

	memset(&userIdList, 0, sizeof(userIdList));
	if (sceUserServiceGetRegisteredUserIdList(&userIdList) < 0)
	{
		show_message(_("Error getting PS4 user list!"));
		return;
	}

	for (int i = 0; i < ORBIS_USER_SERVICE_MAX_REGISTER_USERS; i++)
		if (userIdList.userId[i] != ORBIS_USER_SERVICE_USER_ID_INVALID && !appdb_rebuild(path, userIdList.userId[i]))
		{
			memset(name, 0, sizeof(name));
			sceUserServiceGetUserName(userIdList.userId[i], name, sizeof(name));
			show_message(_("Database rebuild for user %s failed!"), name);
			error++;
		}

	if(!error)
		show_message("%s\n%s", _("Database rebuilt successfully!"), _("Log out for changes to take effect"));
}

static void* orbis_host_callback(int id, int* size)
{
	OrbisNetCtlInfo info;

	memset(host_buf, 0, sizeof(host_buf));

	switch (id)
	{
	case APOLLO_HOST_TEMP_PATH:
		if (size) *size = strlen(APOLLO_LOCAL_CACHE);
		return APOLLO_LOCAL_CACHE;

	case APOLLO_HOST_DATA_PATH:
		if (size) *size = strlen(APOLLO_DATA_PATH);
		return APOLLO_DATA_PATH;

	case APOLLO_HOST_SYS_NAME:
		if (sceSystemServiceParamGetString(ORBIS_SYSTEM_SERVICE_PARAM_ID_SYSTEM_NAME, host_buf, sizeof(host_buf)) < 0)
			LOG("Error getting System name");

		if (size) *size = strlen(host_buf);
		return host_buf;

	case APOLLO_HOST_PSID:
		memcpy(host_buf, apollo_config.psid, 16);
		if (size) *size = 16;
		return host_buf;

	case APOLLO_HOST_ACCOUNT_ID:
		memcpy(host_buf, &apollo_config.account_id, 8);
		*(uint64_t*)host_buf = ES64(*(uint64_t*)host_buf);
		if (size) *size = 8;
		return host_buf;

	case APOLLO_HOST_USERNAME:
		if (sceUserServiceGetUserName(apollo_config.user_id, host_buf, sizeof(host_buf)) < 0)
			LOG("Error getting Username");

		if (size) *size = strlen(host_buf);
		return host_buf;

	case APOLLO_HOST_LAN_ADDR:
	case APOLLO_HOST_WLAN_ADDR:
		memset(&info, 0, sizeof(OrbisNetCtlInfo));
		if (sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_ETHER_ADDR, &info) < 0)
			LOG("Error getting Wlan Ethernet Address");
		else
			memcpy(host_buf, info.ether_addr, 6);

		if (size) *size = 6;
		return host_buf;
	}

	if (size) *size = 1;
	return host_buf;
}

static int apply_sfo_patches(save_entry_t* entry, sfo_patch_t* patch)
{
    option_value_t* optval;
    code_entry_t* code;
    char in_file_path[256];
    char tmp_dir[SFO_DIRECTORY_SIZE];
    u8 tmp_psid[SFO_PSID_SIZE];
    list_node_t* node;

    for (node = list_head(entry->codes); (code = list_get(node)); node = list_next(node))
    {
        if (!code->activated || code->type != PATCH_SFO)
            continue;

        LOG("Active: [%s]", code->name);

        switch (code->codes[0])
        {
        case SFO_CHANGE_ACCOUNT_ID:
            if (entry->flags & SAVE_FLAG_OWNER)
                entry->flags ^= SAVE_FLAG_OWNER;

            optval = list_get_item(code->options[0].opts, code->options[0].sel);
            sscanf(optval->value, "%" PRIx64, &patch->account_id);
            break;

        case SFO_REMOVE_PSID:
            memset(tmp_psid, 0, SFO_PSID_SIZE);
            patch->psid = tmp_psid;
            break;

        case SFO_CHANGE_TITLE_ID:
            patch->directory = strstr(entry->path, entry->title_id);
            snprintf(in_file_path, sizeof(in_file_path), "%s", entry->path);
            strncpy(tmp_dir, patch->directory, SFO_DIRECTORY_SIZE);

            optval = list_get_item(code->options[0].opts, code->options[0].sel);
            strncpy(entry->title_id, optval->name, 9);
            strncpy(patch->directory, entry->title_id, 9);
            strncpy(tmp_dir, entry->title_id, 9);
            *strrchr(tmp_dir, '/') = 0;
            patch->directory = tmp_dir;

            LOG("Moving (%s) -> (%s)", in_file_path, entry->path);
            rename(in_file_path, entry->path);
            break;

        default:
            break;
        }

        code->activated = 0;
    }

	snprintf(in_file_path, sizeof(in_file_path), "%s" "sce_sys/param.sfo", entry->path);
	LOG("Applying SFO patches '%s'...", in_file_path);

	return (patch_sfo(in_file_path, patch) == SUCCESS);
}

static int apply_cheat_patches(const save_entry_t* entry)
{
	int ret = 1;
	char tmpfile[256];
	char* filename;
	code_entry_t* code;
	list_node_t* node;

	init_loading_screen(_("Applying changes..."));

	for (node = list_head(entry->codes); (code = list_get(node)); node = list_next(node))
	{
		if (!code->activated || (code->type != PATCH_GAMEGENIE && code->type != PATCH_BSD && code->type != PATCH_PYTHON))
			continue;

		LOG("Active code: [%s]", code->name);

		if (strrchr(code->file, '\\'))
			filename = strrchr(code->file, '\\')+1;
		else
			filename = code->file;

		if (strchr(filename, '*'))
		{
			option_value_t* optval = list_get_item(code->options[0].opts, code->options[0].sel);
			filename = optval->name;
		}

		if (strncmp(code->file, "~extracted\\", 11) == 0)
			snprintf(tmpfile, sizeof(tmpfile), "%s", code->file);
		else
			snprintf(tmpfile, sizeof(tmpfile), "%s%s", entry->path, filename);

		if (!apply_cheat_patch_code(tmpfile, code, &orbis_host_callback))
		{
			LOG("Error: failed to apply (%s)", code->name);
			ret = 0;
		}

		code->activated = 0;
	}

	free_patch_var_list();
	stop_loading_screen();

	return ret;
}

static void resignSave(save_entry_t* entry)
{
	sfo_patch_t patch = {
		.flags = 0,
		.user_id = apollo_config.user_id,
		.psid = (u8*) apollo_config.psid,
		.directory = NULL,
		.account_id = apollo_config.account_id,
	};

    LOG("Resigning save '%s'...", entry->name);

    if (!apply_sfo_patches(entry, &patch))
        show_message(_("Error! Account changes couldn't be applied"));

    LOG("Applying cheats to '%s'...", entry->name);
    if (!apply_cheat_patches(entry))
        show_message(_("Error! Cheat codes couldn't be applied"));

    show_message(_("Save %s successfully modified!"), entry->title_id);
}

static void resignAllSaves(const save_entry_t* save, int all)
{
	char sfoPath[256];
	int err_count = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ((void**)save->dir_name)[0];
	sfo_patch_t patch = {
		.user_id = apollo_config.user_id,
		.psid = (u8*) apollo_config.psid,
		.account_id = apollo_config.account_id,
	};

	init_progress_bar(_("Resigning all saves..."));

	LOG("Resigning all saves from '%s'...", save->path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (item->type != FILE_TYPE_PS4 || (item->flags & SAVE_FLAG_LOCKED) || !(all || item->flags & SAVE_FLAG_SELECTED))
			continue;

		snprintf(sfoPath, sizeof(sfoPath), "%s" "sce_sys/param.sfo", item->path);
		if (file_exists(sfoPath) != SUCCESS)
			continue;

		LOG("Patching SFO '%s'...", sfoPath);
		err_count += (patch_sfo(sfoPath, &patch) != SUCCESS);
	}

	end_progress_bar();

	if (err_count)
		show_message(_("Error: %d Saves couldn't be resigned"), err_count);
	else
		show_message(_("All saves successfully resigned!"));
}

static void exportZipDB(const char* path)
{
	char *tmp;
	char zipfile[256];
	struct tm t;

	if (mkdirs(EXPORT_DB_PATH) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Export folder is not available:"), EXPORT_DB_PATH);
		return;
	}

	// build file path
	t  = *gmtime(&(time_t){time(NULL)});
	snprintf(zipfile, sizeof(zipfile), "%sdb_%d-%02d-%02d_%02d%02d%02d.zip", EXPORT_DB_PATH, t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	tmp = strdup(path);
	*strrchr(tmp, '/') = 0;

	init_loading_screen(_("Exporting system database..."));
	zip_directory(tmp, path, zipfile);
	stop_loading_screen();
	free(tmp);

	show_message("%s\n%s", _("Zip file successfully saved to:"), zipfile);
}

static void importZipDB(const char* dst_path, const char* zipfile)
{
	char path[256];

	snprintf(path, sizeof(path), "%s%s", EXPORT_DB_PATH, zipfile);
	LOG("Importing Backup %s ...", path);

	if (extract_zip(path, dst_path))
        show_message(_("DB Backup %s successfully restored!"), zipfile);
    else
        show_message(_("Error! Backup %s couldn't be restored"), zipfile);
}

static int _copy_save_file(const char* src_path, const char* dst_path, const char* filename)
{
	char src[256], dst[256];

	snprintf(src, sizeof(src), "%s%s", src_path, filename);
	snprintf(dst, sizeof(dst), "%s%s", dst_path, filename);

	return (copy_file(src, dst) == SUCCESS);
}

static void decryptSaveFile(const save_entry_t* entry, const char* filename)
{
	char path[256];

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s_%s/", apollo_config.user_id, entry->title_id, entry->dir_name);
	mkdirs(path);

	LOG("Decrypt '%s%s' to '%s'...", entry->path, filename, path);

	if (_copy_save_file(entry->path, path, filename))
		show_message("%s\n%s%s", _("File successfully exported to:"), path, filename);
	else
		show_message(_("Error! File %s couldn't be exported"), filename);
}

static void encryptSaveFile(const save_entry_t* entry, const char* filename)
{
	char path[256];

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s_%s/%s", apollo_config.user_id, entry->title_id, entry->dir_name, filename);
	if (file_exists(path) != SUCCESS)
	{
		show_message("%s\n%s", _("Error! Can't find decrypted save-game file:"), path);
		return;
	}

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s_%s/", apollo_config.user_id, entry->title_id, entry->dir_name);
	LOG("Encrypt '%s%s' to '%s'...", path, filename, entry->path);

	if (_copy_save_file(path, entry->path, filename))
		show_message("%s\n%s%s", _("File successfully imported to:"), entry->path, filename);
	else
		show_message(_("Error! File %s couldn't be imported"), filename);
}

static void downloadLink(const char* path)
{
	char url[256] = "http://";
	char out_path[256];

	if (!osk_dialog_get_text(_("Download URL"), url, sizeof(url)))
		return;

	char *fname = strrchr(url, '/');
	snprintf(out_path, sizeof(out_path), "%s%s", path, fname ? ++fname : "download.bin");

	if (http_download(url, NULL, out_path, 1))
		show_message("%s\n%s", _("File successfully downloaded to:"), out_path);
	else
		show_message(_("Error! File couldn't be downloaded"));
}

static void toggleBrowserHistory(int usr)
{
	char path[256];

	snprintf(path, sizeof(path), "/user/home/%08x/webbrowser/endhistory.txt", usr);
	if (dir_exists(path) == SUCCESS)
	{
		if (show_dialog(DIALOG_TYPE_YESNO, _("Enable Browser history?")) && rmdir(path) == SUCCESS)
			show_message("%s\n%s", _("Browser history enabled"), path);

		return;
	}

	if (show_dialog(DIALOG_TYPE_YESNO, _("Disable Browser history?")))
	{
		unlink_secure(path);
		strcat(path, "/");
		if (mkdirs(path) == SUCCESS)
			show_message("%s\n%s", _("Browser history disabled"), path);
	}
}

void execCodeCommand(code_entry_t* code, const char* codecmd)
{
	char *tmp = NULL;
	char mount[ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE];

	if (selected_entry->flags & (SAVE_FLAG_HDD|SAVE_FLAG_LOCKED) &&
		codecmd[0] != CMD_DELETE_SAVE && codecmd[0] != CMD_COPY_PFS)
	{
		if (!orbis_SaveMount(selected_entry, selected_entry->flags & (SAVE_FLAG_TROPHY|SAVE_FLAG_LOCKED), mount))
		{
			LOG("Error Mounting Save! Check Save Mount Patches");
			return;
		}

		tmp = selected_entry->path;
		asprintf(&selected_entry->path, APOLLO_SANDBOX_PATH, mount);
	}

	switch (codecmd[0])
	{
		option_value_t* optval;

		case CMD_DECRYPT_FILE:
			optval = list_get_item(code->options[0].opts, code->options[0].sel);
			decryptSaveFile(selected_entry, optval->name);
			code->activated = 0;
			break;

		case CMD_DOWNLOAD_USB:
			downloadSave(selected_entry, code->file, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_DOWNLOAD_HDD:
			downloadSaveHDD(selected_entry, code->file);
			code->activated = 0;
			break;

		case CMD_EXPORT_ZIP_USB:
			zipSave(selected_entry, codecmd[1] ? EXPORT_PATH_USB1 : EXPORT_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_EXPORT_ZIP_HDD:
			zipSave(selected_entry, APOLLO_PATH "export/");
			code->activated = 0;
			break;

		case CMD_COPY_SAVE_USB:
			copySave(selected_entry, codecmd[1] ? SAVES_PATH_USB1 : SAVES_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_COPY_SAVE_HDD:
			copySaveHDD(selected_entry);
			code->activated = 0;
			break;

		case CMD_EXP_KEYSTONE:
			copyKeystone(selected_entry, 0);
			code->activated = 0;
			break;

		case CMD_IMP_KEYSTONE:
			copyKeystone(selected_entry, 1);
			code->activated = 0;
			break;

		case CMD_CREATE_ACT_DAT:
			activateAccount(codecmd[1]);
			code->activated = 0;
			break;

		case CMD_URL_DOWNLOAD:
			downloadLink(selected_entry->path);
			code->activated = 0;
			break;

		case CMD_NET_WEBSERVER:
			enableWebServer(dbg_simpleWebServerHandler, NULL, 8080);
			code->activated = 0;
			break;

		case CMD_UPDATE_TROPHY:
			toggleTrophy(selected_entry);
			code->activated = 0;
			break;

		case CMD_ZIP_TROPHY_USB:
			exportTrophiesZip(codecmd[1] ? EXPORT_PATH_USB1 : EXPORT_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_COPY_TROPHIES_USB:
		case CMD_COPY_ALL_TROPHIES_USB:
			copyAllSavesUSB(selected_entry, codecmd[1] ? TROPHY_PATH_USB1 : TROPHY_PATH_USB0, codecmd[0] == CMD_COPY_ALL_TROPHIES_USB);
			code->activated = 0;
			break;

		case CMD_EXP_TROPHY_USB:
			copySave(selected_entry, codecmd[1] ? TROPHY_PATH_USB1 : TROPHY_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_BROWSER_HISTORY:
			toggleBrowserHistory(apollo_config.user_id);
			code->activated = 0;
			break;

		case CMD_COPY_SAVES_USB:
		case CMD_COPY_ALL_SAVES_USB:
			copyAllSavesUSB(selected_entry, codecmd[1] ? SAVES_PATH_USB1 : SAVES_PATH_USB0, codecmd[0] == CMD_COPY_ALL_SAVES_USB);
			code->activated = 0;
			break;

		case CMD_EXP_FINGERPRINT:
			exportFingerprint(selected_entry, 0);
			code->activated = 0;
			break;

		case CMD_DUMP_FINGERPRINTS:
			dumpAllFingerprints(selected_entry);
			code->activated = 0;
			break;

		case CMD_RESIGN_SAVE:
			resignSave(selected_entry);
			code->activated = 0;
			break;

		case CMD_RESIGN_SAVES:
		case CMD_RESIGN_ALL_SAVES:
			resignAllSaves(selected_entry, codecmd[0] == CMD_RESIGN_ALL_SAVES);
			code->activated = 0;
			break;

		case CMD_COPY_SAVES_HDD:
		case CMD_COPY_ALL_SAVES_HDD:
			copyAllSavesHDD(selected_entry, codecmd[0] == CMD_COPY_ALL_SAVES_HDD);
			code->activated = 0;
			break;

		case CMD_SAVE_WEBSERVER:
			enableWebServer(webReqHandler, ((void**)selected_entry->dir_name)[0], 8080);
			code->activated = 0;
			break;

		case CMD_IMPORT_DATA_FILE:
			optval = list_get_item(code->options[0].opts, code->options[0].sel);
			encryptSaveFile(selected_entry, optval->name);
			code->activated = 0;
			break;

		case CMD_UPLOAD_SAVE:
			uploadSaveFTP(selected_entry);
			code->activated = 0;
			break;

		case CMD_COPY_PFS:
			copySavePFS(selected_entry);
			code->activated = 0;
			break;

		case CMD_EXTRACT_ARCHIVE:
			extractArchive(code->file);
			code->activated = 0;
			break;

		case CMD_EXP_DATABASE:
			exportZipDB(selected_entry->path);
			code->activated = 0;
			break;

		case CMD_IMP_DATABASE:
			optval = list_get_item(code->options[0].opts, code->options[0].sel);
			importZipDB(selected_entry->path, optval->name);
			code->activated = 0;
			break;

		case CMD_DB_REBUILD:
			rebuildAppDB(code->file);
			code->activated = 0;
			break;

		case CMD_DB_DLC_REBUILD:
			if (addcont_dlc_rebuild(code->file))
				show_message("%s\n%s", _("DLC database fixed successfully!"), _("Log out for changes to take effect"));
			else
				show_message(_("DLC Database rebuild failed!"));

			code->activated = 0;
			break;

		case CMD_EXP_SAVES_VMC:
		case CMD_EXP_ALL_SAVES_VMC:
			exportAllSavesVMC(selected_entry, codecmd[1], codecmd[0] == CMD_EXP_ALL_SAVES_VMC);
			code->activated = 0;
			break;

		case CMD_EXP_VMC2SAVE:
			export_vmc2save(selected_entry, code->options[0].id, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_IMP_VMC2SAVE:
			import_save2vmc(code->file, codecmd[1]);
			selected_entry->flags |= SAVE_FLAG_UPDATED;
			code->activated = 0;
			break;

		case CMD_DELETE_SAVE:
			if (deleteSave(selected_entry))
				selected_entry->flags |= SAVE_FLAG_UPDATED;
			else
				code->activated = 0;
			break;

		case CMD_EXP_PS2_VM2:
		case CMD_EXP_PS2_RAW:
			exportVM2raw(code->file, codecmd[1], codecmd[0] == CMD_EXP_PS2_VM2);
			code->activated = 0;
			break;

		case CMD_EXP_VMC1SAVE:
			exportVmcSave(selected_entry, code->options[0].id, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_IMP_VMC1SAVE:
			if (openSingleSave(code->file, (int*) host_buf))
				show_message("%s\n%s", _("Save successfully imported:"), code->file);
			else
				show_message("%s\n%s", _("Error! Couldn't import save:"), code->file);

			selected_entry->flags |= SAVE_FLAG_UPDATED;
			code->activated = 0;
			break;

		case CMD_EXP_PS1_VM1:
		case CMD_EXP_PS1_VMP:
			export_ps1vmc(code->file, codecmd[1], codecmd[0] == CMD_EXP_PS1_VMP);
			code->activated = 0;
			break;

		default:
			break;
	}

	if (tmp)
	{
		orbis_SaveUmount(mount);
		free(selected_entry->path);
		selected_entry->path = tmp;
	}

	return;
}
