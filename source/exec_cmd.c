#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <orbis/SaveData.h>

#include "saves.h"
#include "menu.h"
#include "common.h"
#include "util.h"
#include "sfo.h"


static void downloadSave(const char* file, const char* path)
{
	if (mkdirs(path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", path);
		return;
	}

	if (!http_download(selected_entry->path, file, APOLLO_LOCAL_CACHE "tmpsave.zip", 1))
	{
		show_message("Error downloading save game from:\n%s%s", selected_entry->path, file);
		return;
	}

	if (extract_zip(APOLLO_LOCAL_CACHE "tmpsave.zip", path))
		show_message("Save game successfully downloaded to:\n%s", path);
	else
		show_message("Error extracting save game!");

	unlink_secure(APOLLO_LOCAL_CACHE "tmpsave.zip");
}

static uint32_t get_filename_id(const char* dir)
{
	char path[128];
	uint32_t tid = 0;

	do
	{
		tid++;
		snprintf(path, sizeof(path), "%s%08d.zip", dir, tid);
	}
	while (file_exists(path) == SUCCESS);

	return tid;
}

static void zipSave(const char* exp_path)
{
	char* export_file;
	char* tmp;
	uint32_t fid;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	init_loading_screen("Exporting save game...");

	fid = get_filename_id(exp_path);
	asprintf(&export_file, "%s%08d.zip", exp_path, fid);

	tmp = strdup(selected_entry->path);
	*strrchr(tmp, '/') = 0;
	*strrchr(tmp, '/') = 0;

	zip_directory(tmp, selected_entry->path, export_file);

	sprintf(export_file, "%s%08x.txt", exp_path, apollo_config.user_id);
	FILE* f = fopen(export_file, "a");
	if (f)
	{
		fprintf(f, "%08d.zip=[%s] %s\n", fid, selected_entry->title_id, selected_entry->name);
		fclose(f);
	}

	sprintf(export_file, "%s%08x.xml", exp_path, apollo_config.user_id);
	save_xml_owner(export_file);

	free(export_file);
	free(tmp);

	stop_loading_screen();
	show_message("Zip file successfully saved to:\n%s%08d.zip", exp_path, fid);
}

static void copySave(const save_entry_t* save, const char* exp_path)
{
	char* copy_path;

	if (strncmp(save->path, exp_path, strlen(exp_path)) == 0)
	{
		show_message("Copy operation cancelled!\nSame source and destination.");
		return;
	}

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	init_loading_screen("Copying files...");

	asprintf(&copy_path, "%s%08x_%s_%s/", exp_path, apollo_config.user_id, save->title_id, save->dir_name);

	LOG("Copying <%s> to %s...", save->path, copy_path);
	copy_directory(save->path, save->path, copy_path);

	free(copy_path);

	stop_loading_screen();
	show_message("Files successfully copied to:\n%s", exp_path);
}

static int _update_save_details(const char* sys_path, const char* mount)
{
	char file_path[256];
	uint8_t* iconBuf;
	size_t iconSize;

	snprintf(file_path, sizeof(file_path), "%s" "param.sfo", sys_path);
	LOG("Update Save Details :: Reading %s...", file_path);

	sfo_context_t* sfo = sfo_alloc();
	if (sfo_read(sfo, file_path) == SUCCESS)
	{
		char* title = (char*) sfo_get_param_value(sfo, "MAINTITLE");
		char* subtitle = (char*) sfo_get_param_value(sfo, "SUBTITLE");
		char* detail = (char*) sfo_get_param_value(sfo, "DETAIL");

		orbis_UpdateSaveParams(mount, title, subtitle, detail);
	}
	sfo_free(sfo);

	snprintf(file_path, sizeof(file_path), "%s" "icon0.png", sys_path);
	if (read_buffer(file_path, &iconBuf, &iconSize) == SUCCESS)
	{
		OrbisSaveDataMountPoint mp;
		OrbisSaveDataIcon icon;

		strlcpy(mp.data, mount, sizeof(mp.data));
		memset(&icon, 0x00, sizeof(icon));
		icon.buf = iconBuf;
		icon.bufSize = iconSize;
		icon.dataSize = iconSize;  // Icon data size

		if (sceSaveDataSaveIcon(&mp, &icon) < 0) {
			// Error handling
			LOG("ERROR sceSaveDataSaveIcon");
		}

		free(iconBuf);
	}

	return 1;
}

static int _copy_save_hdd(const save_entry_t* save)
{
	char copy_path[256];
	char mount[32];

	if (!orbis_SaveMount(save, ORBIS_SAVE_DATA_MOUNT_MODE_RDWR | ORBIS_SAVE_DATA_MOUNT_MODE_CREATE2 | ORBIS_SAVE_DATA_MOUNT_MODE_COPY_ICON, mount))
		return 0;

	snprintf(copy_path, sizeof(copy_path), APOLLO_SANDBOX_PATH, mount);

	LOG("Copying <%s> to %s...", save->path, copy_path);
	copy_directory(save->path, save->path, copy_path);

	snprintf(copy_path, sizeof(copy_path), "%s" "sce_sys/", save->path);
	_update_save_details(copy_path, mount);

	orbis_SaveUmount(mount);
	return 1;
}

static void copySaveHDD(const save_entry_t* save)
{
	//source save is already on HDD
	if (save->flags & SAVE_FLAG_HDD)
	{
		show_message("Copy operation cancelled!\nSame source and destination.");
		return;
	}

	init_loading_screen("Copying save game...");
	int ret = _copy_save_hdd(save);
	stop_loading_screen();

	if (ret)
		show_message("Files successfully copied to:\n%s/%s", save->title_id, save->dir_name);
	else
		show_message("Error! Can't copy Save-game folder:\n%s/%s", save->title_id, save->dir_name);
}

static void copyAllSavesHDD(const save_entry_t* save, int all)
{
	int err_count = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ((void**)save->dir_name)[0];

	init_progress_bar("Copying all saves...");

	LOG("Copying all saves from '%s' to HDD...", save->path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (item->type == FILE_TYPE_PS4 && !(item->flags & SAVE_FLAG_LOCKED) && (all || item->flags & SAVE_FLAG_SELECTED))
			err_count += ! _copy_save_hdd(item);
	}

	end_progress_bar();

	if (err_count)
		show_message("Error: %d Saves couldn't be copied to HDD", err_count);
	else
		show_message("All Saves copied to HDD");
}
/*
void exportLicensesZip(const char* exp_path)
{
	char* export_file;
	char* lic_path;
	char* tmp;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	init_loading_screen("Exporting user licenses...");

	asprintf(&export_file, "%s" "licenses_%08d.zip", exp_path, apollo_config.user_id);
	asprintf(&lic_path, EXDATA_PATH_HDD, apollo_config.user_id);

	tmp = strdup(lic_path);
	*strrchr(tmp, '/') = 0;

	zip_directory(tmp, lic_path, export_file);

	sprintf(export_file, "%s" OWNER_XML_FILE, exp_path);
	_saveOwnerData(export_file);

	sprintf(export_file, "%s" "idps.hex", exp_path);
	write_buffer(export_file, (u8*) apollo_config.idps, 16);

	free(export_file);
	free(lic_path);
	free(tmp);

	stop_loading_screen();
	show_message("Licenses successfully saved to:\n%slicenses_%08d.zip", exp_path, apollo_config.user_id);
}
*/
static void exportFingerprint(const save_entry_t* save, int silent)
{
	char fpath[256];
	uint8_t buffer[0x40];

	snprintf(fpath, sizeof(fpath), "%ssce_sys/keystone", save->path);
	LOG("Reading '%s' ...", fpath);

	if (read_file(fpath, buffer, sizeof(buffer)) != SUCCESS)
	{
		if (!silent) show_message("Error! Keystone file is not available:\n%s", fpath);
		return;
	}

	snprintf(fpath, sizeof(fpath), APOLLO_PATH "fingerprints.txt");
	FILE *fp = fopen(fpath, "a");
	if (!fp)
	{
		if (!silent) show_message("Error! Can't open file:\n%s", fpath);
		return;
	}

	fprintf(fp, "%s=", save->title_id);
	for (size_t i = 0x20; i < 0x40; i++)
		fprintf(fp, "%02x", buffer[i]);

	fprintf(fp, "\n");
	fclose(fp);

	if (!silent) show_message("%s fingerprint successfully saved to:\n%s", save->title_id, fpath);
}
/*
void exportTrophiesZip(const char* exp_path)
{
	char* export_file;
	char* trp_path;
	char* tmp;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	init_loading_screen("Exporting Trophies ...");

	asprintf(&export_file, "%s" "trophies_%08d.zip", exp_path, apollo_config.user_id);
	asprintf(&trp_path, TROPHY_PATH_HDD, apollo_config.user_id);

	tmp = strdup(trp_path);
	*strrchr(tmp, '/') = 0;

	zip_directory(tmp, trp_path, export_file);

	sprintf(export_file, "%s" OWNER_XML_FILE, exp_path);
	_saveOwnerData(export_file);

	free(export_file);
	free(trp_path);
	free(tmp);

	stop_loading_screen();
	show_message("Trophies successfully saved to:\n%strophies_%08d.zip", exp_path, apollo_config.user_id);
}

void resignPSVfile(const char* psv_path)
{
	init_loading_screen("Resigning PSV file...");
	psv_resign(psv_path);
	stop_loading_screen();

	show_message("File successfully resigned!");
}
*/

static void dumpAllFingerprints(const save_entry_t* save)
{
	char mount[32];
	uint64_t progress = 0;
	list_node_t *node;
	save_entry_t *item;
	list_t *list = ((void**)save->dir_name)[0];

	init_progress_bar("Dumping all fingerprints...");

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
	show_message("All fingerprints dumped to:\n%sfingerprints.txt", APOLLO_PATH);
}

static void activateAccount(int user, const char* value)
{
	uint64_t account = 0;

	sscanf(value, "%lx", &account);
	if (!account)
		account = (0x6F6C6C6F70610000 + ((user & 0xFFFF) ^ 0xFFFF));

	LOG("Activating user=%d (%lx)...", user, account);
	if (regMgr_SetAccountId(user, &account) != SUCCESS)
	{
		show_message("Error! Couldn't activate user account");
		return;
	}

	show_message("Account successfully activated!\nA system reboot might be required");
}

static void copySavePFS(const save_entry_t* save)
{
	char src_path[256];
	char hdd_path[256];
	char mount[32];
	sfo_patch_t patch = {
		.user_id = apollo_config.user_id,
		.account_id = apollo_config.account_id,
	};

	if (!orbis_SaveMount(save, ORBIS_SAVE_DATA_MOUNT_MODE_RDWR | ORBIS_SAVE_DATA_MOUNT_MODE_CREATE2 | ORBIS_SAVE_DATA_MOUNT_MODE_COPY_ICON, mount))
	{
		LOG("[!] Error: can't create/mount save!");
		return;
	}
	orbis_SaveUmount(mount);

	snprintf(src_path, sizeof(src_path), "%s%s", save->path, save->dir_name);
	snprintf(hdd_path, sizeof(hdd_path), "/user/home/%08x/savedata/%s/sdimg_%s", apollo_config.user_id, save->title_id, save->dir_name);
	LOG("Copying <%s> to %s...", src_path, hdd_path);
	if (copy_file(src_path, hdd_path) != SUCCESS)
	{
		LOG("[!] Error: can't copy %s", hdd_path);
		return;
	}

	snprintf(src_path, sizeof(src_path), "%s%s.bin", save->path, save->dir_name);
	snprintf(hdd_path, sizeof(hdd_path), "/user/home/%08x/savedata/%s/%s.bin", apollo_config.user_id, save->title_id, save->dir_name);
	LOG("Copying <%s> to %s...", src_path, hdd_path);
	if (copy_file(src_path, hdd_path) != SUCCESS)
	{
		LOG("[!] Error: can't copy %s", hdd_path);
		return;
	}

	if (!orbis_SaveMount(save, ORBIS_SAVE_DATA_MOUNT_MODE_RDWR, mount))
	{
		LOG("[!] Error: can't remount save");
		show_message("Error! Can't mount encrypted save.\n(incompatible save-game firmware version)");
		return;
	}

	snprintf(hdd_path, sizeof(hdd_path), APOLLO_SANDBOX_PATH "sce_sys/param.sfo", mount);
	if (show_dialog(1, "Resign save %s/%s?", save->title_id, save->dir_name))
		patch_sfo(hdd_path, &patch);

	*strrchr(hdd_path, 'p') = 0;
	_update_save_details(hdd_path, mount);
	orbis_SaveUmount(mount);

	show_message("Encrypted save copied successfully!\n%s/%s", save->title_id, save->dir_name);
	return;
}

static void copyKeystone(int import)
{
	char path_data[256];
	char path_save[256];

	snprintf(path_save, sizeof(path_save), "%ssce_sys/keystone", selected_entry->path);
	snprintf(path_data, sizeof(path_data), APOLLO_USER_PATH "%s/keystone", apollo_config.user_id, selected_entry->title_id);
	mkdirs(path_data);

	LOG("Copy '%s' <-> '%s'...", path_save, path_data);

	if (copy_file(import ? path_data : path_save, import ? path_save : path_data) == SUCCESS)
		show_message("Keystone successfully copied to:\n%s", import ? path_save : path_data);
	else
		show_message("Error! Keystone couldn't be copied");
}

/*
void exportPSVfile(const char* in_file, const char* out_path)
{
	if (mkdirs(out_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", out_path);
		return;
	}

	init_loading_screen("Exporting PSV file...");

	if (selected_entry->flags & SAVE_FLAG_PS1)
		ps1_psv2mcs(in_file, out_path);
	else
		ps2_psv2psu(in_file, out_path);

	stop_loading_screen();
	show_message("File successfully saved to:\n%s", out_path);
}

void convertSavePSV(const char* save_path, const char* out_path, uint16_t type)
{
	if (mkdirs(out_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", out_path);
		return;
	}

	init_loading_screen("Converting Save to PSV file...");

	switch (type)
	{
	case FILE_TYPE_MCS:
		ps1_mcs2psv(save_path, out_path);
		break;

	case FILE_TYPE_PSX:
		ps1_psx2psv(save_path, out_path);
		break;

	case FILE_TYPE_MAX:
		ps2_max2psv(save_path, out_path);
		break;

	case FILE_TYPE_PSU:
		ps2_psu2psv(save_path, out_path);
		break;

	case FILE_TYPE_CBS:
		ps2_cbs2psv(save_path, out_path);
		break;

	case FILE_TYPE_XPS:
		ps2_xps2psv(save_path, out_path);
		break;

	default:
		break;
	}

	stop_loading_screen();
	show_message("File successfully saved to:\n%s", out_path);
}
*/
static void copyAllSavesUSB(const save_entry_t* save, const char* dst_path, int all)
{
	char copy_path[256];
	char save_path[256];
	char mount[32];
	uint64_t progress = 0;
	list_node_t *node;
	save_entry_t *item;
	list_t *list = ((void**)save->dir_name)[0];

	if (!list || mkdirs(dst_path) != SUCCESS)
	{
		show_message("Error! Folder is not available:\n%s", dst_path);
		return;
	}

	init_progress_bar("Copying all saves...");

	LOG("Copying all saves to '%s'...", dst_path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (item->type != FILE_TYPE_PS4 || !(all || item->flags & SAVE_FLAG_SELECTED) || !orbis_SaveMount(item, ORBIS_SAVE_DATA_MOUNT_MODE_RDONLY, mount))
			continue;

		snprintf(save_path, sizeof(save_path), APOLLO_SANDBOX_PATH, mount);
		snprintf(copy_path, sizeof(copy_path), "%s%08x_%s_%s/", dst_path, apollo_config.user_id, item->title_id, item->dir_name);

		LOG("Copying <%s> to %s...", save_path, copy_path);
		copy_directory(save_path, save_path, copy_path);

		orbis_SaveUmount(mount);
	}

	end_progress_bar();
	show_message("All Saves copied to:\n%s", dst_path);
}

void exportFolder(const char* src_path, const char* exp_path, const char* msg)
{
	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	init_loading_screen(msg);

    LOG("Copying <%s> to %s...", src_path, exp_path);
	copy_directory(src_path, src_path, exp_path);

	stop_loading_screen();
	show_message("Files successfully copied to:\n%s", exp_path);
}
/*
void exportLicensesRap(const char* fname, uint8_t dest)
{
	DIR *d;
	struct dirent *dir;
	char lic_path[256];
	char exp_path[256];
	char msg[128] = "Exporting user licenses...";

	if (dest <= MAX_USB_DEVICES)
		snprintf(exp_path, sizeof(exp_path), EXPORT_RAP_PATH_USB, dest);
	else
		snprintf(exp_path, sizeof(exp_path), EXPORT_RAP_PATH_HDD);

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	snprintf(lic_path, sizeof(lic_path), EXDATA_PATH_HDD, apollo_config.user_id);
	d = opendir(lic_path);
	if (!d)
		return;

    init_loading_screen(msg);

	LOG("Exporting RAPs from folder '%s'...", lic_path);
	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 &&
			(!fname || (strcmp(dir->d_name, fname) == 0)) &&
			strcmp(strrchr(dir->d_name, '.'), ".rif") == 0)
		{
			LOG("Exporting %s", dir->d_name);
			snprintf(msg, sizeof(msg), "Exporting %.36s...", dir->d_name);
//			rif2rap((u8*) apollo_config.idps, lic_path, dir->d_name, exp_path);
		}
	}
	closedir(d);

    stop_loading_screen();
	show_message("Files successfully copied to:\n%s", exp_path);
}

void importLicenses(const char* fname, const char* exdata_path)
{
	DIR *d;
	struct dirent *dir;
	char lic_path[256];

	if (dir_exists(exdata_path) != SUCCESS)
	{
		show_message("Error! Import folder is not available:\n%s", exdata_path);
		return;
	}

	snprintf(lic_path, sizeof(lic_path), EXDATA_PATH_HDD, apollo_config.user_id);
	d = opendir(exdata_path);
	if (!d)
		return;

    init_loading_screen("Importing user licenses...");

	LOG("Importing RAPs from folder '%s'...", exdata_path);
	while ((dir = readdir(d)) != NULL)
	{
		if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0 &&
			(!fname || (strcmp(dir->d_name, fname)) == 0) &&
			strcasecmp(strrchr(dir->d_name, '.'), ".rap") == 0)
		{
			LOG("Importing %s", dir->d_name);
//			rap2rif((u8*) apollo_config.idps, exdata_path, dir->d_name, lic_path);
		}
	}
	closedir(d);

    stop_loading_screen();
	show_message("Files successfully copied to:\n%s", lic_path);
}
*/
static int apply_sfo_patches(sfo_patch_t* patch)
{
    code_entry_t* code;
    char in_file_path[256];
    char tmp_dir[SFO_DIRECTORY_SIZE];
    u8 tmp_psid[SFO_PSID_SIZE];
    list_node_t* node;

    for (node = list_head(selected_entry->codes); (code = list_get(node)); node = list_next(node))
    {
        if (!code->activated || code->type != PATCH_SFO)
            continue;

        LOG("Active: [%s]", code->name);

        switch (code->codes[0])
        {
        case SFO_UNLOCK_COPY:
            if (selected_entry->flags & SAVE_FLAG_LOCKED)
                selected_entry->flags ^= SAVE_FLAG_LOCKED;

            patch->flags = SFO_PATCH_FLAG_REMOVE_COPY_PROTECTION;
            break;

        case SFO_CHANGE_ACCOUNT_ID:
            if (selected_entry->flags & SAVE_FLAG_OWNER)
                selected_entry->flags ^= SAVE_FLAG_OWNER;

            sscanf(code->options->value[code->options->sel], "%lx", &patch->account_id);
            break;

        case SFO_REMOVE_PSID:
            bzero(tmp_psid, SFO_PSID_SIZE);
            patch->psid = tmp_psid;
            break;

        case SFO_CHANGE_TITLE_ID:
            patch->directory = strstr(selected_entry->path, selected_entry->title_id);
            snprintf(in_file_path, sizeof(in_file_path), "%s", selected_entry->path);
            strncpy(tmp_dir, patch->directory, SFO_DIRECTORY_SIZE);

            strncpy(selected_entry->title_id, code->options[0].name[code->options[0].sel], 9);
            strncpy(patch->directory, selected_entry->title_id, 9);
            strncpy(tmp_dir, selected_entry->title_id, 9);
            *strrchr(tmp_dir, '/') = 0;
            patch->directory = tmp_dir;

            LOG("Moving (%s) -> (%s)", in_file_path, selected_entry->path);
            rename(in_file_path, selected_entry->path);
            break;

        default:
            break;
        }

        code->activated = 0;
    }

	snprintf(in_file_path, sizeof(in_file_path), "%s" "sce_sys/param.sfo", selected_entry->path);
	LOG("Applying SFO patches '%s'...", in_file_path);

	return (patch_sfo(in_file_path, patch) == SUCCESS);
}

static int apply_cheat_patches()
{
	int ret = 1;
	char tmpfile[256];
	char* filename;
	code_entry_t* code;
	list_node_t* node;

	init_loading_screen("Applying changes...");

	for (node = list_head(selected_entry->codes); (code = list_get(node)); node = list_next(node))
	{
		if (!code->activated || (code->type != PATCH_GAMEGENIE && code->type != PATCH_BSD))
			continue;

    	LOG("Active code: [%s]", code->name);

		if (strrchr(code->file, '\\'))
			filename = strrchr(code->file, '\\')+1;
		else
			filename = code->file;

		if (strchr(filename, '*'))
			filename = code->options[0].name[code->options[0].sel];

		if (strstr(code->file, "~extracted\\"))
			snprintf(tmpfile, sizeof(tmpfile), "%s[%s]%s", APOLLO_LOCAL_CACHE, selected_entry->title_id, filename);
		else
			snprintf(tmpfile, sizeof(tmpfile), "%s%s", selected_entry->path, filename);

		if (!apply_cheat_patch_code(tmpfile, selected_entry->title_id, code, APOLLO_LOCAL_CACHE))
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

static void resignSave(sfo_patch_t* patch)
{
    LOG("Resigning save '%s'...", selected_entry->name);

    if (!apply_sfo_patches(patch))
        show_message("Error! Account changes couldn't be applied");

    LOG("Applying cheats to '%s'...", selected_entry->name);
    if (!apply_cheat_patches())
        show_message("Error! Cheat codes couldn't be applied");

    show_message("Save %s successfully modified!", selected_entry->title_id);
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

	init_progress_bar("Resigning all saves...");

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
		err_count += patch_sfo(sfoPath, &patch);
	}

	end_progress_bar();

	if (err_count)
		show_message("Error: %d Saves couldn't be resigned", err_count);
	else
		show_message("All saves successfully resigned!");
}
/*
int apply_trophy_account()
{
	char sfoPath[256];
	char account_id[SFO_ACCOUNT_ID_SIZE+1];

	snprintf(account_id, sizeof(account_id), "%*lx", SFO_ACCOUNT_ID_SIZE, apollo_config.account_id);
	if (!apollo_config.account_id)
		memset(account_id, 0, SFO_ACCOUNT_ID_SIZE);

	snprintf(sfoPath, sizeof(sfoPath), "%s" "PARAM.SFO", selected_entry->path);

	patch_sfo_trophy(sfoPath, account_id);
//	patch_trophy_account(selected_entry->path, account_id);

	return 1;
}

int apply_trophy_patches()
{
	int ret = 1;
	uint32_t trophy_id;
	code_entry_t* code;
	list_node_t* node;

	init_loading_screen("Applying changes...");

	for (node = list_head(selected_entry->codes); (code = list_get(node)); node = list_next(node))
	{
		if (!code->activated || (code->type != PATCH_TROP_UNLOCK && code->type != PATCH_TROP_LOCK))
			continue;

		trophy_id = *(uint32_t*)(code->file);
    	LOG("Active code: [%d] '%s'", trophy_id, code->name);

if (0)
//		if (!apply_trophy_patch(selected_entry->path, trophy_id, (code->type == PATCH_TROP_UNLOCK)))
		{
			LOG("Error: failed to apply (%s)", code->name);
			ret = 0;
		}

		if (code->type == PATCH_TROP_UNLOCK)
		{
			code->type = PATCH_TROP_LOCK;
			code->name[1] = ' ';
		}
		else
		{
			code->type = PATCH_TROP_UNLOCK;
			code->name[1] = CHAR_TAG_LOCKED;
		}

		code->activated = 0;
	}

	stop_loading_screen();

	return ret;
}

void resignTrophy()
{
	LOG("Decrypting TROPTRNS.DAT ...");
if (0)
//	if (!decrypt_trophy_trns(selected_entry->path))
	{
		LOG("Error: failed to decrypt TROPTRNS.DAT");
		return;
	}

    if (!apply_trophy_account())
        show_message("Error! Account changes couldn't be applied");

    LOG("Applying trophy changes to '%s'...", selected_entry->name);
    if (!apply_trophy_patches())
        show_message("Error! Trophy changes couldn't be applied");

	LOG("Encrypting TROPTRNS.DAT ...");
if (0)
//	if (!encrypt_trophy_trns(selected_entry->path))
	{
		LOG("Error: failed to encrypt TROPTRNS.DAT");
		return;
	}

    LOG("Resigning trophy '%s'...", selected_entry->name);

if (0)
//    if (!pfd_util_init((u8*) apollo_config.idps, apollo_config.user_id, selected_entry->title_id, selected_entry->path) ||
//        (pfd_util_process(PFD_CMD_UPDATE, 0) != SUCCESS))
        show_message("Error! Trophy %s couldn't be resigned", selected_entry->title_id);
    else
        show_message("Trophy %s successfully modified!", selected_entry->title_id);

//    pfd_util_end();

	if ((file_exists("/dev_hdd0/mms/db.err") != SUCCESS) && show_dialog(1, "Schedule Database rebuild on next boot?"))
	{
		LOG("Creating db.err file for database rebuild...");
		write_buffer("/dev_hdd0/mms/db.err", (u8*) "\x00\x00\x03\xE9", 4);
	}
}
*/
static int _copy_save_file(const char* src_path, const char* dst_path, const char* filename)
{
	char src[256], dst[256];

	snprintf(src, sizeof(src), "%s%s", src_path, filename);
	snprintf(dst, sizeof(dst), "%s%s", dst_path, filename);

	return (copy_file(src, dst) == SUCCESS);
}

static void decryptSaveFile(const char* filename)
{
	char path[256];

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s_%s/", apollo_config.user_id, selected_entry->title_id, selected_entry->dir_name);
	mkdirs(path);

	LOG("Decrypt '%s%s' to '%s'...", selected_entry->path, filename, path);

	if (_copy_save_file(selected_entry->path, path, filename))
		show_message("File successfully exported to:\n%s%s", path, filename);
	else
		show_message("Error! File %s couldn't be exported", filename);
}

static void encryptSaveFile(const char* filename)
{
	char path[256];

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s_%s/%s", apollo_config.user_id, selected_entry->title_id, selected_entry->dir_name, filename);

	if (file_exists(path) != SUCCESS)
	{
		show_message("Error! Can't find decrypted save-game file:\n%s", path);
		return;
	}
	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s_%s/", apollo_config.user_id, selected_entry->title_id, selected_entry->dir_name);

	LOG("Encrypt '%s%s' to '%s'...", path, filename, selected_entry->path);

	if (_copy_save_file(path, selected_entry->path, filename))
		show_message("File successfully imported to:\n%s%s", selected_entry->path, filename);
	else
		show_message("Error! File %s couldn't be imported", filename);
}

void execCodeCommand(code_entry_t* code, const char* codecmd)
{
	char *tmp, mount[32];

	if (selected_entry->flags & SAVE_FLAG_HDD)
	{
		if (!orbis_SaveMount(selected_entry, ORBIS_SAVE_DATA_MOUNT_MODE_RDWR, mount))
		{
			LOG("Error Mounting Save! Check Save Mount Patches");
			return;
		}

		tmp = selected_entry->path;
		asprintf(&selected_entry->path, APOLLO_SANDBOX_PATH, mount);
	}

	switch (codecmd[0])
	{
		case CMD_DECRYPT_FILE:
			decryptSaveFile(code->options[0].name[code->options[0].sel]);
			code->activated = 0;
			break;

		case CMD_DOWNLOAD_USB:
			if (selected_entry->flags & SAVE_FLAG_PS4)
				downloadSave(code->file, codecmd[1] ? SAVES_PATH_USB1 : SAVES_PATH_USB0);
			else
				downloadSave(code->file, codecmd[1] ? EXP_PSV_PATH_USB1 : EXP_PSV_PATH_USB0);
			
			code->activated = 0;
			break;

		case CMD_EXPORT_ZIP_USB:
			zipSave(codecmd[1] ? EXPORT_PATH_USB1 : EXPORT_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_EXPORT_ZIP_HDD:
			zipSave(APOLLO_PATH);
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
			copyKeystone(0);
			code->activated = 0;
			break;

		case CMD_IMP_KEYSTONE:
			copyKeystone(1);
			code->activated = 0;
			break;

		case CMD_CREATE_ACT_DAT:
			activateAccount(code->file[0], code->options->value[code->options->sel] + 1);
			code->activated = 0;
			break;
/*
		case CMD_EXP_TROPHY_USB:
			copySave(selected_entry, codecmd[1] ? TROPHY_PATH_USB1 : TROPHY_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_ZIP_TROPHY_USB:
			exportTrophiesZip(codecmd[1] ? EXPORT_PATH_USB1 : EXPORT_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_COPY_TROPHIES_USB:
			exportFolder(selected_entry->path, codecmd[1] ? TROPHY_PATH_USB1 : TROPHY_PATH_USB0, "Copying trophies...");
			code->activated = 0;
			break;
*/
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
			{
				sfo_patch_t patch = {
					.flags = 0,
					.user_id = apollo_config.user_id,
					.psid = (u8*) apollo_config.psid,
					.directory = NULL,
					.account_id = apollo_config.account_id,
				};

				resignSave(&patch);
			}
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

		case CMD_RUN_WEBSERVER:
			enableWebServer(selected_entry, 8080);
			code->activated = 0;
			break;

		case CMD_IMPORT_DATA_FILE:
			encryptSaveFile(code->options[0].name[code->options[0].sel]);
			code->activated = 0;
			break;

		case CMD_COPY_PFS:
			copySavePFS(selected_entry);
			code->activated = 0;
			break;
/*
		case CMD_RESIGN_TROPHY:
			resignTrophy();
			code->activated = 0;
			break;
*/
		default:
			break;
	}

	if (selected_entry->flags & SAVE_FLAG_HDD)
	{
		orbis_SaveUmount(mount);
		free(selected_entry->path);
		selected_entry->path = tmp;
	}

	return;
}
