#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <time.h>
#include <orbis/NetCtl.h>
#include <orbis/SaveData.h>
#include <orbis/UserService.h>
#include <polarssl/md5.h>

#include "saves.h"
#include "menu.h"
#include "common.h"
#include "util.h"
#include "sfo.h"


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

	_set_dest_path(path, dst, PS4_SAVES_PATH_USB);
	if (mkdirs(path) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", path);
		return;
	}

	if (!http_download(entry->path, file, APOLLO_LOCAL_CACHE "tmpsave.zip", 1))
	{
		show_message("Error downloading save game from:\n%s%s", entry->path, file);
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

static void zipSave(const save_entry_t* entry, const char* exp_path)
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

	tmp = strdup(entry->path);
	*strrchr(tmp, '/') = 0;
	*strrchr(tmp, '/') = 0;

	zip_directory(tmp, entry->path, export_file);

	sprintf(export_file, "%s%08x.txt", exp_path, apollo_config.user_id);
	FILE* f = fopen(export_file, "a");
	if (f)
	{
		fprintf(f, "%08d.zip=[%s] %s\n", fid, entry->title_id, entry->name);
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
		orbis_UpdateSaveParams(mount,
			(char*) sfo_get_param_value(sfo, "MAINTITLE"),
			(char*) sfo_get_param_value(sfo, "SUBTITLE"),
			(char*) sfo_get_param_value(sfo, "DETAIL"),
			*(uint32_t*) sfo_get_param_value(sfo, "SAVEDATA_LIST_PARAM"));
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
		show_message("All files extracted to:\n%s", exp_path);
	else
		show_message("Error: %s couldn't be extracted", file_path);
}

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

static void activateAccount(int user)
{
	uint64_t account = 0;
	char value[SFO_ACCOUNT_ID_SIZE*2+1];

	snprintf(value, sizeof(value), "%016lx", 0x6F6C6C6F70610000 + (~apollo_config.user_id & 0xFFFF));
	if (!osk_dialog_get_text("Enter the Account ID", value, sizeof(value)))
		return;

	if (!sscanf(value, "%lx", &account))
	{
		show_message("Error! Account ID is not valid");
		return;
	};

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
	if (show_dialog(DIALOG_TYPE_YESNO, "Resign save %s/%s?", save->title_id, save->dir_name))
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

		fprintf(f, "<html><head><meta charset=\"UTF-8\"><style>h1, h2 { font-family: arial; } table { border-collapse: collapse; margin: 25px 0; font-size: 0.9em; font-family: sans-serif; min-width: 400px; box-shadow: 0 0 20px rgba(0, 0, 0, 0.15); } table thead tr { background-color: #009879; color: #ffffff; text-align: left; } table th, td { padding: 12px 15px; } table tbody tr { border-bottom: 1px solid #dddddd; } table tbody tr:nth-of-type(even) { background-color: #f3f3f3; } table tbody tr:last-of-type { border-bottom: 2px solid #009879; }</style>");
		fprintf(f, "<script language=\"javascript\">document.addEventListener(\"DOMContentLoaded\",function(){var e;if(\"IntersectionObserver\"in window){e=document.querySelectorAll(\".lazy\");var n=new IntersectionObserver(function(e,t){e.forEach(function(e){if(e.isIntersecting){var t=e.target;t.src=t.dataset.src,t.classList.remove(\"lazy\"),n.unobserve(t)}})});e.forEach(function(e){n.observe(e)})}else{var t;function r(){t&&clearTimeout(t),t=setTimeout(function(){var n=window.pageYOffset;e.forEach(function(e){e.offsetTop<window.innerHeight+n&&(e.src=e.dataset.src,e.classList.remove(\"lazy\"))}),0==e.length&&(document.removeEventListener(\"scroll\",r),window.removeEventListener(\"resize\",r),window.removeEventListener(\"orientationChange\",r))},20)}e=document.querySelectorAll(\".lazy\"),document.addEventListener(\"scroll\",r),window.addEventListener(\"resize\",r),window.addEventListener(\"orientationChange\",r)}});</script>");
		fprintf(f, "<title>Apollo Save Tool</title></head><body><h1>.:: Apollo Save Tool</h1><h2>Index of %s</h2><table><thead><tr><th>Name</th><th>Icon</th><th>Title ID</th><th>Folder</th><th>Location</th></tr></thead><tbody>", selected_entry->path);

		int i = 0;
		for (node = list_head(list); (item = list_get(node)); node = list_next(node), i++)
		{
			if (item->type == FILE_TYPE_MENU || !(item->flags & SAVE_FLAG_PS4) || item->flags & SAVE_FLAG_LOCKED)
				continue;

			fprintf(f, "<tr><td><a href=\"/zip/%08d/%s_%s.zip\">%s</a></td>", i, item->title_id, item->dir_name, item->name);
			fprintf(f, "<td><img class=\"lazy\" data-src=\"");

			if (item->flags & SAVE_FLAG_HDD)
				fprintf(f, "/icon/%s/%s_icon0.png", item->title_id, item->dir_name);
			else
				fprintf(f, "/icon%ssce_sys/icon0.png", strchr(item->path +20, '/'));

			fprintf(f, "\" alt=\"%s\" width=\"228\" height=\"128\"></td>", item->name);
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

		FILE* f = fopen(res->data, "w");
		if (!f)
			return 0;

		for (node = list_head(list); (item = list_get(node)); node = list_next(node))
		{
			if (item->type == FILE_TYPE_MENU || !(item->flags & SAVE_FLAG_PS4))
				continue;

			fprintf(f, "%s=%s\n", item->title_id, item->name);
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
		char mount[32];
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
			*strrchr(base, '/') = 0;
		}
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
				asprintf(&res->data, PS4_SAVES_PATH_HDD "%s/%s_icon0.png", apollo_config.user_id, item->title_id, item->dir_name);
			else
				asprintf(&res->data, "%ssce_sys/icon0.png", item->path);

			return (file_exists(res->data) == SUCCESS);
		}

		return 0;
	}

	// http://ps3-ip:8080/icon/CUSA12345-DIR-NAME/sce_sys/icon0.png
	if (wildcard_match(req->resource, "/icon/*/sce_sys/icon0.png"))
	{
		asprintf(&res->data, "%sAPOLLO/%s", selected_entry->path, req->resource + 6);
		return (file_exists(res->data) == SUCCESS);
	}

	// http://ps3-ip:8080/icon/CUSA12345/DIR-NAME_icon0.png
	if (wildcard_match(req->resource, "/icon/\?\?\?\?\?\?\?\?\?/*_icon0.png"))
	{
		asprintf(&res->data, PS4_SAVES_PATH_HDD "%s", apollo_config.user_id, req->resource + 6);
		return (file_exists(res->data) == SUCCESS);
	}

	return 0;
}

static void enableWebServer(dWebReqHandler_t handler, void* data, int port)
{
	OrbisNetCtlInfo info;

	memset(&info, 0, sizeof(info));
	sceNetCtlGetInfo(ORBIS_NET_CTL_INFO_IP_ADDRESS, &info);
	LOG("Starting local web server %s:%d ...", info.ip_address, port);

	if (dbg_webserver_start(port, handler, data))
	{
		show_message("Web Server listening on http://%s:%d\nPress OK to stop the Server.", info.ip_address, port);
		dbg_webserver_stop();
	}
	else show_message("Error starting Web Server!");
}

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

static void rebuildAppDB(const char* path)
{
	int error = 0;
	char name[ORBIS_USER_SERVICE_MAX_USER_NAME_LENGTH+1];
	OrbisUserServiceRegisteredUserIdList userIdList;

	memset(&userIdList, 0, sizeof(userIdList));
	if (sceUserServiceGetRegisteredUserIdList(&userIdList) < 0)
	{
		show_message("Error getting PS4 user list!");
		return;
	}

	for (int i = 0; i < ORBIS_USER_SERVICE_MAX_REGISTER_USERS; i++)
		if (userIdList.userId[i] != ORBIS_USER_SERVICE_USER_ID_INVALID && !appdb_rebuild(path, userIdList.userId[i]))
		{
			memset(name, 0, sizeof(name));
			sceUserServiceGetUserName(userIdList.userId[i], name, sizeof(name));
			show_message("Database rebuild for user %s failed!", name);
			error++;
		}

	if(!error)
		show_message("Database rebuilt successfully!\nLog out for changes to take effect");
}

static int apply_sfo_patches(save_entry_t* entry, sfo_patch_t* patch)
{
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

            sscanf(code->options->value[code->options->sel], "%lx", &patch->account_id);
            break;

        case SFO_REMOVE_PSID:
            bzero(tmp_psid, SFO_PSID_SIZE);
            patch->psid = tmp_psid;
            break;

        case SFO_CHANGE_TITLE_ID:
            patch->directory = strstr(entry->path, entry->title_id);
            snprintf(in_file_path, sizeof(in_file_path), "%s", entry->path);
            strncpy(tmp_dir, patch->directory, SFO_DIRECTORY_SIZE);

            strncpy(entry->title_id, code->options[0].name[code->options[0].sel], 9);
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

	init_loading_screen("Applying changes...");

	for (node = list_head(entry->codes); (code = list_get(node)); node = list_next(node))
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
			snprintf(tmpfile, sizeof(tmpfile), "%s[%s]%s", APOLLO_LOCAL_CACHE, entry->title_id, filename);
		else
			snprintf(tmpfile, sizeof(tmpfile), "%s%s", entry->path, filename);

		if (!apply_cheat_patch_code(tmpfile, entry->title_id, code, APOLLO_LOCAL_CACHE))
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
        show_message("Error! Account changes couldn't be applied");

    LOG("Applying cheats to '%s'...", entry->name);
    if (!apply_cheat_patches(entry))
        show_message("Error! Cheat codes couldn't be applied");

    show_message("Save %s successfully modified!", entry->title_id);
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
		err_count += (patch_sfo(sfoPath, &patch) != SUCCESS);
	}

	end_progress_bar();

	if (err_count)
		show_message("Error: %d Saves couldn't be resigned", err_count);
	else
		show_message("All saves successfully resigned!");
}

static void exportZipDB(const char* path)
{
	char *tmp;
	char zipfile[256];
	struct tm t;

	if (mkdirs(EXPORT_DB_PATH) != SUCCESS)
	{
		show_message("Error! Export folder is not available:\n%s", EXPORT_DB_PATH);
		return;
	}

	// build file path
	t  = *gmtime(&(time_t){time(NULL)});
	snprintf(zipfile, sizeof(zipfile), "%sdb_%d-%02d-%02d_%02d%02d%02d.zip", EXPORT_DB_PATH, t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
	tmp = strdup(path);
	*strrchr(tmp, '/') = 0;

	init_loading_screen("Exporting system database...");
	zip_directory(tmp, path, zipfile);
	stop_loading_screen();
	free(tmp);

	show_message("Zip file successfully saved to:\n%s", zipfile);
}

static void importZipDB(const char* dst_path, const char* zipfile)
{
	char path[256];

	snprintf(path, sizeof(path), "%s%s", EXPORT_DB_PATH, zipfile);
	LOG("Importing Backup %s ...", path);

	if (extract_zip(path, dst_path))
        show_message("DB Backup %s successfully restored!", zipfile);
    else
        show_message("Error! Backup %s couldn't be restored", zipfile);
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
		show_message("File successfully exported to:\n%s%s", path, filename);
	else
		show_message("Error! File %s couldn't be exported", filename);
}

static void encryptSaveFile(const save_entry_t* entry, const char* filename)
{
	char path[256];

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s_%s/%s", apollo_config.user_id, entry->title_id, entry->dir_name, filename);
	if (file_exists(path) != SUCCESS)
	{
		show_message("Error! Can't find decrypted save-game file:\n%s", path);
		return;
	}

	snprintf(path, sizeof(path), APOLLO_USER_PATH "%s_%s/", apollo_config.user_id, entry->title_id, entry->dir_name);
	LOG("Encrypt '%s%s' to '%s'...", path, filename, entry->path);

	if (_copy_save_file(path, entry->path, filename))
		show_message("File successfully imported to:\n%s%s", entry->path, filename);
	else
		show_message("Error! File %s couldn't be imported", filename);
}

static void downloadLink(const char* path)
{
	char url[256] = "http://";
	char out_path[256];

	if (!osk_dialog_get_text("Download URL", url, sizeof(url)))
		return;

	char *fname = strrchr(url, '/');
	snprintf(out_path, sizeof(out_path), "%s%s", path, fname ? ++fname : "download.bin");

	if (http_download(url, NULL, out_path, 1))
		show_message("File successfully downloaded to:\n%s", out_path);
	else
		show_message("Error! File couldn't be downloaded");
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
			decryptSaveFile(selected_entry, code->options[0].name[code->options[0].sel]);
			code->activated = 0;
			break;

		case CMD_DOWNLOAD_USB:
			if (selected_entry->flags & SAVE_FLAG_PS4)
				downloadSave(selected_entry, code->file, codecmd[1]);
			
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
			copyKeystone(0);
			code->activated = 0;
			break;

		case CMD_IMP_KEYSTONE:
			copyKeystone(1);
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
/*
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
			encryptSaveFile(selected_entry, code->options[0].name[code->options[0].sel]);
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
			importZipDB(selected_entry->path, code->options[0].name[code->options[0].sel]);
			code->activated = 0;
			break;

		case CMD_DB_REBUILD:
			rebuildAppDB(code->file);
			code->activated = 0;
			break;

		case CMD_DB_DEL_FIX:
			if (appdb_fix_delete(code->file, apollo_config.user_id))
				show_message("User %x database fixed successfully!\nLog out for changes to take effect", apollo_config.user_id);
			else
				show_message("Database fix failed!");

			code->activated = 0;
			break;

		case CMD_DB_DLC_REBUILD:
			if (addcont_dlc_rebuild(code->file))
				show_message("DLC database fixed successfully!\nLog out for changes to take effect");
			else
				show_message("DLC Database rebuild failed!");

			code->activated = 0;
			break;

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
