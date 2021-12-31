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


void downloadSave(const char* file, const char* path)
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

void _saveOwnerData(const char* path)
{
	/*
	char buff[SYSUTIL_SYSTEMPARAM_CURRENT_USERNAME_SIZE+1];

	sysUtilGetSystemParamString(SYSUTIL_SYSTEMPARAM_ID_CURRENT_USERNAME, buff, SYSUTIL_SYSTEMPARAM_CURRENT_USERNAME_SIZE);
	LOG("Saving User '%s'...", buff);
	save_xml_owner(path, buff);
	*/
}

uint32_t get_filename_id(const char* dir)
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

void zipSave(const char* exp_path)
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

	sprintf(export_file, "%s%08d.txt", exp_path, fid);
	FILE* f = fopen(export_file, "a");
	if (f)
	{
		fprintf(f, "%08d.zip=[%s] %s\n", fid, selected_entry->title_id, selected_entry->name);
		fclose(f);
	}

	sprintf(export_file, "%s" OWNER_XML_FILE, exp_path);
	_saveOwnerData(export_file);

	free(export_file);
	free(tmp);

	stop_loading_screen();
	show_message("Zip file successfully saved to:\n%s%08d.zip", exp_path, fid);
}

void copySave(const save_entry_t* save, const char* exp_path)
{
	char* copy_path;

	if (strncmp(save->path, exp_path, strlen(exp_path)) == 0)
		return;

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

static int _copy_save_hdd(const save_entry_t* save)
{
	char copy_path[256];
	char mount[32];
	uint8_t* iconBuf;
	size_t iconSize;

	if (!orbis_SaveMount(save, ORBIS_SAVE_DATA_MOUNT_MODE_RDWR | ORBIS_SAVE_DATA_MOUNT_MODE_CREATE2 | ORBIS_SAVE_DATA_MOUNT_MODE_COPY_ICON, mount))
		return 0;

	snprintf(copy_path, sizeof(copy_path), APOLLO_SANDBOX_PATH, mount);

	LOG("Copying <%s> to %s...", save->path, copy_path);
	copy_directory(save->path, save->path, copy_path);

	snprintf(copy_path, sizeof(copy_path), "%s" "sce_sys/param.sfo", save->path);
	LOG("Save Details :: Reading %s...", copy_path);

	sfo_context_t* sfo = sfo_alloc();
	if (sfo_read(sfo, copy_path) == SUCCESS)
	{
		char* subtitle = (char*) sfo_get_param_value(sfo, "SUBTITLE");
		char* detail = (char*) sfo_get_param_value(sfo, "DETAIL");

		orbis_UpdateSaveParams(mount, save->name, subtitle, detail);
	}
	sfo_free(sfo);

	snprintf(copy_path, sizeof(copy_path), "%s" "sce_sys/icon0.png", save->path);
	if (read_buffer(copy_path, &iconBuf, &iconSize) == SUCCESS)
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

	orbis_SaveUmount(mount);
	return 1;
}

void copySaveHDD(const save_entry_t* save)
{
	//source save is already on HDD
	if (save->flags & SAVE_FLAG_HDD)
		return;

	init_loading_screen("Copying save game...");
	int ret = _copy_save_hdd(save);
	stop_loading_screen();

	if (ret)
		show_message("Files successfully copied to:\n%s/%s", save->title_id, save->dir_name);
	else
		show_message("Error! Can't copy Save-game folder:\n%s/%s", save->title_id, save->dir_name);
}

void copyAllSavesHDD(const char* path)
{
	int err_count = 0;
	list_node_t *node;
	save_entry_t *item;
	uint64_t progress = 0;
	list_t *list = ReadUsbList(path);

	if (!list)
	{
		show_message("Error! Folder is not available:\n%s", path);
		return;
	}

	init_progress_bar("Copying all saves...");

	LOG("Copying all saves from '%s'...", path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (item->type == FILE_TYPE_PS4)
			err_count += ! _copy_save_hdd(item);
	}

	end_progress_bar();
	UnloadGameList(list);

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

void exportFlashZip(const char* exp_path)
{
	char* export_file;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	init_loading_screen("Exporting /dev_flash2.zip ...");

	asprintf(&export_file, "%s" "dev_flash2.zip", exp_path);
	zip_directory("/dev_flash2", "/dev_flash2/", export_file);

	sprintf(export_file, "%s" OWNER_XML_FILE, exp_path);
	_saveOwnerData(export_file);

	sprintf(export_file, "%s" "idps.hex", exp_path);
	write_buffer(export_file, (u8*) apollo_config.idps, 16);

	free(export_file);

	stop_loading_screen();
	show_message("Files successfully saved to:\n%sdev_flash2.zip", exp_path);
}

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
void activateAccount(int user)
{
	uint64_t account = (0x6F6C6C6F70610000 + ((user & 0xFFFF) ^ 0xFFFF));

	LOG("Activating user=%d (%lx)...", user, account);
	if (regMgr_SetAccountId(user, &account) != SUCCESS)
	{
		show_message("Error! Couldn't activate user account");
		return;
	}

	show_message("Account successfully activated!\nA system reboot might be required");
}
/*
void copyDummyPSV(const char* psv_file, const char* out_path)
{
	char *in, *out;

	if (mkdirs(out_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", out_path);
		return;
	}

	asprintf(&in, APOLLO_DATA_PATH "%s", psv_file);
	asprintf(&out, "%s%s", out_path, psv_file);

	init_loading_screen("Copying PSV file...");
	copy_file(in, out);
	stop_loading_screen();

	free(in);
	free(out);

	show_message("File successfully saved to:\n%s%s", out_path, psv_file);
}

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

void decryptVMEfile(const char* vme_path, const char* vme_file, uint8_t dst)
{
	char vmefile[256];
	char outfile[256];
	const char *path;

	switch (dst)
	{
	case 0:
		path = EXP_PS2_PATH_USB0;
		break;

	case 1:
		path = EXP_PS2_PATH_USB1;
		break;

	case 2:
		path = EXP_PS2_PATH_HDD;
		break;

	default:
		return;
	}

	if (mkdirs(path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", path);
		return;
	}

	snprintf(vmefile, sizeof(vmefile), "%s%s", vme_path, vme_file);
	snprintf(outfile, sizeof(outfile), "%sAPOLLO%c.VM2", path, vme_file[6]);

	init_loading_screen("Decrypting VME card...");
	ps2_crypt_vmc(0, vmefile, outfile, 0);
	stop_loading_screen();

	show_message("File successfully saved to:\n%s", outfile);
}

void encryptVM2file(const char* vme_path, const char* vme_file, const char* src_name)
{
	char vmefile[256];
	char srcfile[256];

	snprintf(vmefile, sizeof(vmefile), "%s%s", vme_path, vme_file);
	snprintf(srcfile, sizeof(srcfile), "%s%s", EXP_PS2_PATH_HDD, src_name);

	init_loading_screen("Encrypting VM2 card...");
	ps2_crypt_vmc(0, srcfile, vmefile, 1);
	stop_loading_screen();

	show_message("File successfully saved to:\n%s", vmefile);
}

void importPS2VMC(const char* vmc_path, const char* vmc_file)
{
	char vm2file[256];
	char srcfile[256];

	snprintf(srcfile, sizeof(srcfile), "%s%s", vmc_path, vmc_file);
	snprintf(vm2file, sizeof(vm2file), "%s%s", EXP_PS2_PATH_HDD, vmc_file);
	strcpy(strrchr(vm2file, '.'), ".VM2");

	init_loading_screen("Importing PS2 memory card...");
	ps2_add_vmc_ecc(srcfile, vm2file);
	stop_loading_screen();

	show_message("File successfully saved to:\n%s", vm2file);
}

void exportVM2raw(const char* vm2_path, const char* vm2_file, const char* dst_path)
{
	char vm2file[256];
	char dstfile[256]; 

	if (mkdirs(dst_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", dst_path);
		return;
	}

	snprintf(vm2file, sizeof(vm2file), "%s%s", vm2_path, vm2_file);
	snprintf(dstfile, sizeof(dstfile), "%s%s.vmc", dst_path, vm2_file);

	init_loading_screen("Exporting PS2 .VM2 memory card...");
	ps2_remove_vmc_ecc(vm2file, dstfile);
	stop_loading_screen();

	show_message("File successfully saved to:\n%s%s", dstfile);
}

void importPS2classicsCfg(const char* cfg_path, const char* cfg_file)
{
	char ps2file[256];
	char outfile[256];

	snprintf(ps2file, sizeof(ps2file), "%s%s", cfg_path, cfg_file);
	snprintf(outfile, sizeof(outfile), PS2ISO_PATH_HDD "%s", cfg_file);
	*strrchr(outfile, '.') = 0;
	strcat(outfile, ".ENC");

	init_loading_screen("Encrypting PS2 CONFIG...");
	ps2_encrypt_image(1, ps2file, outfile, NULL);
	stop_loading_screen();

	show_message("File successfully saved to:\n%s", outfile);
}

void importPS2classics(const char* iso_path, const char* iso_file)
{
	char ps2file[256];
	char outfile[256];
	char msg[128] = "Encrypting PS2 ISO...";

	snprintf(ps2file, sizeof(ps2file), "%s%s", iso_path, iso_file);
	snprintf(outfile, sizeof(outfile), PS2ISO_PATH_HDD "%s", iso_file);
	*strrchr(outfile, '.') = 0;
	strcat(outfile, ".BIN.ENC");

	init_loading_screen(msg);
	ps2_encrypt_image(0, ps2file, outfile, msg);
	stop_loading_screen();

	show_message("File successfully saved to:\n%s", outfile);
}

void exportPS2classics(const char* enc_path, const char* enc_file, uint8_t dst)
{
	char path[256];
	char ps2file[256];
	char outfile[256];
	char msg[128] = "Decrypting PS2 BIN.ENC...";

	if (dst <= MAX_USB_DEVICES)
		snprintf(path, sizeof(path), PS2ISO_PATH_USB, dst);
	else
		snprintf(path, sizeof(path), PS2ISO_PATH_HDD);

	snprintf(ps2file, sizeof(ps2file), "%s%s", enc_path, enc_file);
	snprintf(outfile, sizeof(outfile), "%s%s", path, enc_file);
	*strrchr(outfile, '.') = 0;
	strcat(outfile, ".ps2.iso");

	if (mkdirs(outfile) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", outfile);
		return;
	}

	init_loading_screen(msg);
	ps2_decrypt_image(0, ps2file, outfile, msg);
	stop_loading_screen();

	show_message("File successfully saved to:\n%s", outfile);
}
*/
void copyAllSavesUSB(const char* path, const char* dst_path)
{
	char copy_path[256];
	char save_path[256];
	char mount[32];
	uint64_t progress = 0;
	list_node_t *node;
	save_entry_t *item;
	list_t *list = ReadUserList(path);

	if (!list || mkdirs(dst_path) != SUCCESS)
	{
		show_message("Error! Folder is not available:\n%s", dst_path);
		return;
	}

	init_progress_bar("Copying all saves...");

	LOG("Copying all saves from '%s'...", path);
	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		update_progress_bar(progress++, list_count(list), item->name);
		if (item->type == FILE_TYPE_PS4)
		{
			if (!orbis_SaveMount(item, ORBIS_SAVE_DATA_MOUNT_MODE_RDONLY, mount))
				continue;

			snprintf(save_path, sizeof(save_path), APOLLO_SANDBOX_PATH, mount);
			snprintf(copy_path, sizeof(copy_path), "%s%08x_%s_%s/", dst_path, apollo_config.user_id, item->title_id, item->dir_name);

			LOG("Copying <%s> to %s...", save_path, copy_path);
			copy_directory(save_path, save_path, copy_path);

			orbis_SaveUmount(mount);
		}
	}

	end_progress_bar();
	UnloadGameList(list);
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
int apply_sfo_patches(sfo_patch_t* patch)
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

int apply_cheat_patches()
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

void resignSave(sfo_patch_t* patch)
{
    LOG("Resigning save '%s'...", selected_entry->name);

    if (!apply_sfo_patches(patch))
        show_message("Error! Account changes couldn't be applied");

    LOG("Applying cheats to '%s'...", selected_entry->name);
    if (!apply_cheat_patches())
        show_message("Error! Cheat codes couldn't be applied");

    show_message("Save %s successfully modified!", selected_entry->title_id);
}

void resignAllSaves(const char* path)
{
	DIR *d;
	struct dirent *dir;
	char sfoPath[256];
	char titleid[10];
	char message[128] = "Resigning all saves...";

	if (dir_exists(path) != SUCCESS)
	{
		show_message("Error! Folder is not available:\n%s", path);
		return;
	}

	d = opendir(path);
	if (!d)
		return;

    init_loading_screen(message);

	sfo_patch_t patch = {
		.flags = SFO_PATCH_FLAG_REMOVE_COPY_PROTECTION,
		.user_id = apollo_config.user_id,
		.psid = (u8*) apollo_config.psid,
		.account_id = apollo_config.account_id,
		.directory = NULL,
	};

	LOG("Resigning saves from '%s'...", path);
	while ((dir = readdir(d)) != NULL)
	{
		if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
		{
			snprintf(sfoPath, sizeof(sfoPath), "%s%s/sce_sys/param.sfo", path, dir->d_name);
			if (file_exists(sfoPath) == SUCCESS)
			{
				LOG("Patching SFO '%s'...", sfoPath);
				if (patch_sfo(sfoPath, &patch) != SUCCESS)
					continue;

				snprintf(titleid, sizeof(titleid), "%.9s", dir->d_name);
				snprintf(sfoPath, sizeof(sfoPath), "%s%s/", path, dir->d_name);
				snprintf(message, sizeof(message), "Resigning %s...", dir->d_name);

				LOG("Resigning save '%s'...", sfoPath);
			}
		}
	}
	closedir(d);

	stop_loading_screen();
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
int _copy_save_file(const char* src_path, const char* dst_path, const char* filename)
{
	char src[256], dst[256];

	snprintf(src, sizeof(src), "%s%s", src_path, filename);
	snprintf(dst, sizeof(dst), "%s%s", dst_path, filename);

	return (copy_file(src, dst) == SUCCESS);
}

void decryptSaveFile(const char* filename)
{
	char path[256];

	snprintf(path, sizeof(path), APOLLO_TMP_PATH "%s/", selected_entry->title_id);
	mkdirs(path);

	LOG("Decrypt '%s%s' to '%s'...", selected_entry->path, filename, path);

	if (_copy_save_file(selected_entry->path, path, filename))
		show_message("File successfully decrypted to:\n%s%s", path, filename);
	else
		show_message("Error! File %s couldn't be decrypted", filename);
}

void encryptSaveFile(const char* filename)
{
	char path[256];

	snprintf(path, sizeof(path), APOLLO_TMP_PATH "%s/%s", selected_entry->title_id, filename);

	if (file_exists(path) != SUCCESS)
	{
		show_message("Error! Can't find decrypted save-game file:\n%s", path);
		return;
	}
	*(strrchr(path, '/')+1) = 0;

	LOG("Encrypt '%s%s' to '%s'...", path, filename, selected_entry->path);

	if (_copy_save_file(path, selected_entry->path, filename))
		show_message("File successfully encrypted to:\n%s%s", selected_entry->path, filename);
	else
		show_message("Error! File %s couldn't be encrypted", filename);
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
			zipSave(APOLLO_TMP_PATH);
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
/*
		case CMD_EXP_EXDATA_USB:
			exportLicensesZip(codecmd[1] ? EXPORT_PATH_USB1 : EXPORT_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_EXP_LICS_RAPS:
			exportLicensesRap(code->file, codecmd[1]);
			code->activated = 0;
			break;
*/
		case CMD_CREATE_ACT_DAT:
			activateAccount(code->file[0]);
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
			copyAllSavesUSB(selected_entry->path, codecmd[1] ? SAVES_PATH_USB1 : SAVES_PATH_USB0);
			code->activated = 0;
			break;
/*
		case CMD_EXP_FLASH2_USB:
			exportFlashZip(codecmd[1] ? EXPORT_PATH_USB1 : EXPORT_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_IMP_EXDATA_USB:
			importLicenses(code->file, selected_entry->path);
			code->activated = 0;
			break;
*/
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

		case CMD_RESIGN_ALL_SAVES:
			resignAllSaves(selected_entry->path);
			code->activated = 0;
			break;

		case CMD_COPY_SAVES_HDD:
			copyAllSavesHDD(selected_entry->path);
			code->activated = 0;
			break;
/*
		case CMD_RESIGN_PSV:
			resignPSVfile(selected_entry->path);
			code->activated = 0;
			break;

		case CMD_DECRYPT_PS2_VME:
			decryptVMEfile(selected_entry->path, code->file, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_ENCRYPT_PS2_VMC:
			encryptVM2file(selected_entry->path, code->file, code->options[0].name[code->options[0].sel]);
			code->activated = 0;
			break;

		case CMD_IMP_PS2_ISO:
			importPS2classics(selected_entry->path, code->file);
			code->activated = 0;
			break;

		case CMD_IMP_PS2_CONFIG:
			importPS2classicsCfg(selected_entry->path, code->file);
			code->activated = 0;
			break;

		case CMD_CONVERT_TO_PSV:
			convertSavePSV(selected_entry->path, codecmd[1] ? EXP_PSV_PATH_USB1 : EXP_PSV_PATH_USB0, selected_entry->type);
			code->activated = 0;
			break;

		case CMD_COPY_DUMMY_PSV:
			copyDummyPSV(code->file, codecmd[1] ? EXP_PSV_PATH_USB1 : EXP_PSV_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_EXP_PS2_BINENC:
			exportPS2classics(selected_entry->path, code->file, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_EXP_PSV_MCS:
			exportPSVfile(selected_entry->path, codecmd[1] ? USB1_PATH PS1_IMP_PATH_USB : USB0_PATH PS1_IMP_PATH_USB);
			code->activated = 0;
			break;

		case CMD_EXP_PSV_PSU:
			exportPSVfile(selected_entry->path, codecmd[1] ? USB1_PATH PS2_IMP_PATH_USB : USB0_PATH PS2_IMP_PATH_USB);
			code->activated = 0;
			break;

		case CMD_EXP_VM2_RAW:
			exportVM2raw(selected_entry->path, code->file, codecmd[1] ? EXP_PS2_PATH_USB1 : EXP_PS2_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_IMP_PS2VMC_USB:
			importPS2VMC(selected_entry->path, code->file);
			code->activated = 0;
			break;

		case CMD_IMPORT_DATA_FILE:
			encryptSaveFile(code->options[0].name[code->options[0].sel]);
			code->activated = 0;
			break;

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
