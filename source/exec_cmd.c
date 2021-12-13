#include <stdio.h>
#include <string.h>
#include <dirent.h>

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

void copySave(const char* save_path, const char* exp_path)
{
	char* copy_path;
	char* tmp;

	if (strncmp(save_path, exp_path, strlen(exp_path)) == 0)
		return;

	if (mkdirs(exp_path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", exp_path);
		return;
	}

	init_loading_screen("Copying files...");

	tmp = strdup(save_path);
	*strrchr(tmp, '/') = 0;
	asprintf(&copy_path, "%s%s/", exp_path, strrchr(tmp, '/')+1);

	LOG("Copying <%s> to %s...", save_path, copy_path);
	copy_directory(save_path, save_path, copy_path);

	free(copy_path);
	free(tmp);

	stop_loading_screen();
	show_message("Files successfully copied to:\n%s", exp_path);
}

void copySaveHDD(const char* save_path)
{
	char* copy_path;
	char* tmp = strdup(save_path);
	const char* folder;

	*strrchr(tmp, '/') = 0;
	folder = strrchr(tmp, '/')+1;
	asprintf(&copy_path, SAVES_PATH_HDD "%s/", apollo_config.user_id, folder);

	if (dir_exists(copy_path) == SUCCESS)
	{
		show_message("Error! Save-game folder already exists:\n%s", copy_path);
		free(copy_path);
		free(tmp);
		return;
	}

	init_loading_screen("Copying save game...");

if (0)
//	if (create_savegame_folder(folder))
	{
		LOG("Copying <%s> to %s...", save_path, copy_path);
		copy_directory(save_path, save_path, copy_path);
	}

	free(copy_path);
	free(tmp);

	stop_loading_screen();
}

void copyAllSavesHDD(const char* path)
{
	DIR *d;
	struct dirent *dir;
	char savePath[256];

	if (dir_exists(path) != SUCCESS)
	{
		show_message("Error! Folder is not available:\n%s", path);
		return;
	}

	d = opendir(path);
	if (!d)
		return;

	LOG("Copying all saves from '%s'...", path);
	while ((dir = readdir(d)) != NULL)
	{
		if (dir->d_type == DT_DIR && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0)
		{
			snprintf(savePath, sizeof(savePath), "%s%s/sce_sys/param.sfo", path, dir->d_name);
			if (file_exists(savePath) == SUCCESS)
			{
				snprintf(savePath, sizeof(savePath), "%s%s/", path, dir->d_name);
				copySaveHDD(savePath);
			}
		}
	}
	closedir(d);
}

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

void activateAccount(const char* ex_path)
{
	int ret;
	char path[256];

	snprintf(path, sizeof(path), "%s" "act.dat", ex_path);
	if (file_exists(path) == SUCCESS)
	{
		show_message("Error! The account already has an act.dat");
		return;
	}

	if (mkdirs(ex_path) != SUCCESS)
	{
		show_message("Error! Folder is not available:\n%s", ex_path);
		return;
	}

if (0)
//	if (!apollo_config.account_id && (apollo_config.account_id = create_fake_account(apollo_config.user_id)) == 0)
	{
		show_message("Error! Fake Account could not be assigned to xRegistry.sys");
		return;
	}

	init_loading_screen("Activating PS3...");
//	ret = create_actdat(ex_path, apollo_config.account_id);
	stop_loading_screen();

	if (!ret)
	{
		show_message("Error! Account could not be activated!");
		return;
	}

//	save_app_settings(&apollo_config);

	show_message("Account successfully activated!\nA system reboot might be required");
}

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

	snprintf(in_file_path, sizeof(in_file_path), "%s" "PARAM.SFO", selected_entry->path);
	LOG("Applying SFO patches '%s'...", in_file_path);

	return (patch_sfo(in_file_path, patch) == SUCCESS);
}

int _is_decrypted(list_t* list, const char* fname) {
	list_node_t *node;
	u8 *protected_file_id = NULL;//get_secure_file_id(selected_entry->title_id, "UNPROTECTED");

	if (protected_file_id && (strncmp("UNPROTECTEDGAME", (char*)protected_file_id, 16) == 0))
		return 1;

	for (node = list_head(list); node; node = list_next(node))
		if (strcmp(list_get(node), fname) == 0)
			return 1;

	return 0;
}

int apply_cheat_patches()
{
	int ret = 1;
	char tmpfile[256];
	char* filename;
	code_entry_t* code;
	uint8_t* protected_file_id;
	list_t* decrypted_files = list_alloc();
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
		{
			snprintf(tmpfile, sizeof(tmpfile), "%s%s", selected_entry->path, filename);

			if (!_is_decrypted(decrypted_files, filename))
			{
				LOG("Decrypting '%s'...", filename);
//				protected_file_id = get_secure_file_id(selected_entry->title_id, filename);

if (0)
//				if (decrypt_save_file(selected_entry->path, filename, NULL, protected_file_id))
				{
					list_append(decrypted_files, strdup(filename));
				}
				else
				{
					LOG("Error: failed to decrypt (%s)", filename);
					ret = 0;
					continue;
				}
			}
		}

		if (!apply_cheat_patch_code(tmpfile, selected_entry->title_id, code, APOLLO_LOCAL_CACHE))
		{
			LOG("Error: failed to apply (%s)", code->name);
			ret = 0;
		}

		code->activated = 0;
	}

	for (node = list_head(decrypted_files); (filename = list_get(node)); node = list_next(node))
	{
		LOG("Encrypting '%s'...", filename);
//		protected_file_id = get_secure_file_id(selected_entry->title_id, filename);
		
if (0)
//		if (!encrypt_save_file(selected_entry->path, filename, protected_file_id))
		{
			LOG("Error: failed to encrypt (%s)", filename);
			ret = 0;
		}

		free(filename);
	}

	list_free(decrypted_files);
	free_patch_var_list();
	stop_loading_screen();

	return ret;
}

void resignSave(sfo_patch_t* patch)
{
    if (!apply_sfo_patches(patch))
        show_message("Error! Account changes couldn't be applied");

    LOG("Applying cheats to '%s'...", selected_entry->name);
    if (!apply_cheat_patches())
        show_message("Error! Cheat codes couldn't be applied");

    LOG("Resigning save '%s'...", selected_entry->name);
if (0)
//    if (!pfd_util_init((u8*) apollo_config.idps, apollo_config.user_id, selected_entry->title_id, selected_entry->path) ||
//        (pfd_util_process(PFD_CMD_UPDATE, 0) != SUCCESS))
        show_message("Error! Save %s couldn't be resigned", selected_entry->title_id);
    else
        show_message("Save %s successfully modified!", selected_entry->title_id);

//    pfd_util_end();
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
if (0)
//				if (!pfd_util_init((u8*) apollo_config.idps, apollo_config.user_id, titleid, sfoPath) ||
//					(pfd_util_process(PFD_CMD_UPDATE, 0) != SUCCESS))
					LOG("Error! Save file couldn't be resigned");

//				pfd_util_end();
			}
		}
	}
	closedir(d);

	stop_loading_screen();
}

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

int _copy_save_file(const char* src_path, const char* dst_path, const char* filename)
{
	char src[256], dst[256];

	snprintf(src, sizeof(src), "%s%s", src_path, filename);
	snprintf(dst, sizeof(dst), "%s%s", dst_path, filename);

	return copy_file(src, dst);
}

void decryptSaveFile(const char* filename)
{
	char path[256];

	snprintf(path, sizeof(path), APOLLO_TMP_PATH "%s/", selected_entry->title_id);
	mkdirs(path);

	if (_is_decrypted(NULL, filename))
	{
		_copy_save_file(selected_entry->path, path, filename);
		show_message("Save-game %s is not encrypted. File was not decrypted:\n%s%s", selected_entry->title_id, path, filename);
		return;
	}

	u8* protected_file_id = NULL;//get_secure_file_id(selected_entry->title_id, filename);

	LOG("Decrypt '%s%s' to '%s'...", selected_entry->path, filename, path);

if (0)
//	if (decrypt_save_file(selected_entry->path, filename, path, protected_file_id))
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

	if (_is_decrypted(NULL, filename))
	{
		_copy_save_file(path, selected_entry->path, filename);
		show_message("Save-game %s is not encrypted.\nFile %s was not encrypted", selected_entry->title_id, filename);
		return;
	}

	u8* protected_file_id = NULL;//get_secure_file_id(selected_entry->title_id, filename);

	LOG("Encrypt '%s%s' to '%s'...", path, filename, selected_entry->path);
	_copy_save_file(path, selected_entry->path, filename);

if (0)
//	if (encrypt_save_file(selected_entry->path, filename, protected_file_id))
		show_message("File successfully encrypted to:\n%s%s", selected_entry->path, filename);
	else
		show_message("Error! File %s couldn't be encrypted", filename);
}

void execCodeCommand(code_entry_t* code, const char* codecmd)
{
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
			copySave(selected_entry->path, codecmd[1] ? SAVES_PATH_USB1 : SAVES_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_COPY_SAVE_HDD:
			copySaveHDD(selected_entry->path);
			code->activated = 0;
			break;

		case CMD_EXP_EXDATA_USB:
			exportLicensesZip(codecmd[1] ? EXPORT_PATH_USB1 : EXPORT_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_EXP_LICS_RAPS:
			exportLicensesRap(code->file, codecmd[1]);
			code->activated = 0;
			break;

		case CMD_CREATE_ACT_DAT:
			activateAccount(selected_entry->path);
			code->activated = 0;
			break;

		case CMD_EXP_TROPHY_USB:
			copySave(selected_entry->path, codecmd[1] ? TROPHY_PATH_USB1 : TROPHY_PATH_USB0);
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

		case CMD_COPY_SAVES_USB:
			exportFolder(selected_entry->path, codecmd[1] ? SAVES_PATH_USB1 : SAVES_PATH_USB0, "Copying saves...");
			code->activated = 0;
			break;

		case CMD_EXP_FLASH2_USB:
			exportFlashZip(codecmd[1] ? EXPORT_PATH_USB1 : EXPORT_PATH_USB0);
			code->activated = 0;
			break;

		case CMD_IMP_EXDATA_USB:
			importLicenses(code->file, selected_entry->path);
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

		case CMD_RESIGN_ALL_SAVES:
			resignAllSaves(selected_entry->path);
			code->activated = 0;
			break;

		case CMD_COPY_SAVES_HDD:
			copyAllSavesHDD(selected_entry->path);
			code->activated = 0;
			break;

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

		default:
			break;
	}

}
