#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <orbis/SaveData.h>

#include "saves.h"
#include "common.h"
#include "sfo.h"
#include "settings.h"
#include "util.h"
#include "sqlite3.h"

#define UTF8_CHAR_STAR		"\xE2\x98\x85"

#define CHAR_ICON_ZIP		"\x0C"
#define CHAR_ICON_COPY		"\x0B"
#define CHAR_ICON_SIGN		"\x06"
#define CHAR_ICON_USER		"\x07"
#define CHAR_ICON_LOCK		"\x08"
#define CHAR_ICON_WARN		"\x0F"

const void* sqlite3_get_sqlite3Apis();
int sqlite3_memvfs_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);
int sqlite3_memvfs_dump(sqlite3 *db, const char *zSchema, const char *zFilename);

static sqlite3* open_sqlite_db(const char* db_path)
{
	uint8_t* db_buf;
	size_t db_size;
	sqlite3 *db;
	const sqlite3_api_routines* api = sqlite3_get_sqlite3Apis();

	// Open an in-memory database to use as a handle for loading the memvfs extension
	if (sqlite3_open(":memory:", &db) != SQLITE_OK)
	{
		LOG("open :memory: %s", sqlite3_errmsg(db));
		return NULL;
	}

	sqlite3_enable_load_extension(db, 1);
	if (sqlite3_memvfs_init(db, NULL, api) != SQLITE_OK_LOAD_PERMANENTLY)
	{
		LOG("Error loading extension: %s", "memvfs");
		return NULL;
	}

	// Done with this database
	sqlite3_close(db);

	if (read_buffer(db_path, &db_buf, &db_size) != SUCCESS)
	{
		LOG("Cannot open database file: %s", db_path);
		return NULL;
	}

	// And open that memory with memvfs now that it holds a valid database
	char *memuri = sqlite3_mprintf("file:memdb?ptr=0x%p&sz=%lld&freeonclose=1", db_buf, db_size);
	LOG("Opening '%s' (%ld bytes)...", db_path, db_size);

	if (sqlite3_open_v2(memuri, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_URI, "memvfs") != SQLITE_OK)
	{
		LOG("Error open memvfs: %s", sqlite3_errmsg(db));
		return NULL;
	}
	sqlite3_free(memuri);

	if (sqlite3_exec(db, "PRAGMA journal_mode = OFF;", NULL, NULL, NULL) != SQLITE_OK)
		LOG("Error set pragma: %s", sqlite3_errmsg(db));

	return db;
}

static int get_appdb_title(sqlite3* db, const char* titleid, char* name)
{
	sqlite3_stmt* res;

	if (!db)
		return 0;

	char* query = sqlite3_mprintf("SELECT titleId, val FROM tbl_appinfo WHERE key='TITLE' AND (titleId = %Q "
		"OR titleId = (SELECT titleId FROM tbl_appinfo WHERE key='INSTALL_DIR_SAVEDATA' AND val = %Q))", titleid, titleid);

	if (sqlite3_prepare_v2(db, query, -1, &res, NULL) != SQLITE_OK || sqlite3_step(res) != SQLITE_ROW)
	{
		LOG("Failed to fetch details: %s", titleid);
		sqlite3_free(query);
		return 0;
	}

	strncpy(name, (const char*) sqlite3_column_text(res, 1), ORBIS_SAVE_DATA_DETAIL_MAXSIZE);
	sqlite3_free(query);
	return 1;
}

int addcont_dlc_rebuild(const char* db_path)
{
	char path[256];
	uint8_t hdr[0x80];
	DIR *dp, *dp2;
	struct dirent *dirp, *dirp2;
	sqlite3* db;
	char* query;

	dp = opendir("/user/addcont");
	if (!dp)
	{
		LOG("Failed to open /user/addcont/");
		return 0;
	}

	db = open_sqlite_db(db_path);
	if (!db)
		return 0;

	while ((dirp = readdir(dp)) != NULL)
	{
		if (dirp->d_type != DT_DIR || dirp->d_namlen != 9)
			continue;

		snprintf(path, sizeof(path), "/user/addcont/%s", dirp->d_name);
		dp2 = opendir(path);
		if (!dp2)
			continue;

		while ((dirp2 = readdir(dp2)) != NULL)
		{
			if (dirp2->d_type != DT_DIR || dirp2->d_namlen != 16)
				continue;

			snprintf(path, sizeof(path), "/user/addcont/%s/%s/ac.pkg", dirp->d_name, dirp2->d_name);
			if (read_file(path, hdr, sizeof(hdr)) != SUCCESS)
				continue;

			LOG("Adding %s to addcont...", dirp2->d_name);
			query = sqlite3_mprintf("INSERT OR IGNORE INTO addcont(title_id, dir_name, content_id, title, version, attribute, status) "
				"VALUES(%Q, %Q, %Q, 'DLC %s (Restored by Apollo)', 536870912, '01.00', 0)",
				dirp->d_name, dirp2->d_name, hdr + 0x40, dirp2->d_name);

			if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK)
				LOG("addcont insert failed: %s", sqlite3_errmsg(db));

			sqlite3_free(query);
		}
		closedir(dp2);
	}
	closedir(dp);

	LOG("Saving database to %s", db_path);
	sqlite3_memvfs_dump(db, NULL, db_path);
	sqlite3_close(db);

	return 1;
}

int appdb_fix_delete(const char* db_path, uint32_t userid)
{
	sqlite3* db;
	char* query;
	char where[] = "WHERE titleId != 'CUSA00001' AND metaDataPath LIKE '/user/appmeta/_________'";

	db = open_sqlite_db(db_path);
	if (!db)
		return 0;

	LOG("Fixing %s (tbl_appbrowse_%010d) delete...", db_path, userid);
	query = sqlite3_mprintf("UPDATE tbl_appbrowse_%010d SET canRemove=1 %s;"
		"UPDATE tbl_appinfo SET val=1 WHERE key='_uninstallable' AND titleId IN (SELECT titleId FROM tbl_appbrowse_%010d %s);"
		"UPDATE tbl_appinfo SET val=100 WHERE key='_sort_priority' AND titleId IN (SELECT titleId FROM tbl_appbrowse_%010d %s);",
		userid, where, userid, where, userid, where);

	if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK)
	{
		LOG("Failed to execute query: %s", sqlite3_errmsg(db));
		sqlite3_free(query);
		return 0;
	}

	LOG("Saving database to %s", db_path);
	sqlite3_memvfs_dump(db, NULL, db_path);
	sqlite3_free(query);
	sqlite3_close(db);

	return 1;
}

static void insert_appinfo_row(sqlite3* db, const char* titleId, const char* key, const char* value)
{
	char* query = sqlite3_mprintf("INSERT OR IGNORE INTO tbl_appinfo(titleId, key, val) VALUES(%Q, %Q, %Q)", titleId, key, value);

	if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK)
		LOG("tbl_appinfo insert failed: %s", sqlite3_errmsg(db));

	sqlite3_free(query);
}

static void insert_appinfo_row_int(sqlite3* db, const char* titleId, const char* key, uint64_t value)
{
	char* query = sqlite3_mprintf("INSERT OR IGNORE INTO tbl_appinfo(titleId, key, val) VALUES(%Q, %Q, %ld)", titleId, key, value);

	if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK)
		LOG("tbl_appinfo insert failed: %s", sqlite3_errmsg(db));

	sqlite3_free(query);
}

static void insert_appinfo_sfo_value(sqlite3* db, const char* titleId, const char* key, sfo_context_t* sfo)
{
	char* value = (char*) sfo_get_param_value(sfo, key);

	if (value)
		insert_appinfo_row(db, titleId, key, value);
}

static void insert_appinfo_sfo_value_int(sqlite3* db, const char* titleId, const char* key, sfo_context_t* sfo)
{
	uint64_t tmp;
	void* value = sfo_get_param_value(sfo, key);

	tmp = (value ? *(uint32_t*) value : 0);
	insert_appinfo_row_int(db, titleId, key, tmp);
}

int appdb_rebuild(const char* db_path, uint32_t userid)
{
	int fw, i;
	DIR *dp;
	struct dirent *dirp;
	char path[256];
	sqlite3* db;
	sqlite3_stmt* res;
	sfo_context_t* sfo;
	uint64_t pkg_size;

	if ((fw = get_firmware_version()) < 0)
		return 0;

	dp = opendir("/user/app");
	if (!dp)
	{
		LOG("Failed to open /user/app/");
		return 0;
	}

	db = open_sqlite_db(db_path);
	if (!db)
		return 0;

	while ((dirp = readdir(dp)) != NULL)
	{
		if (dirp->d_type != DT_DIR || strlen(dirp->d_name) != 9)
			continue;

		snprintf(path, sizeof(path), "SELECT 1 FROM tbl_appbrowse_%010d WHERE titleId='%s'", userid, dirp->d_name);
		if (sqlite3_prepare_v2(db, path, -1, &res, NULL) == SQLITE_OK && sqlite3_step(res) == SQLITE_ROW)
		{
			LOG("(%s) already in database", dirp->d_name);
			continue;
		}

		snprintf(path, sizeof(path), "/user/app/%s/app.pkg", dirp->d_name);
		if (get_file_size(path, &pkg_size) < 0)
		{
			LOG("Failed to get %s file size: %s", dirp->d_name, path);
			continue;
		}

		snprintf(path, sizeof(path), "/system_data/priv/appmeta/%s/param.sfo", dirp->d_name);
		sfo = sfo_alloc();
		if (sfo_read(sfo, path) < 0)
		{
			LOG("Unable to read from '%s'", path);
			sfo_free(sfo);
			continue;
		}

		char *sfo_title = (char*) sfo_get_param_value(sfo, "TITLE");
		char *sfo_titleid = (char*) sfo_get_param_value(sfo, "TITLE_ID");
		char *sfo_content = (char*) sfo_get_param_value(sfo, "CONTENT_ID");
		char *sfo_category = (char*) sfo_get_param_value(sfo, "CATEGORY");
		if (strcmp(sfo_category, "gp") == 0)
			sfo_category = "gd";

		LOG("Adding (%s) %s '%s' to tbl_appbrowse_%010d...", sfo_titleid, sfo_content, sfo_title, userid);
		char* query = sqlite3_mprintf("INSERT INTO tbl_appbrowse_%010d VALUES (%Q, %Q, %Q, '/user/appmeta/%s',"
			"'2020-01-01 20:20:03.000', 0, 0, 5, 1, 100, 0, 1, 5, 1, %Q, 0, 0, 0, 0, NULL, NULL, NULL, %ld,"
			"'2020-01-01 20:20:01.000', 0, %Q, NULL, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,"
			"0, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, '2020-01-01 20:20:02.000'%s)",
			userid, sfo_titleid, sfo_content, sfo_title, dirp->d_name, sfo_category, pkg_size,
			(strcmp(sfo_category, "gde") == 0) ? "app" : "game",
			(fw <= 0x555) ? "" : ",0,0,0,0,0,NULL");

		if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK)
		{
			LOG("Error inserting '%s': %s", sfo_titleid, sqlite3_errmsg(db));
			sqlite3_free(query);
			sfo_free(sfo);
			continue;
		}

		LOG("Adding (%s) attributes to tbl_appinfo...", sfo_titleid);
		insert_appinfo_row(db, sfo_titleid, "TITLE", sfo_title);
		insert_appinfo_row(db, sfo_titleid, "TITLE_ID", sfo_titleid);
		insert_appinfo_row(db, sfo_titleid, "CATEGORY", sfo_category);
		insert_appinfo_row(db, sfo_titleid, "CONTENT_ID", sfo_content);

		insert_appinfo_sfo_value(db, sfo_titleid, "APP_VER", sfo);
		insert_appinfo_sfo_value(db, sfo_titleid, "VERSION", sfo);
		insert_appinfo_sfo_value(db, sfo_titleid, "FORMAT", sfo);
		insert_appinfo_sfo_value(db, sfo_titleid, "INSTALL_DIR_SAVEDATA", sfo);

		insert_appinfo_sfo_value_int(db, sfo_titleid, "APP_TYPE", sfo);
		insert_appinfo_sfo_value_int(db, sfo_titleid, "ATTRIBUTE", sfo);
		insert_appinfo_sfo_value_int(db, sfo_titleid, "PARENTAL_LEVEL", sfo);
		insert_appinfo_sfo_value_int(db, sfo_titleid, "SYSTEM_VER", sfo);
		insert_appinfo_sfo_value_int(db, sfo_titleid, "DOWNLOAD_DATA_SIZE", sfo);
		insert_appinfo_sfo_value_int(db, sfo_titleid, "REMOTE_PLAY_KEY_ASSIGN", sfo);
		insert_appinfo_sfo_value_int(db, sfo_titleid, "ATTRIBUTE_INTERNAL", sfo);
		insert_appinfo_sfo_value_int(db, sfo_titleid, "DISP_LOCATION_1", sfo);
		insert_appinfo_sfo_value_int(db, sfo_titleid, "DISP_LOCATION_2", sfo);
		insert_appinfo_sfo_value_int(db, sfo_titleid, "PT_PARAM", sfo);

		for (i = 1; i < 5; i++)
		{
			snprintf(path, sizeof(path), "USER_DEFINED_PARAM_%d", i);
			insert_appinfo_sfo_value_int(db, sfo_titleid, path, sfo);
		}
		for (i = 1; i < 8; i++)
		{
			snprintf(path, sizeof(path), "SERVICE_ID_ADDCONT_ADD_%d", i);
			insert_appinfo_sfo_value(db, sfo_titleid, path, sfo);

			snprintf(path, sizeof(path), "SAVE_DATA_TRANSFER_TITLE_ID_LIST_%d", i);
			insert_appinfo_sfo_value(db, sfo_titleid, path, sfo);
		}
		for (i = 0; i < 30; i++)
		{
			snprintf(path, sizeof(path), "TITLE_%02d", i);
			insert_appinfo_sfo_value(db, sfo_titleid, path, sfo);
		}

		snprintf(path, sizeof(path), "/user/appmeta/%s", dirp->d_name);
		insert_appinfo_row(db, sfo_titleid, "_metadata_path", path);

		snprintf(path, sizeof(path), "/user/app/%s", dirp->d_name);
		insert_appinfo_row(db, sfo_titleid, "_org_path", path);

		insert_appinfo_row_int(db, sfo_titleid, "#_size", pkg_size);
		insert_appinfo_row_int(db, sfo_titleid, "#_contents_status", 0);
		insert_appinfo_row_int(db, sfo_titleid, "#_access_index", 1);
		insert_appinfo_row_int(db, sfo_titleid, "_contents_location", 0);
		insert_appinfo_row_int(db, sfo_titleid, "#_update_index", 74);
		insert_appinfo_row_int(db, sfo_titleid, "#exit_type", 0);
		insert_appinfo_row_int(db, sfo_titleid, "_contents_ext_type", 0);
		insert_appinfo_row_int(db, sfo_titleid, "_current_slot", 0);
		insert_appinfo_row_int(db, sfo_titleid, "_disable_live_detail", 0);
		insert_appinfo_row_int(db, sfo_titleid, "_hdd_location", 0);
		insert_appinfo_row_int(db, sfo_titleid, "_path_info", 0);
		insert_appinfo_row_int(db, sfo_titleid, "_path_info_2", 0);
		insert_appinfo_row_int(db, sfo_titleid, "_size_other_hdd", 0);
		insert_appinfo_row_int(db, sfo_titleid, "_sort_priority", 100);
		insert_appinfo_row_int(db, sfo_titleid, "_uninstallable", 1);
		insert_appinfo_row_int(db, sfo_titleid, "_view_category", 0);
		insert_appinfo_row_int(db, sfo_titleid, "_working_status", 0);
		insert_appinfo_row_int(db, sfo_titleid, "#_booted", 1);
		insert_appinfo_row(db, sfo_titleid, "#_last_access_time", "2020-01-01 20:20:03.000");
		insert_appinfo_row(db, sfo_titleid, "#_mtime", "2020-01-01 20:20:02.000");
		insert_appinfo_row(db, sfo_titleid, "#_promote_time", "2020-01-01 20:20:01.000");

		sqlite3_free(query);
		sfo_free(sfo);
	}

	LOG("Saving database to %s", db_path);
	sqlite3_memvfs_dump(db, NULL, db_path);
	sqlite3_close(db);
	closedir(dp);

	return 1;
}

int orbis_SaveDelete(const save_entry_t *save)
{
	OrbisSaveDataDelete del;
	OrbisSaveDataDirName dir;
	OrbisSaveDataTitleId title;

	memset(&del, 0, sizeof(OrbisSaveDataDelete));
	memset(&dir, 0, sizeof(OrbisSaveDataDirName));
	memset(&title, 0, sizeof(OrbisSaveDataTitleId));
	strlcpy(dir.data, save->dir_name, sizeof(dir.data));
	strlcpy(title.data, save->title_id, sizeof(title.data));

	del.userId = apollo_config.user_id;
	del.dirName = &dir;
	del.titleId = &title;

	if (sceSaveDataDelete(&del) < 0) {
		LOG("DELETE_ERROR");
		return 0;
	}

	return 1;
}

int orbis_SaveUmount(const char* mountPath)
{
	OrbisSaveDataMountPoint umount;

	memset(&umount, 0, sizeof(OrbisSaveDataMountPoint));
	strncpy(umount.data, mountPath, sizeof(umount.data));

	int32_t umountErrorCode = sceSaveDataUmount(&umount);
	
	if (umountErrorCode < 0)
	{
		LOG("UMOUNT_ERROR (%X)", umountErrorCode);
		notifi(NULL, "Warning! Save couldn't be unmounted!");
		disable_unpatch();
	}

	return (umountErrorCode == SUCCESS);
}

int orbis_SaveMount(const save_entry_t *save, uint32_t mount_mode, char* mount_path)
{
	OrbisSaveDataDirName dirName;
	OrbisSaveDataTitleId titleId;
	int32_t saveDataInitializeResult = sceSaveDataInitialize3(0);

	if (saveDataInitializeResult != SUCCESS)
	{
		LOG("Failed to initialize save data library (%X)", saveDataInitializeResult);
		return 0;
	}

	memset(&dirName, 0, sizeof(OrbisSaveDataDirName));
	memset(&titleId, 0, sizeof(OrbisSaveDataTitleId));
	strlcpy(dirName.data, save->dir_name, sizeof(dirName.data));
	strlcpy(titleId.data, save->title_id, sizeof(titleId.data));

	OrbisSaveDataMount mount;
	memset(&mount, 0, sizeof(OrbisSaveDataMount));

	OrbisSaveDataFingerprint fingerprint;
	memset(&fingerprint, 0, sizeof(OrbisSaveDataFingerprint));
	strlcpy(fingerprint.data, "0000000000000000000000000000000000000000000000000000000000000000", sizeof(fingerprint.data));

	mount.userId = apollo_config.user_id;
	mount.dirName = dirName.data;
	mount.fingerprint = fingerprint.data;
	mount.titleId = titleId.data;
	mount.blocks = save->blocks;
	mount.mountMode = mount_mode | ORBIS_SAVE_DATA_MOUNT_MODE_DESTRUCT_OFF;
	
	OrbisSaveDataMountResult mountResult;
	memset(&mountResult, 0, sizeof(OrbisSaveDataMountResult));

	int32_t mountErrorCode = sceSaveDataMount(&mount, &mountResult);
	if (mountErrorCode < 0)
	{
		LOG("ERROR (%X): can't mount '%s/%s'", mountErrorCode, save->title_id, save->dir_name);
		return 0;
	}

	LOG("'%s/%s' mountPath (%s)", save->title_id, save->dir_name, mountResult.mountPathName);
	strncpy(mount_path, mountResult.mountPathName, ORBIS_SAVE_DATA_MOUNT_POINT_DATA_MAXSIZE);

	return 1;
}

int orbis_UpdateSaveParams(const char* mountPath, const char* title, const char* subtitle, const char* details, uint32_t userParam)
{
	OrbisSaveDataParam saveParams;
	OrbisSaveDataMountPoint mount;

	memset(&saveParams, 0, sizeof(OrbisSaveDataParam));
	memset(&mount, 0, sizeof(OrbisSaveDataMountPoint));

	strlcpy(mount.data, mountPath, sizeof(mount.data));
	strlcpy(saveParams.title, title, ORBIS_SAVE_DATA_TITLE_MAXSIZE);
	strlcpy(saveParams.subtitle, subtitle, ORBIS_SAVE_DATA_SUBTITLE_MAXSIZE);
	strlcpy(saveParams.details, details, ORBIS_SAVE_DATA_DETAIL_MAXSIZE);
	saveParams.userParam = userParam;
	saveParams.mtime = time(NULL);

	int32_t setParamResult = sceSaveDataSetParam(&mount, ORBIS_SAVE_DATA_PARAM_TYPE_ALL, &saveParams, sizeof(OrbisSaveDataParam));
	if (setParamResult < 0) {
		LOG("sceSaveDataSetParam error (%X)", setParamResult);
		return 0;
	}

	return (setParamResult == SUCCESS);
}

/*
 * Function:		endsWith()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Checks to see if a ends with b
 * Arguments:
 *	a:				String
 *	b:				Potential end
 * Return:			pointer if true, NULL if false
 */
static char* endsWith(const char * a, const char * b)
{
	int al = strlen(a), bl = strlen(b);
    
	if (al < bl)
		return NULL;

	a += (al - bl);
	while (*a)
		if (toupper(*a++) != toupper(*b++)) return NULL;

	return (char*) (a - bl);
}

/*
 * Function:		readFile()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		reads the contents of a file into a new buffer
 * Arguments:
 *	path:			Path to file
 * Return:			Pointer to the newly allocated buffer
 */
char * readTextFile(const char * path, long* size)
{
	FILE *f = fopen(path, "rb");

	if (!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	long fsize = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (fsize <= 0)
	{
		fclose(f);
		return NULL;
	}

	char * string = malloc(fsize + 1);
	fread(string, fsize, 1, f);
	fclose(f);

	string[fsize] = 0;
	if (size)
		*size = fsize;

	return string;
}

static code_entry_t* _createCmdCode(uint8_t type, const char* name, char code)
{
	code_entry_t* entry = (code_entry_t *)calloc(1, sizeof(code_entry_t));
	entry->type = type;
	entry->name = strdup(name);
	asprintf(&entry->codes, "%c", code);

	return entry;
}

static option_entry_t* _initOptions(int count)
{
	option_entry_t* options = (option_entry_t*)malloc(sizeof(option_entry_t));

	options->id = 0;
	options->sel = -1;
	options->size = count;
	options->line = NULL;
	options->value = malloc (sizeof(char *) * count);
	options->name = malloc (sizeof(char *) * count);

	return options;
}

static option_entry_t* _createOptions(int count, const char* name, char value)
{
	option_entry_t* options = _initOptions(count);

	asprintf(&options->name[0], "%s %d", name, 0);
	asprintf(&options->value[0], "%c%c", value, STORAGE_USB0);
	asprintf(&options->name[1], "%s %d", name, 1);
	asprintf(&options->value[1], "%c%c", value, STORAGE_USB1);

	return options;
}

static save_entry_t* _createSaveEntry(uint16_t flag, const char* name)
{
	save_entry_t* entry = (save_entry_t *)calloc(1, sizeof(save_entry_t));
	entry->flags = flag;
	entry->name = strdup(name);

	return entry;
}

static void _walk_dir_list(const char* startdir, const char* inputdir, const char* mask, list_t* list)
{
	char fullname[256];	
	struct dirent *dirp;
	int len = strlen(startdir);
	DIR *dp = opendir(inputdir);

	if (!dp) {
		LOG("Failed to open input directory: '%s'", inputdir);
		return;
	}

	while ((dirp = readdir(dp)) != NULL)
	{
		if ((strcmp(dirp->d_name, ".")  == 0) || (strcmp(dirp->d_name, "..") == 0) || (strcmp(dirp->d_name, "sce_sys") == 0))
			continue;

		snprintf(fullname, sizeof(fullname), "%s%s", inputdir, dirp->d_name);

		if (dirp->d_type == DT_DIR)
		{
			strcat(fullname, "/");
			_walk_dir_list(startdir, fullname, mask, list);
		}
		else if (wildcard_match_icase(dirp->d_name, mask))
		{
			//LOG("Adding file '%s'", fullname+len);
			list_append(list, strdup(fullname+len));
		}
	}
	closedir(dp);
}

static option_entry_t* _getFileOptions(const char* save_path, const char* mask, uint8_t is_cmd)
{
	char *filename;
	list_t* file_list;
	list_node_t* node;
	int i = 0;
	option_entry_t* opt;

	if (dir_exists(save_path) != SUCCESS)
		return NULL;

	LOG("Loading filenames {%s} from '%s'...", mask, save_path);

	file_list = list_alloc();
	_walk_dir_list(save_path, save_path, mask, file_list);

	if (!list_count(file_list))
	{
		is_cmd = 0;
		asprintf(&filename, CHAR_ICON_WARN " --- %s%s --- " CHAR_ICON_WARN, save_path, mask);
		list_append(file_list, filename);
	}
	opt = _initOptions(list_count(file_list));

	for (node = list_head(file_list); (filename = list_get(node)); node = list_next(node))
	{
		LOG("Adding '%s' (%s)", filename, mask);
		opt->name[i] = filename;

		if (is_cmd)
			asprintf(&opt->value[i], "%c", is_cmd);
		else
			asprintf(&opt->value[i], "%s", mask);

		i++;
	}

	list_free(file_list);

	return opt;
}

static void _addBackupCommands(save_entry_t* item)
{
	code_entry_t* cmd;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Apply Changes & Resign", CMD_RESIGN_SAVE);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " File Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy save game", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(3, "Copy Save to USB", CMD_COPY_SAVE_USB);
	asprintf(&cmd->options->name[2], "Copy Save to HDD");
	asprintf(&cmd->options->value[2], "%c", CMD_COPY_SAVE_HDD);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_ZIP " Export save game to Zip", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(3, "Export Zip to USB", CMD_EXPORT_ZIP_USB);
	asprintf(&cmd->options->name[2], "Export Zip to HDD");
	asprintf(&cmd->options->value[2], "%c", CMD_EXPORT_ZIP_HDD);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export decrypted save files", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _getFileOptions(item->path, "*", CMD_DECRYPT_FILE);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Import decrypted save files", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _getFileOptions(item->path, "*", CMD_IMPORT_DATA_FILE);
	list_append(item->codes, cmd);
}
/*
static option_entry_t* _getSaveTitleIDs(const char* title_id)
{
	int count = 1;
	option_entry_t* opt;
	char tmp[16];
	const char *ptr;
	const char *tid = NULL;//get_game_title_ids(title_id);

	if (!tid)
		tid = title_id;

	ptr = tid;
	while (*ptr)
		if (*ptr++ == '/') count++;

	LOG("Adding (%d) TitleIDs=%s", count, tid);

	opt = _initOptions(count);
	int i = 0;

	ptr = tid;
	while (*ptr++)
	{
		if ((*ptr == '/') || (*ptr == 0))
		{
			memset(tmp, 0, sizeof(tmp));
			strncpy(tmp, tid, ptr - tid);
			asprintf(&opt->name[i], "%s", tmp);
			asprintf(&opt->value[i], "%c", SFO_CHANGE_TITLE_ID);
			tid = ptr+1;
			i++;
		}
	}

	return opt;
}
*/
static void _addSfoCommands(save_entry_t* save)
{
	code_entry_t* cmd;

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Keystone Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Export Keystone", CMD_EXP_KEYSTONE);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Import Keystone", CMD_IMP_KEYSTONE);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Dump save Fingerprint", CMD_EXP_FINGERPRINT);
	list_append(save->codes, cmd);

	return;
/*
	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " SFO Patches " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_SFO, CHAR_ICON_USER " Change Account ID", SFO_CHANGE_ACCOUNT_ID);
	cmd->options_count = 1;
	cmd->options = _initOptions(2);
	cmd->options->name[0] = strdup("Remove ID/Offline");
	cmd->options->value[0] = calloc(1, SFO_ACCOUNT_ID_SIZE);
	cmd->options->name[1] = strdup("Fake Owner");
	cmd->options->value[1] = strdup("ffffffffffffffff");
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_SFO, CHAR_ICON_USER " Remove Console ID", SFO_REMOVE_PSID);
	list_append(save->codes, cmd);

	if (save->flags & SAVE_FLAG_LOCKED)
	{
		cmd = _createCmdCode(PATCH_SFO, CHAR_ICON_LOCK " Remove copy protection", SFO_UNLOCK_COPY);
		list_append(save->codes, cmd);
	}

	cmd = _createCmdCode(PATCH_SFO, CHAR_ICON_USER " Change Region Title ID", SFO_CHANGE_TITLE_ID);
	cmd->options_count = 1;
	cmd->options = _getSaveTitleIDs(save->title_id);
	list_append(save->codes, cmd);
*/
}

static int set_pfs_codes(save_entry_t* item)
{
	code_entry_t* cmd;
	item->codes = list_alloc();

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy Save game to HDD", CMD_COPY_PFS);
	list_append(item->codes, cmd);

	return list_count(item->codes);
}

option_entry_t* get_file_entries(const char* path, const char* mask)
{
	return _getFileOptions(path, mask, CMD_CODE_NULL);
}

/*
 * Function:		ReadLocalCodes()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Reads an entire NCL file into an array of code_entry
 * Arguments:
 *	path:			Path to ncl
 *	_count_count:	Pointer to int (set to the number of codes within the ncl)
 * Return:			Returns an array of code_entry, null if failed to load
 */
int ReadCodes(save_entry_t * save)
{
	code_entry_t * code;
	char filePath[256];
	char * buffer = NULL;
	char mount[32];
	char *tmp;

	if (save->flags & SAVE_FLAG_LOCKED)
		return set_pfs_codes(save);

	save->codes = list_alloc();

	if (save->flags & SAVE_FLAG_HDD)
	{
		if (!orbis_SaveMount(save, ORBIS_SAVE_DATA_MOUNT_MODE_RDONLY, mount))
		{
			code = _createCmdCode(PATCH_NULL, CHAR_ICON_WARN " --- Error Mounting Save! Check Save Mount Patches --- " CHAR_ICON_WARN, CMD_CODE_NULL);
			list_append(save->codes, code);
			return list_count(save->codes);
		}
		tmp = save->path;
		asprintf(&save->path, APOLLO_SANDBOX_PATH, mount);
	}

	_addBackupCommands(save);
	_addSfoCommands(save);

	snprintf(filePath, sizeof(filePath), APOLLO_DATA_PATH "%s.savepatch", save->title_id);
	if (file_exists(filePath) != SUCCESS)
		goto skip_end;

	code = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Cheats " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);	
	list_append(save->codes, code);

	code = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Raw Patch File", CMD_VIEW_RAW_PATCH);
	list_append(save->codes, code);

	LOG("Loading BSD codes '%s'...", filePath);
	buffer = readTextFile(filePath, NULL);
	load_patch_code_list(buffer, save->codes, &get_file_entries, save->path);
	free (buffer);

	LOG("Loaded %zu codes", list_count(save->codes));

skip_end:
	if (save->flags & SAVE_FLAG_HDD)
	{
		orbis_SaveUmount(mount);
		free(save->path);
		save->path = tmp;
	}

	return list_count(save->codes);
}

int ReadTrophies(save_entry_t * game)
{
	int trop_count = 0;
	code_entry_t * trophy;
	char query[256];
	sqlite3 *db;
	sqlite3_stmt *res;

	if ((db = open_sqlite_db(game->path)) == NULL)
		return 0;

	game->codes = list_alloc();
/*
	trophy = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Apply Changes & Resign Trophy Set", CMD_RESIGN_TROPHY);
	list_append(game->codes, trophy);

	trophy = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Backup Trophy Set to USB", CMD_CODE_NULL);
	trophy->file = strdup(game->path);
	trophy->options_count = 1;
	trophy->options = _createOptions(2, "Copy Trophy to USB", CMD_EXP_TROPHY_USB);
	list_append(game->codes, trophy);

	trophy = _createCmdCode(PATCH_COMMAND, CHAR_ICON_ZIP " Export Trophy Set to Zip", CMD_CODE_NULL);
	trophy->file = strdup(game->path);
	trophy->options_count = 1;
	trophy->options = _createOptions(2, "Save .Zip to USB", CMD_EXPORT_ZIP_USB);
	list_append(game->codes, trophy);
*/
	trophy = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Trophies " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(game->codes, trophy);

	snprintf(query, sizeof(query), "SELECT title_id, trophy_title_id, title, description, grade, unlocked, id FROM tbl_trophy_flag WHERE title_id = %d", game->blocks);

	if (sqlite3_prepare_v2(db, query, -1, &res, NULL) != SQLITE_OK)
	{
		LOG("Failed to fetch data: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 0;
	}

	while (sqlite3_step(res) == SQLITE_ROW)
	{
		snprintf(query, sizeof(query), "   %s", sqlite3_column_text(res, 2));
		trophy = _createCmdCode(PATCH_NULL, query, CMD_CODE_NULL);

		asprintf(&trophy->codes, "%s\n", sqlite3_column_text(res, 3));

		switch (sqlite3_column_int(res, 4))
		{
		case 4:
			trophy->name[0] = CHAR_TRP_BRONZE;
			break;

		case 3:
			trophy->name[0] = CHAR_TRP_SILVER;
			break;

		case 2:
			trophy->name[0] = CHAR_TRP_GOLD;
			break;

		case 1:
			trophy->name[0] = CHAR_TRP_PLATINUM;
			break;

		default:
			break;
		}

		trop_count = sqlite3_column_int(res, 6);
		trophy->file = malloc(sizeof(trop_count));
		memcpy(trophy->file, &trop_count, sizeof(trop_count));

		if (!sqlite3_column_int(res, 5))
			trophy->name[1] = CHAR_TAG_LOCKED;

		// if trophy has been synced, we can't allow changes
		if (0)
			trophy->name[1] = CHAR_TRP_SYNC;
		else
			trophy->type = (sqlite3_column_int(res, 5) ? PATCH_TROP_LOCK : PATCH_TROP_UNLOCK);

		LOG("Trophy=%d [%d] '%s' (%s)", trop_count, trophy->type, trophy->name, trophy->codes);
		list_append(game->codes, trophy);
	}

	sqlite3_finalize(res);
	sqlite3_close(db);

	return list_count(game->codes);
}

/*
 * Function:		ReadOnlineSaves()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Downloads an entire NCL file into an array of code_entry
 * Arguments:
 *	filename:		File name ncl
 *	_count_count:	Pointer to int (set to the number of codes within the ncl)
 * Return:			Returns an array of code_entry, null if failed to load
 */
int ReadOnlineSaves(save_entry_t * game)
{
	code_entry_t* item;
	char path[256];
	snprintf(path, sizeof(path), APOLLO_LOCAL_CACHE "%s.txt", game->title_id);

	if (file_exists(path) == SUCCESS)
	{
		struct stat stats;
		stat(path, &stats);
		// re-download if file is +1 day old
		if ((stats.st_mtime + ONLINE_CACHE_TIMEOUT) < time(NULL))
			http_download(game->path, "saves.txt", path, 0);
	}
	else
	{
		if (!http_download(game->path, "saves.txt", path, 0))
			return -1;
	}

	long fsize;
	char *data = readTextFile(path, &fsize);
	
	char *ptr = data;
	char *end = data + fsize;

	game->codes = list_alloc();

	while (ptr < end && *ptr)
	{
		const char* content = ptr;

		while (ptr < end && *ptr != '\n' && *ptr != '\r')
		{
			ptr++;
		}
		*ptr++ = 0;

		if (content[12] == '=')
		{
			snprintf(path, sizeof(path), CHAR_ICON_ZIP " %s", content + 13);
			item = _createCmdCode(PATCH_COMMAND, path, CMD_CODE_NULL);
			asprintf(&item->file, "%.12s", content);

			item->options_count = 1;
			item->options = _createOptions(3, "Download to USB", CMD_DOWNLOAD_USB);
			asprintf(&item->options->name[2], "Download to HDD");
			asprintf(&item->options->value[2], "%c%c", CMD_DOWNLOAD_USB, STORAGE_HDD);
			list_append(game->codes, item);

			LOG("[%s%s] %s", game->path, item->file, item->name + 1);
		}

		if (ptr < end && *ptr == '\r')
		{
			ptr++;
		}
		if (ptr < end && *ptr == '\n')
		{
			ptr++;
		}
	}

	if (data) free(data);
	LOG("Loaded %d saves", list_count(game->codes));

	return (list_count(game->codes));
}

list_t * ReadBackupList(const char* userPath)
{
	char tmp[32];
	save_entry_t * item;
	code_entry_t * cmd;
	list_t *list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_ZIP, CHAR_ICON_ZIP " Extract Archives (RAR, Zip, 7z)");
	item->path = strdup("/data/");
	item->type = FILE_TYPE_ZIP;
	list_append(list, item);

	item = _createSaveEntry(SAVE_FLAG_PS4, CHAR_ICON_USER " Activate PS4 Accounts");
	asprintf(&item->path, "%s%s", APOLLO_PATH, OWNER_XML_FILE);
	item->type = FILE_TYPE_ACT;
	list_append(list, item);

	item = _createSaveEntry(SAVE_FLAG_PS4, CHAR_ICON_USER " App.db Database Management");
	item->path = strdup(APP_DB_PATH_HDD);
	strrchr(item->path, '/')[1] = 0;
	item->type = FILE_TYPE_SQL;
	list_append(list, item);

	item = _createSaveEntry(SAVE_FLAG_PS4, CHAR_ICON_LOCK " Show Parental Security Passcode");
	item->codes = list_alloc();
	cmd = _createCmdCode(PATCH_NULL, CHAR_ICON_LOCK " Security Passcode: ????????", CMD_CODE_NULL);
	regMgr_GetParentalPasscode(tmp);
	strncpy(cmd->name + 21, tmp, 8);
	list_append(item->codes, cmd);
	list_append(list, item);

/*
	for (int i = 0; i <= MAX_USB_DEVICES; i++)
	{
		snprintf(tmp, sizeof(tmp), USB_PATH, i);

		if (dir_exists(tmp) != SUCCESS)
			continue;

		snprintf(tmp, sizeof(tmp), CHAR_ICON_COPY " Import Licenses (USB %d)", i);
		item = _createSaveEntry(SAVE_FLAG_PS3, tmp);
		asprintf(&item->path, IMPORT_RAP_PATH_USB, i);
		item->type = FILE_TYPE_RAP;
		list_append(list, item);
	}
*/

	return list;
}

int ReadBackupCodes(save_entry_t * bup)
{
	code_entry_t * cmd;
	char tmp[256];

	switch(bup->type)
	{
	case FILE_TYPE_ZIP:
		break;

	case FILE_TYPE_SQL:
		bup->codes = list_alloc();

		cmd = _createCmdCode(PATCH_COMMAND, "\x18 Rebuild App.db Database (Restore missing XMB items)", CMD_DB_REBUILD);
		asprintf(&cmd->file, "%sapp.db", bup->path);
		list_append(bup->codes, cmd);

		cmd = _createCmdCode(PATCH_COMMAND, "\x18 Rebuild DLC Database (addcont.db)", CMD_DB_DLC_REBUILD);
		asprintf(&cmd->file, "%saddcont.db", bup->path);
		list_append(bup->codes, cmd);

		cmd = _createCmdCode(PATCH_COMMAND, "\x18 Restore Delete option to XMB items (app.db fix)", CMD_DB_DEL_FIX);
		asprintf(&cmd->file, "%sapp.db", bup->path);
		list_append(bup->codes, cmd);

		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Backup System Database Folder", CMD_EXP_DATABASE);
		list_append(bup->codes, cmd);

		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Restore System Database Backup", CMD_CODE_NULL);
		cmd->options_count = 1;
		cmd->options = _getFileOptions(EXPORT_DB_PATH, "*.zip", CMD_IMP_DATABASE);
		list_append(bup->codes, cmd);

		return list_count(bup->codes);

	case FILE_TYPE_ACT:
		bup->codes = list_alloc();

		LOG("Getting Users...");
		for (int i = 1; i <= 16; i++)
		{
			uint64_t account;
			char userName[0x20];

			regMgr_GetUserName(i, userName);
			if (!userName[0])
				continue;

			regMgr_GetAccountId(i, &account);
			snprintf(tmp, sizeof(tmp), "%c Activate Offline Account %s (%016lx)", account ? CHAR_TAG_LOCKED : CHAR_TAG_OWNER, userName, account);
			cmd = _createCmdCode(account ? PATCH_NULL : PATCH_COMMAND, tmp, CMD_CODE_NULL); //CMD_CREATE_ACT_DAT

			if (!account)
			{
				cmd->options_count = 1;
				cmd->options = calloc(1, sizeof(option_entry_t));
				cmd->options->sel = -1;
				cmd->options->size = get_xml_owners(bup->path, CMD_CREATE_ACT_DAT, &cmd->options->name, &cmd->options->value);
				cmd->file = malloc(1);
				cmd->file[0] = i;
			}
			list_append(bup->codes, cmd);

			LOG("ID %d = '%s' (%lx)", i, userName, account);
		}

		return list_count(bup->codes);

	default:
		return 0;
	}

	bup->codes = list_alloc();

	LOG("Loading files from '%s'...", bup->path);

	DIR *d;
	struct dirent *dir;
	d = opendir(bup->path);

	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if ((dir->d_type != DT_REG) ||
				(!endsWith(dir->d_name, ".RAR") && !endsWith(dir->d_name, ".ZIP") && !endsWith(dir->d_name, ".7Z")))
				continue;

			snprintf(tmp, sizeof(tmp), CHAR_ICON_ZIP " Extract %s", dir->d_name);
			cmd = _createCmdCode(PATCH_COMMAND, tmp, CMD_EXTRACT_ARCHIVE);
			asprintf(&cmd->file, "%s%s", bup->path, dir->d_name);

			LOG("[%s] name '%s'", cmd->file, cmd->name);
			list_append(bup->codes, cmd);
		}
		closedir(d);
	}

	if (!list_count(bup->codes))
	{
		list_free(bup->codes);
		bup->codes = NULL;
		return 0;
	}

	LOG("%zu items loaded", list_count(bup->codes));

	return list_count(bup->codes);
}

/*
 * Function:		UnloadGameList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Free entire array of game_entry
 * Arguments:
 *	list:			Array of game_entry to free
 *	count:			number of game entries
 * Return:			void
 */
void UnloadGameList(list_t * list)
{
	list_node_t *node, *nc;
	save_entry_t *item;
	code_entry_t *code;

	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		if (item->name)
		{
			free(item->name);
			item->name = NULL;
		}

		if (item->path)
		{
			free(item->path);
			item->path = NULL;
		}

		if (item->dir_name)
		{
			free(item->dir_name);
			item->dir_name = NULL;
		}

		if (item->title_id)
		{
			free(item->title_id);
			item->title_id = NULL;
		}
		
		if (item->codes)
		{
			for (nc = list_head(item->codes); (code = list_get(nc)); nc = list_next(nc))
			{
				if (code->codes)
				{
					free (code->codes);
					code->codes = NULL;
				}
				if (code->name)
				{
					free (code->name);
					code->name = NULL;
				}
				if (code->options && code->options_count > 0)
				{
					for (int z = 0; z < code->options_count; z++)
					{
						if (code->options[z].line)
							free(code->options[z].line);
						if (code->options[z].name)
							free(code->options[z].name);
						if (code->options[z].value)
							free(code->options[z].value);
					}
					
					free (code->options);
				}
			}
			
			list_free(item->codes);
			item->codes = NULL;
		}
	}

	list_free(list);
	
	LOG("UnloadGameList() :: Successfully unloaded game list");
}

int sortCodeList_Compare(const void* a, const void* b)
{
	return strcasecmp(((code_entry_t*) a)->name, ((code_entry_t*) b)->name);
}

/*
 * Function:		qsortSaveList_Compare()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Compares two game_entry for QuickSort
 * Arguments:
 *	a:				First code
 *	b:				Second code
 * Return:			1 if greater, -1 if less, or 0 if equal
 */
int sortSaveList_Compare(const void* a, const void* b)
{
	return strcasecmp(((save_entry_t*) a)->name, ((save_entry_t*) b)->name);
}

int sortSaveList_Compare_TitleID(const void* a, const void* b)
{
	char* ta = ((save_entry_t*) a)->title_id;
	char* tb = ((save_entry_t*) b)->title_id;

	if (!ta)
		return (-1);

	if (!tb)
		return (1);

	return strcasecmp(ta, tb);
}

static void read_usb_encrypted_saves(const char* userPath, list_t *list, uint64_t account)
{
	DIR *d, *d2;
	struct dirent *dir, *dir2;
	save_entry_t *item;
	char savePath[256];

	d = opendir(userPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (dir->d_type != DT_DIR || strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
			continue;

		snprintf(savePath, sizeof(savePath), "%s%s", userPath, dir->d_name);
		d2 = opendir(savePath);

		if (!d2)
			continue;

		LOG("Reading %s...", savePath);

		while ((dir2 = readdir(d2)) != NULL)
		{
			if (dir2->d_type != DT_REG || endsWith(dir2->d_name, ".bin"))
				continue;

			snprintf(savePath, sizeof(savePath), "%s%s/%s.bin", userPath, dir->d_name, dir2->d_name);
			if (file_exists(savePath) != SUCCESS)
				continue;

			snprintf(savePath, sizeof(savePath), "(Encrypted) %s/%s", dir->d_name, dir2->d_name);
			item = _createSaveEntry(SAVE_FLAG_PS4 | SAVE_FLAG_LOCKED, savePath);
			item->type = FILE_TYPE_PS4;

			asprintf(&item->path, "%s%s/", userPath, dir->d_name);
			asprintf(&item->title_id, "%.9s", dir->d_name);
			item->dir_name = strdup(dir2->d_name);

			if (apollo_config.account_id == account)
				item->flags |= SAVE_FLAG_OWNER;

			snprintf(savePath, sizeof(savePath), "%s%s/%s", userPath, dir->d_name, dir2->d_name);
			
			uint64_t size = 0;
			get_file_size(savePath, &size);
			item->blocks = size / ORBIS_SAVE_DATA_BLOCK_SIZE;

			LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
			list_append(list, item);

		}
		closedir(d2);
	}

	closedir(d);
}

static void read_usb_encrypted_savegames(const char* userPath, list_t *list)
{
	DIR *d;
	struct dirent *dir;
	char accPath[256];
	uint64_t acc_id;

	d = opendir(userPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (dir->d_type != DT_DIR || strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
			continue;

		sscanf(dir->d_name, "%lx", &acc_id);
		snprintf(accPath, sizeof(accPath), "%s%s/", userPath, dir->d_name);
		read_usb_encrypted_saves(accPath, list, acc_id);
	}

	closedir(d);
}

static void read_usb_savegames(const char* userPath, list_t *list)
{
	DIR *d;
	struct dirent *dir;
	save_entry_t *item;
	char sfoPath[256];

	d = opendir(userPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (dir->d_type != DT_DIR || strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0)
			continue;

		snprintf(sfoPath, sizeof(sfoPath), "%s%s/sce_sys/param.sfo", userPath, dir->d_name);
		if (file_exists(sfoPath) != SUCCESS)
			continue;

		LOG("Reading %s...", sfoPath);

		sfo_context_t* sfo = sfo_alloc();
		if (sfo_read(sfo, sfoPath) < 0) {
			LOG("Unable to read from '%s'", sfoPath);
			sfo_free(sfo);
			continue;
		}

		char *sfo_data = (char*) sfo_get_param_value(sfo, "MAINTITLE");
		item = _createSaveEntry(SAVE_FLAG_PS4, sfo_data);
		item->type = FILE_TYPE_PS4;

		sfo_data = (char*) sfo_get_param_value(sfo, "TITLE_ID");
		asprintf(&item->path, "%s%s/", userPath, dir->d_name);
		asprintf(&item->title_id, "%.9s", sfo_data);

		sfo_data = (char*) sfo_get_param_value(sfo, "SAVEDATA_DIRECTORY");
		item->dir_name = strdup(sfo_data);

		uint64_t* int_data = (uint64_t*) sfo_get_param_value(sfo, "ACCOUNT_ID");
		if (int_data && (apollo_config.account_id == *int_data))
			item->flags |= SAVE_FLAG_OWNER;

		int_data = (uint64_t*) sfo_get_param_value(sfo, "SAVEDATA_BLOCKS");
		item->blocks = (*int_data);

		sfo_free(sfo);
			
		LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	closedir(d);
}

static void read_hdd_savegames(const char* userPath, list_t *list, sqlite3 *appdb)
{
	save_entry_t *item;
	sqlite3_stmt *res;
	char name[ORBIS_SAVE_DATA_DETAIL_MAXSIZE];
	sqlite3 *db = open_sqlite_db(userPath);

	if (!db)
		return;

	int rc = sqlite3_prepare_v2(db, "SELECT title_id, dir_name, main_title, blocks, account_id, sub_title FROM savedata", -1, &res, NULL);
	if (rc != SQLITE_OK)
	{
		LOG("Failed to fetch data: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	while (sqlite3_step(res) == SQLITE_ROW)
	{
		const char* subtitle = (const char*) sqlite3_column_text(res, 5);
		strncpy(name, (const char*) sqlite3_column_text(res, 2), ORBIS_SAVE_DATA_DETAIL_MAXSIZE);
		get_appdb_title(appdb, (const char*) sqlite3_column_text(res, 0), name);
		strcat(name, " - ");
		strcat(name, subtitle[0] ? subtitle : (const char*) sqlite3_column_text(res, 1));

		item = _createSaveEntry(SAVE_FLAG_PS4 | SAVE_FLAG_HDD, name);
		item->type = FILE_TYPE_PS4;
		item->path = strdup(userPath);
		item->dir_name = strdup((const char*) sqlite3_column_text(res, 1));
		item->title_id = strdup((const char*) sqlite3_column_text(res, 0));
		item->blocks = sqlite3_column_int(res, 3);
		item->flags |= (apollo_config.account_id == (uint64_t)sqlite3_column_int64(res, 4) ? SAVE_FLAG_OWNER : 0);

		LOG("[%s] F(%X) {%d} '%s'", item->title_id, item->flags, item->blocks, item->name);
		list_append(list, item);
	}

	sqlite3_finalize(res);
	sqlite3_close(db);
}

/*
 * Function:		ReadUserList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Reads the entire userlist folder into a game_entry array
 * Arguments:
 *	gmc:			Set as the number of games read
 * Return:			Pointer to array of game_entry, null if failed
 */
list_t * ReadUsbList(const char* userPath)
{
	save_entry_t *item;
	code_entry_t *cmd;
	list_t *list;
	char pathEnc[64], pathDec[64];

	snprintf(pathDec, sizeof(pathDec), "%sAPOLLO/", userPath);
	snprintf(pathEnc, sizeof(pathEnc), "%sSAVEDATA/", userPath);
	if (dir_exists(pathDec) != SUCCESS && dir_exists(pathEnc) != SUCCESS)
		return NULL;

	list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_PS4, CHAR_ICON_COPY " Bulk Save Management");
	item->type = FILE_TYPE_MENU;
	item->codes = list_alloc();
	item->path = strdup(userPath);
	//bulk management hack
	item->dir_name = malloc(sizeof(void**));
	((void**)item->dir_name)[0] = list;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Resign selected Saves", CMD_RESIGN_SAVES);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Resign all decrypted Saves", CMD_RESIGN_ALL_SAVES);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy selected Saves to HDD", CMD_COPY_SAVES_HDD);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy all decrypted Saves to HDD", CMD_COPY_ALL_SAVES_HDD);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Start local Web Server", CMD_RUN_WEBSERVER);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Dump all decrypted Save Fingerprints", CMD_DUMP_FINGERPRINTS);
	list_append(item->codes, cmd);
	list_append(list, item);

	read_usb_savegames(pathDec, list);
	read_usb_encrypted_savegames(pathEnc, list);

	return list;
}

list_t * ReadUserList(const char* userPath)
{
	save_entry_t *item;
	code_entry_t *cmd;
	list_t *list;
	sqlite3* appdb;

	if (file_exists(userPath) != SUCCESS)
		return NULL;

	list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_PS4, CHAR_ICON_COPY " Bulk Save Management");
	item->type = FILE_TYPE_MENU;
	item->codes = list_alloc();
	item->path = strdup(userPath);
	//bulk management hack
	item->dir_name = malloc(sizeof(void**));
	((void**)item->dir_name)[0] = list;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy selected Saves to USB", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(2, "Copy Saves to USB", CMD_COPY_SAVES_USB);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy all Saves to USB", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(2, "Copy Saves to USB", CMD_COPY_ALL_SAVES_USB);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Start local Web Server", CMD_RUN_WEBSERVER);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Dump all Save Fingerprints", CMD_DUMP_FINGERPRINTS);
	list_append(item->codes, cmd);
	list_append(list, item);

	appdb = open_sqlite_db(APP_DB_PATH_HDD);
	read_hdd_savegames(userPath, list, appdb);
	sqlite3_close(appdb);

	return list;
}

/*
 * Function:		ReadOnlineList()
 * File:			saves.c
 * Project:			Apollo PS3
 * Description:		Downloads the entire gamelist file into a game_entry array
 * Arguments:
 *	gmc:			Set as the number of games read
 * Return:			Pointer to array of game_entry, null if failed
 */
static void _ReadOnlineListEx(const char* urlPath, uint16_t flag, list_t *list)
{
	save_entry_t *item;
	char path[256];

	snprintf(path, sizeof(path), APOLLO_LOCAL_CACHE "%04X_games.txt", flag);

	if (file_exists(path) == SUCCESS)
	{
		struct stat stats;
		stat(path, &stats);
		// re-download if file is +1 day old
		if ((stats.st_mtime + ONLINE_CACHE_TIMEOUT) < time(NULL))
			http_download(urlPath, "games.txt", path, 0);
	}
	else
	{
		if (!http_download(urlPath, "games.txt", path, 0))
			return;
	}
	
	long fsize;
	char *data = readTextFile(path, &fsize);
	
	char *ptr = data;
	char *end = data + fsize;

	while (ptr < end && *ptr)
	{
		char *tmp, *content = ptr;

		while (ptr < end && *ptr != '\n' && *ptr != '\r')
		{
			ptr++;
		}
		*ptr++ = 0;

		if ((tmp = strchr(content, '=')) != NULL)
		{
			*tmp++ = 0;
			item = _createSaveEntry(flag | SAVE_FLAG_ONLINE, tmp);
			item->title_id = strdup(content);
			asprintf(&item->path, "%s%s/", urlPath, item->title_id);

			LOG("+ [%s] %s", item->title_id, item->name);
			list_append(list, item);
		}

		if (ptr < end && *ptr == '\r')
		{
			ptr++;
		}
		if (ptr < end && *ptr == '\n')
		{
			ptr++;
		}
	}

	if (data) free(data);
}

list_t * ReadOnlineList(const char* urlPath)
{
	char url[256];
	list_t *list = list_alloc();

	// PS4 save-games (Zip folder)
	snprintf(url, sizeof(url), "%s" "PS4/", urlPath);
	_ReadOnlineListEx(url, SAVE_FLAG_PS4, list);

/*
	// PS2 save-games (Zip PSV)
	snprintf(url, sizeof(url), "%s" "PS2/", urlPath);
	_ReadOnlineListEx(url, SAVE_FLAG_PS2, list);

	// PS1 save-games (Zip PSV)
	//snprintf(url, sizeof(url), "%s" "PS1/", urlPath);
	//_ReadOnlineListEx(url, SAVE_FLAG_PS1, list);
*/

	if (!list_count(list))
	{
		list_free(list);
		return NULL;
	}

	return list;
}

list_t * ReadTrophyList(const char* userPath)
{
	save_entry_t *item;
//	code_entry_t *cmd;
	list_t *list;
	sqlite3 *db;
	sqlite3_stmt *res;

	if ((db = open_sqlite_db(userPath)) == NULL)
		return NULL;

	list = list_alloc();
/*
	item = _createSaveEntry(SAVE_FLAG_PS4, CHAR_ICON_COPY " Export Trophies");
	item->type = FILE_TYPE_MENU;
	item->path = strdup(userPath);
	item->codes = list_alloc();
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Backup Trophies to USB", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(2, "Save Trophies to USB", CMD_COPY_TROPHIES_USB);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_ZIP " Export Trophies to .Zip", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(2, "Save .Zip to USB", CMD_ZIP_TROPHY_USB);
	list_append(item->codes, cmd);
	list_append(list, item);
*/
	int rc = sqlite3_prepare_v2(db, "SELECT id, trophy_title_id, title FROM tbl_trophy_title WHERE status = 0", -1, &res, NULL);
	if (rc != SQLITE_OK)
	{
		LOG("Failed to fetch data: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL;
	}

	while (sqlite3_step(res) == SQLITE_ROW)
	{
		item = _createSaveEntry(SAVE_FLAG_PS4 | SAVE_FLAG_TROPHY, (const char*) sqlite3_column_text(res, 2));
		item->blocks = sqlite3_column_int(res, 0);
		item->path = strdup(userPath);
		item->title_id = strdup((const char*) sqlite3_column_text(res, 1));
		item->type = FILE_TYPE_TRP;

		LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	sqlite3_finalize(res);
	sqlite3_close(db);

	return list;
}

int get_save_details(const save_entry_t* save, char **details)
{
	char sfoPath[256];
	sqlite3 *db;
	sqlite3_stmt *res;


	if (!(save->flags & SAVE_FLAG_PS4))
	{
		asprintf(details, "%s\n\nTitle: %s\n", save->path, save->name);
		return 1;
	}

	if (save->flags & SAVE_FLAG_TROPHY)
	{
		if ((db = open_sqlite_db(save->path)) == NULL)
			return 0;

		char* query = sqlite3_mprintf("SELECT id, description, trophy_num, unlocked_trophy_num, progress,"
			"platinum_num, unlocked_platinum_num, gold_num, unlocked_gold_num, silver_num, unlocked_silver_num,"
			"bronze_num, unlocked_bronze_num FROM tbl_trophy_title WHERE id = %d", save->blocks);

		if (sqlite3_prepare_v2(db, query, -1, &res, NULL) != SQLITE_OK || sqlite3_step(res) != SQLITE_ROW)
		{
			LOG("Failed to fetch data: %s", sqlite3_errmsg(db));
			sqlite3_free(query);
			sqlite3_close(db);
			return 0;
		}

		asprintf(details, "Trophy-Set Details\n\n"
			"Title: %s\n"
			"Description: %s\n"
			"NP Comm ID: %s\n"
			"Progress: %d/%d - %d%%\n"
			"%c Platinum: %d/%d\n"
			"%c Gold: %d/%d\n"
			"%c Silver: %d/%d\n"
			"%c Bronze: %d/%d\n",
			save->name, sqlite3_column_text(res, 1), save->title_id,
			sqlite3_column_int(res, 3), sqlite3_column_int(res, 2), sqlite3_column_int(res, 4),
			CHAR_TRP_PLATINUM, sqlite3_column_int(res, 6), sqlite3_column_int(res, 5),
			CHAR_TRP_GOLD, sqlite3_column_int(res, 8), sqlite3_column_int(res, 7),
			CHAR_TRP_SILVER, sqlite3_column_int(res, 10), sqlite3_column_int(res, 9),
			CHAR_TRP_BRONZE, sqlite3_column_int(res, 12), sqlite3_column_int(res, 11));

		sqlite3_free(query);
		sqlite3_finalize(res);
		sqlite3_close(db);

		return 1;
	}

	if(save->flags & SAVE_FLAG_LOCKED)
	{
		asprintf(details, "%s\n\n"
			"Title ID: %s\n"
			"Dir Name: %s\n"
			"Blocks: %d\n"
			"Account ID: %.16s\n",
			save->path,
			save->title_id,
			save->dir_name,
			save->blocks,
			save->path + 23);

		return 1;
	}

	if(save->flags & SAVE_FLAG_HDD)
	{
		if ((db = open_sqlite_db(save->path)) == NULL)
			return 0;

		char* query = sqlite3_mprintf("SELECT sub_title, detail, free_blocks, size_kib, user_id, account_id, main_title FROM savedata "
			" WHERE title_id = %Q AND dir_name = %Q", save->title_id, save->dir_name);

		if (sqlite3_prepare_v2(db, query, -1, &res, NULL) != SQLITE_OK || sqlite3_step(res) != SQLITE_ROW)
		{
			LOG("Failed to fetch data: %s", sqlite3_errmsg(db));
			sqlite3_free(query);
			sqlite3_close(db);
			return 0;
		}

		asprintf(details, "%s\n\n"
			"Title: %s\n"
			"Subtitle: %s\n"
			"Detail: %s\n"
			"Dir Name: %s\n"
			"Blocks: %d (%d Free)\n"
			"Size: %d Kb\n"
			"User ID: %08x\n"
			"Account ID: %016llx\n",
			save->path,
			sqlite3_column_text(res, 6),
			sqlite3_column_text(res, 0),
			sqlite3_column_text(res, 1),
			save->dir_name,
			save->blocks, sqlite3_column_int(res, 2), 
			sqlite3_column_int(res, 3),
			sqlite3_column_int(res, 4),
			sqlite3_column_int64(res, 5));

		sqlite3_free(query);
		sqlite3_finalize(res);
		sqlite3_close(db);

		return 1;
	}

	snprintf(sfoPath, sizeof(sfoPath), "%s" "sce_sys/param.sfo", save->path);
	LOG("Save Details :: Reading %s...", sfoPath);

	sfo_context_t* sfo = sfo_alloc();
	if (sfo_read(sfo, sfoPath) < 0) {
		LOG("Unable to read from '%s'", sfoPath);
		sfo_free(sfo);
		return 0;
	}

	char* subtitle = (char*) sfo_get_param_value(sfo, "SUBTITLE");
	char* detail = (char*) sfo_get_param_value(sfo, "DETAIL");
	uint64_t* account_id = (uint64_t*) sfo_get_param_value(sfo, "ACCOUNT_ID");
	sfo_params_ids_t* param_ids = (sfo_params_ids_t*) sfo_get_param_value(sfo, "PARAMS");

	asprintf(details, "%s\n\n"
		"Title: %s\n"
		"Subtitle: %s\n"
		"Detail: %s\n"
		"Dir Name: %s\n"
		"Blocks: %d\n"
		"User ID: %08x\n"
		"Account ID: %016lx\n",
		save->path, save->name,
		subtitle,
		detail,
		save->dir_name,
		save->blocks,
		param_ids->user_id,
		*account_id);

	sfo_free(sfo);
	return 1;
}
