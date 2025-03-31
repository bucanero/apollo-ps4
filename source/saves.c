#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <orbis/SaveData.h>
#include <sqlite3.h>

#include "saves.h"
#include "common.h"
#include "sfo.h"
#include "settings.h"
#include "util.h"
#include "ps2mc.h"
#include "mcio.h"
#include "ps1card.h"
#include "sd.h"

#define UTF8_CHAR_STAR		"\xE2\x98\x85"

#define CHAR_ICON_NET		"\x09"
#define CHAR_ICON_ZIP		"\x0C"
#define CHAR_ICON_VMC		"\x0B"
#define CHAR_ICON_COPY		"\x0E"
#define CHAR_ICON_SIGN		"\x06"
#define CHAR_ICON_USER		"\x07"
#define CHAR_ICON_LOCK		"\x08"
#define CHAR_ICON_WARN		"\x0F"

int sqlite3_memvfs_init(const char* vfsName);
int sqlite3_memvfs_dump(sqlite3 *db, const char *zSchema, const char *zFilename);

static sqlite3* open_sqlite_db(const char* db_path)
{
	uint8_t* db_buf;
	size_t db_size;
	sqlite3 *db;

	if (sqlite3_memvfs_init("orbis_rw") != SQLITE_OK_LOAD_PERMANENTLY)
	{
		LOG("Error loading extension: %s", "memvfs");
		return NULL;
	}

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
	sfo_context_t* sfo;
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
			sfo = sfo_alloc();
			if (sfo_read(sfo, path) < 0)
			{
				LOG("Unable to read from '%s'", path);
				sfo_free(sfo);
				continue;
			}

			LOG("Adding %s to addcont...", dirp2->d_name);
			query = sqlite3_mprintf("INSERT OR IGNORE INTO addcont(title_id, dir_name, content_id, title, version, attribute, status) "
				"VALUES(%Q, %Q, %Q, %Q, 536870912, '01.00', 0)",
				dirp->d_name, dirp2->d_name, sfo_get_param_value(sfo, "CONTENT_ID"), sfo_get_param_value(sfo, "TITLE"));

			if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK)
				LOG("addcont insert failed: %s", sqlite3_errmsg(db));

			sqlite3_free(query);
			sfo_free(sfo);
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
		sqlite3_close(db);
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

static const char* trophy_type_field(int type)
{
	switch (type)
	{
	case CHAR_TRP_PLATINUM:
		return "unlocked_platinum_num";

	case CHAR_TRP_GOLD:
		return "unlocked_gold_num";

	case CHAR_TRP_SILVER:
		return "unlocked_silver_num";

	case CHAR_TRP_BRONZE:
		return "unlocked_bronze_num";

	default:
		return NULL;
	}
}

int trophy_unlock(const save_entry_t* game, int trp_id, int grp_id, int type)
{
	char dbpath[256];
	sqlite3 *db;
	const char *field = trophy_type_field(type);

	if (!field)
		return 0;

	snprintf(dbpath, sizeof(dbpath), TROPHY_DB_PATH, apollo_config.user_id);
	if ((db = open_sqlite_db(dbpath)) == NULL)
		return 0;

	char* query = sqlite3_mprintf("UPDATE tbl_trophy_flag SET (visible, unlocked, time_unlocked, time_unlocked_uc) ="
		"(1, 1, strftime('%%Y-%%m-%%dT%%H:%%M:%%S.00Z', CURRENT_TIMESTAMP), strftime('%%Y-%%m-%%dT%%H:%%M:%%S.00Z', CURRENT_TIMESTAMP))"
		"WHERE (title_id = %d AND trophyid = %d);\n"
		"UPDATE tbl_trophy_title SET (progress, unlocked_trophy_num, %s) ="
		"(((unlocked_trophy_num+1)*100)/trophy_num, unlocked_trophy_num+1, %s+1) WHERE (id=%d);\n"
		"UPDATE tbl_trophy_group SET (progress, unlocked_trophy_num, %s) ="
		"(((unlocked_trophy_num+1)*100)/trophy_num, unlocked_trophy_num+1, %s+1) WHERE (title_id=%d AND groupid=%d);",
		game->blocks, trp_id,
		field, field, game->blocks,
		field, field, game->blocks, grp_id);

	if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK)
	{
		LOG("Error updating '%s': %s", game->title_id, sqlite3_errmsg(db));
		sqlite3_free(query);
		sqlite3_close(db);
		return 0;
	}

	LOG("Saving database to %s", dbpath);
	sqlite3_memvfs_dump(db, NULL, dbpath);
	sqlite3_free(query);
	sqlite3_close(db);

	return 1;
}

int trophy_lock(const save_entry_t* game, int trp_id, int grp_id, int type)
{
	char dbpath[256];
	sqlite3 *db;
	const char *field = trophy_type_field(type);

	if (!field)
		return 0;

	snprintf(dbpath, sizeof(dbpath), TROPHY_DB_PATH, apollo_config.user_id);
	if ((db = open_sqlite_db(dbpath)) == NULL)
		return 0;

	char* query = sqlite3_mprintf("UPDATE tbl_trophy_flag SET (visible, unlocked, time_unlocked, time_unlocked_uc) ="
		"((~(hidden&1))&(hidden|1), 0, '0001-01-01T00:00:00.00Z', '0001-01-01T00:00:00.00Z') "
		"WHERE (title_id = %d AND trophyid = %d);\n"
		"UPDATE tbl_trophy_title SET (progress, unlocked_trophy_num, %s) ="
		"(((unlocked_trophy_num-1)*100)/trophy_num, unlocked_trophy_num-1, %s-1) WHERE (id=%d);\n"
		"UPDATE tbl_trophy_group SET (progress, unlocked_trophy_num, %s) ="
		"(((unlocked_trophy_num-1)*100)/trophy_num, unlocked_trophy_num-1, %s-1) WHERE (title_id=%d AND groupid=%d);",
		game->blocks, trp_id,
		field, field, game->blocks,
		field, field, game->blocks, grp_id);

	if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK)
	{
		LOG("Error updating '%s': %s", game->title_id, sqlite3_errmsg(db));
		sqlite3_free(query);
		sqlite3_close(db);
		return 0;
	}

	LOG("Saving database to %s", dbpath);
	sqlite3_memvfs_dump(db, NULL, dbpath);
	sqlite3_free(query);
	sqlite3_close(db);

	return 1;
}

int trophySet_delete(const save_entry_t* game)
{
	char dbpath[256];
	sqlite3 *db;

	snprintf(dbpath, sizeof(dbpath), TROPHY_DB_PATH, apollo_config.user_id);
	if ((db = open_sqlite_db(dbpath)) == NULL)
		return 0;

	char* query = sqlite3_mprintf("DELETE FROM tbl_trophy_title WHERE (id=%d);\n"
		"DELETE FROM tbl_trophy_title_entry WHERE (id=%d);\n"
		"DELETE FROM tbl_trophy_flag WHERE (title_id=%d);",
		game->blocks, game->blocks, game->blocks);

	if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK)
	{
		LOG("Error updating '%s': %s", game->title_id, sqlite3_errmsg(db));
		sqlite3_free(query);
		sqlite3_close(db);
		return 0;
	}

	LOG("Saving database to %s", dbpath);
	sqlite3_memvfs_dump(db, NULL, dbpath);
	sqlite3_free(query);
	sqlite3_close(db);

	snprintf(dbpath, sizeof(dbpath), TROPHY_PATH_HDD "%s/sealedkey", apollo_config.user_id, game->title_id);
	unlink_secure(dbpath);

	snprintf(dbpath, sizeof(dbpath), TROPHY_PATH_HDD "%s/trophy.img", apollo_config.user_id, game->title_id);
	unlink_secure(dbpath);

	*strrchr(dbpath, '/') = 0;
	rmdir(dbpath);

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
	char mountDir[256];

	snprintf(mountDir, sizeof(mountDir), APOLLO_SANDBOX_PATH, mountPath);
	int umountErrorCode = umountSave(mountDir, 0, 0);
	
	if (umountErrorCode < 0)
	{
		LOG("UMOUNT_ERROR (%X)", umountErrorCode);
		notify_popup(NULL, "Warning! Save couldn't be unmounted!");
	}

	rmdir(mountDir);
	return (umountErrorCode == SUCCESS);
}

int orbis_SaveMount(const save_entry_t *save, uint32_t mount_mode, char* mount_path)
{
	char mountDir[256];
	char keyPath[256];
	char volumePath[256];

	snprintf(mountDir, sizeof(mountDir), APOLLO_SANDBOX_PATH, save->dir_name);
	if (mkdirs(mountDir) < 0)
	{
		LOG("ERROR: can't create '%s'", mountDir);
		return 0;
	}

	if (mount_mode & SAVE_FLAG_TROPHY)
	{
		snprintf(keyPath, sizeof(keyPath), TROPHY_PATH_HDD "%s/sealedkey", apollo_config.user_id, save->title_id);
		snprintf(volumePath, sizeof(volumePath), TROPHY_PATH_HDD "%s/trophy.img", apollo_config.user_id, save->title_id);
	}
	else if (mount_mode & SAVE_FLAG_LOCKED)
	{
		snprintf(keyPath, sizeof(keyPath), "%s%s.bin", save->path, save->dir_name);
		snprintf(volumePath, sizeof(volumePath), "%s%s", save->path, save->dir_name);
	}
	else
	{
		snprintf(keyPath, sizeof(keyPath), SAVES_PATH_HDD "%s/%s.bin", apollo_config.user_id, save->title_id, save->dir_name);
		snprintf(volumePath, sizeof(volumePath), SAVES_PATH_HDD "%s/sdimg_%s", apollo_config.user_id, save->title_id, save->dir_name);
	}

	if ((mount_mode & ORBIS_SAVE_DATA_MOUNT_MODE_CREATE2) && (file_exists(keyPath) != SUCCESS))
	{
		sqlite3 *db;
		char *query, dbpath[256];

		LOG("Creating save '%s'...", keyPath);
		mkdirs(volumePath);
		if (createSave(volumePath, keyPath, save->blocks) < 0)
		{
			LOG("ERROR: can't create '%s'", keyPath);
			return 0;
		}

		snprintf(dbpath, sizeof(dbpath), SAVES_DB_PATH, apollo_config.user_id);
		if ((db = open_sqlite_db(dbpath)) == NULL)
			return 0;

		query = sqlite3_mprintf("INSERT INTO savedata(title_id, dir_name, main_title, sub_title, detail, tmp_dir_name, is_broken, user_param, blocks, free_blocks, size_kib, mtime, fake_broken, account_id, user_id, faked_owner, cloud_icon_url, cloud_revision, game_title_id) "
			"VALUES (%Q, %Q, '', '', '', '', 0, 0, %d, %d, %d, strftime('%%Y-%%m-%%dT%%H:%%M:%%S.00Z', CURRENT_TIMESTAMP), 0, %ld, %d, 0, '', 0, %Q);",
			save->title_id, save->dir_name, save->blocks, save->blocks, (save->blocks*32), apollo_config.account_id, apollo_config.user_id, save->title_id);

		if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK)
		{
			LOG("Error inserting '%s': %s", save->title_id, sqlite3_errmsg(db));
			sqlite3_free(query);
			sqlite3_close(db);
			return 0;
		}

		LOG("Saving database to %s", dbpath);
		sqlite3_memvfs_dump(db, NULL, dbpath);
		sqlite3_free(query);
		sqlite3_close(db);
	}

	int mountErrorCode = mountSave(volumePath, keyPath, mountDir);
	if (mountErrorCode < 0)
	{
		LOG("ERROR (%X): can't mount '%s/%s'", mountErrorCode, save->title_id, save->dir_name);
		rmdir(mountDir);
		return 0;
	}

	LOG("'%s/%s' mountPath (%s)", save->title_id, save->dir_name, mountDir);
	strlcpy(mount_path, save->dir_name, ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE);

	return 1;
}

int orbis_UpdateSaveParams(const save_entry_t* save, const char* title, const char* subtitle, const char* details, uint32_t userParam)
{
	sqlite3 *db;
	char* query, dbpath[256];

	snprintf(dbpath, sizeof(dbpath), SAVES_DB_PATH, apollo_config.user_id);
	db = open_sqlite_db(dbpath);
	if (!db)
		return 0;

	LOG("Updating %s ...", save->title_id);
	query = sqlite3_mprintf("UPDATE savedata SET (main_title, sub_title, detail, user_param)="
		"(%Q, %Q, %Q, %ld) WHERE (title_id=%Q AND dir_name=%Q);",
		title, subtitle, details, userParam, save->title_id, save->dir_name);

	if (sqlite3_exec(db, query, NULL, NULL, NULL) != SQLITE_OK)
	{
		LOG("Failed to execute query: %s", sqlite3_errmsg(db));
		sqlite3_free(query);
		sqlite3_close(db);
		return 0;
	}

	LOG("Saving database to %s", dbpath);
	sqlite3_memvfs_dump(db, NULL, dbpath);
	sqlite3_free(query);
	sqlite3_close(db);

	return 1;
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
	entry->name = name ? strdup(name) : NULL;
	asprintf(&entry->codes, "%c", code);

	return entry;
}

static option_entry_t* _initOptions(int count)
{
	option_entry_t* options = (option_entry_t*)calloc(count, sizeof(option_entry_t));

	for(int i = 0; i < count; i++)
	{
		options[i].sel = -1;
		options[i].opts = list_alloc();
	}

	return options;
}

static void _createOptions(code_entry_t* code, const char* name, char value)
{
	option_value_t* optval;

	code->options_count = 1;
	code->options = _initOptions(1);

	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "%s %d", name, 0);
	asprintf(&optval->value, "%c%c", value, STORAGE_USB0);
	list_append(code->options[0].opts, optval);

	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "%s %d", name, 1);
	asprintf(&optval->value, "%c%c", value, STORAGE_USB1);
	list_append(code->options[0].opts, optval);

	return;
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
	option_value_t* optval;
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
	opt = _initOptions(1);

	for (node = list_head(file_list); (filename = list_get(node)); node = list_next(node))
	{
		LOG("Adding '%s' (%s)", filename, mask);
		optval = malloc(sizeof(option_value_t));
		optval->name = filename;

		if (is_cmd)
			asprintf(&optval->value, "%c", is_cmd);
		else
			asprintf(&optval->value, "%s", mask);

		list_append(opt[0].opts, optval);
	}

	list_free(file_list);

	return opt;
}

static void _addBackupCommands(save_entry_t* item)
{
	code_entry_t* cmd;
	option_value_t* optval;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Apply Changes & Resign", CMD_RESIGN_SAVE);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_WARN " Delete Save Game", CMD_DELETE_SAVE);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " File Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy save game", CMD_CODE_NULL);
	_createOptions(cmd, "Copy Save to USB", CMD_COPY_SAVE_USB);
	if (!(item->flags & SAVE_FLAG_HDD))
	{
		optval = malloc(sizeof(option_value_t));
		asprintf(&optval->name, "Copy Save to HDD");
		asprintf(&optval->value, "%c", (item->flags & SAVE_FLAG_LOCKED) ? CMD_COPY_PFS : CMD_COPY_SAVE_HDD);
		list_append(cmd->options[0].opts, optval);
	}
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_ZIP " Export save game to Zip", CMD_CODE_NULL);
	_createOptions(cmd, "Export Zip to USB", CMD_EXPORT_ZIP_USB);
	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "Export Zip to HDD");
	asprintf(&optval->value, "%c", CMD_EXPORT_ZIP_HDD);
	list_append(cmd->options[0].opts, optval);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export decrypted save files", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _getFileOptions(item->path, "*", CMD_DECRYPT_FILE);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Import decrypted save files", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _getFileOptions(item->path, "*", CMD_IMPORT_DATA_FILE);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Hex Edit save game files", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _getFileOptions(item->path, "*", CMD_HEX_EDIT_FILE);
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

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Show Keystone Fingerprint", CMD_EXP_FINGERPRINT);
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
	uint8_t data[16];
	char savePath[256];
	code_entry_t* cmd;
	item->codes = list_alloc();

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(item->codes, cmd);

	snprintf(savePath, sizeof(savePath), "%s%s.bin", item->path, item->dir_name);
	read_file(savePath, data, sizeof(data));
	snprintf(savePath, sizeof(savePath), CHAR_ICON_WARN " --- Encrypted save requires firmware %s --- " CHAR_ICON_WARN, get_fw_by_pfskey_ver(data[8]));

	cmd = _createCmdCode(PATCH_NULL, savePath, CMD_CODE_NULL);
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
	char mount[ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE];
	char *tmp = NULL;

	if (save->type == FILE_TYPE_NULL)
		return set_pfs_codes(save);

	save->codes = list_alloc();

	if (save->flags & (SAVE_FLAG_HDD|SAVE_FLAG_LOCKED))
	{
		if (!orbis_SaveMount(save, (save->flags & SAVE_FLAG_LOCKED), mount))
		{
			code = _createCmdCode(PATCH_NULL, CHAR_ICON_WARN " --- Error Mounting Save! --- " CHAR_ICON_WARN, CMD_CODE_NULL);
			list_append(save->codes, code);
			return list_count(save->codes);
		}
		tmp = save->path;
		asprintf(&save->path, APOLLO_SANDBOX_PATH, mount);
	}

	_addBackupCommands(save);
	_addSfoCommands(save);

	snprintf(filePath, sizeof(filePath), APOLLO_DATA_PATH "%s.savepatch", save->title_id);
	if ((buffer = readTextFile(filePath, NULL)) == NULL)
		goto skip_end;

	code = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Cheats " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);	
	list_append(save->codes, code);

	code = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Raw Patch File", CMD_VIEW_RAW_PATCH);
	list_append(save->codes, code);

	LOG("Loading BSD codes '%s'...", filePath);
	load_patch_code_list(buffer, save->codes, &get_file_entries, save->path);
	free (buffer);

	LOG("Loaded %zu codes", list_count(save->codes));

skip_end:
	if (tmp)
	{
		orbis_SaveUmount(mount);
		free(save->path);
		save->path = tmp;
	}

	return list_count(save->codes);
}

int ReadTrophies(save_entry_t * game)
{
	int *trop_id;
	code_entry_t * trophy;
	option_value_t* optval;
	char query[256];
	char mount[ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE];
	char *tmp;
	sqlite3 *db;
	sqlite3_stmt *res;

	if ((db = open_sqlite_db(game->path)) == NULL)
		return 0;

	game->codes = list_alloc();

	if (!orbis_SaveMount(game, (game->flags & SAVE_FLAG_TROPHY), mount))
	{
		trophy = _createCmdCode(PATCH_NULL, CHAR_ICON_WARN " --- Error Mounting Trophy Set! --- " CHAR_ICON_WARN, CMD_CODE_NULL);
		list_append(game->codes, trophy);
		return list_count(game->codes);
	}
	tmp = game->path;
	asprintf(&game->path, APOLLO_SANDBOX_PATH, mount);

	trophy = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Apply Changes to Trophy Set", CMD_UPDATE_TROPHY);
	list_append(game->codes, trophy);

	trophy = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Trophy Set Details", CMD_VIEW_DETAILS);
	list_append(game->codes, trophy);

	trophy = _createCmdCode(PATCH_COMMAND, CHAR_ICON_WARN " Delete Trophy Set", CMD_DELETE_SAVE);
	list_append(game->codes, trophy);

	trophy = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " File Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(game->codes, trophy);

	trophy = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Backup Trophy files to USB", CMD_CODE_NULL);
	trophy->file = strdup(game->path);
	_createOptions(trophy, "Copy Trophy to USB", CMD_EXP_TROPHY_USB);
	list_append(game->codes, trophy);

	trophy = _createCmdCode(PATCH_COMMAND, CHAR_ICON_ZIP " Export Trophy files to Zip", CMD_CODE_NULL);
	trophy->file = strdup(game->path);
	_createOptions(trophy, "Save .Zip to USB", CMD_EXPORT_ZIP_USB);
	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "Save .Zip to HDD");
	asprintf(&optval->value, "%c", CMD_EXPORT_ZIP_HDD);
	list_append(trophy->options[0].opts, optval);
	list_append(game->codes, trophy);

	trophy = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export decrypted trophy files", CMD_CODE_NULL);
	trophy->options_count = 1;
	trophy->options = _getFileOptions(game->path, "*", CMD_DECRYPT_FILE);
	list_append(game->codes, trophy);

	trophy = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Import decrypted trophy files", CMD_CODE_NULL);
	trophy->options_count = 1;
	trophy->options = _getFileOptions(game->path, "*", CMD_IMPORT_DATA_FILE);
	list_append(game->codes, trophy);

	orbis_SaveUmount(mount);
	free(game->path);
	game->path = tmp;

	trophy = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Trophies " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(game->codes, trophy);

	snprintf(query, sizeof(query), "SELECT trophyid, groupid, title, description, grade, unlocked FROM tbl_trophy_flag WHERE title_id = %d", game->blocks);

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
		trophy->file = malloc(sizeof(int)*2);
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

		trop_id = (int*)trophy->file;
		trop_id[0] = sqlite3_column_int(res, 0);
		trop_id[1] = sqlite3_column_int(res, 1);

		if (!sqlite3_column_int(res, 5))
			trophy->name[1] = CHAR_TAG_LOCKED;

		// if trophy has been synced, we can't allow changes
		if (0)
			trophy->name[1] = CHAR_TRP_SYNC;
		else
			trophy->type = (sqlite3_column_int(res, 5) ? PATCH_TROP_LOCK : PATCH_TROP_UNLOCK);

		LOG("Trophy=%d [%d] '%s' (%s)", trop_id[0], trophy->type, trophy->name+2, trophy->codes);
		list_append(game->codes, trophy);
	}

	sqlite3_finalize(res);
	sqlite3_close(db);

	return list_count(game->codes);
}

static void add_vmc_import_saves(list_t* list, const char* path, const char* folder)
{
	code_entry_t * cmd;
	DIR *d;
	struct dirent *dir;
	char psvPath[256];

	snprintf(psvPath, sizeof(psvPath), "%s%s", path, folder);
	d = opendir(psvPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (!endsWith(dir->d_name, ".PSV") && !endsWith(dir->d_name, ".MCS") && !endsWith(dir->d_name, ".PSX") &&
			!endsWith(dir->d_name, ".PS1") && !endsWith(dir->d_name, ".MCB") && !endsWith(dir->d_name, ".PDA"))
			continue;

		// check for PS1 PSV saves
		if (endsWith(dir->d_name, ".PSV"))
		{
			snprintf(psvPath, sizeof(psvPath), "%s%s%s", path, folder, dir->d_name);
			if (read_file(psvPath, (uint8_t*) psvPath, 0x40) < 0 || psvPath[0x3C] != 0x01)
				continue;
		}

		snprintf(psvPath, sizeof(psvPath), CHAR_ICON_COPY "%c %s", CHAR_TAG_PS1, dir->d_name);
		cmd = _createCmdCode(PATCH_COMMAND, psvPath, CMD_IMP_VMC1SAVE);
		asprintf(&cmd->file, "%s%s%s", path, folder, dir->d_name);
		cmd->codes[1] = FILE_TYPE_PS1;
		list_append(list, cmd);

		LOG("[%s] F(%X) name '%s'", cmd->file, cmd->flags, cmd->name+2);
	}

	closedir(d);
}

int ReadVmc1Codes(save_entry_t * save)
{
	code_entry_t * cmd;
	option_value_t* optval;

	save->codes = list_alloc();

	if (save->type == FILE_TYPE_MENU)
	{
		add_vmc_import_saves(save->codes, save->path, PS1_SAVES_PATH_USB);
		add_vmc_import_saves(save->codes, save->path, PSV_SAVES_PATH_USB);
		if (!list_count(save->codes))
		{
			list_free(save->codes);
			save->codes = NULL;
			return 0;
		}

		list_bubbleSort(save->codes, &sortCodeList_Compare);

		return list_count(save->codes);
	}

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_WARN " Delete Save Game", CMD_DELETE_SAVE);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Save Game Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save game to .MCS format", CMD_CODE_NULL);
	_createOptions(cmd, "Copy .MCS Save to USB", CMD_EXP_VMC1SAVE);
	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "Copy .MCS Save to HDD");
	asprintf(&optval->value, "%c%c", CMD_EXP_VMC1SAVE, STORAGE_HDD);
	list_append(cmd->options[0].opts, optval);
	cmd->options[0].id = PS1SAVE_MCS;
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save game to .PSV format", CMD_CODE_NULL);
	_createOptions(cmd, "Copy .PSV Save to USB", CMD_EXP_VMC1SAVE);
	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "Copy .PSV Save to HDD");
	asprintf(&optval->value, "%c%c", CMD_EXP_VMC1SAVE, STORAGE_HDD);
	list_append(cmd->options[0].opts, optval);
	cmd->options[0].id = PS1SAVE_PSV;
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save game to .PSX format", CMD_CODE_NULL);
	_createOptions(cmd, "Copy .PSX Save to USB", CMD_EXP_VMC1SAVE);
	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "Copy .PSX Save to HDD");
	asprintf(&optval->value, "%c%c", CMD_EXP_VMC1SAVE, STORAGE_HDD);
	list_append(cmd->options[0].opts, optval);
	cmd->options[0].id = PS1SAVE_AR;
	list_append(save->codes, cmd);

	LOG("Loaded %ld codes", list_count(save->codes));

	return list_count(save->codes);
}

static void add_vmc2_import_saves(list_t* list, const char* path, const char* folder)
{
	code_entry_t * cmd;
	DIR *d;
	struct dirent *dir;
	char psvPath[256];
	char data[64];
	int type, toff;

	snprintf(psvPath, sizeof(psvPath), "%s%s", path, folder);
	d = opendir(psvPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (dir->d_type != DT_REG)
			continue;

		// check for PS2 PSV saves
		if (endsWith(dir->d_name, ".PSV"))
		{
			snprintf(psvPath, sizeof(psvPath), "%s%s%s", path, folder, dir->d_name);
			if (read_file(psvPath, (uint8_t*) psvPath, 0x40) < 0 || psvPath[0x3C] != 0x02)
				continue;

			toff = 0x80;
			type = FILE_TYPE_PSV;
		}
		else if (endsWith(dir->d_name, ".PSU"))
		{
			toff = 0x40;
			type = FILE_TYPE_PSU;
		}
		else if (endsWith(dir->d_name, ".CBS"))
		{
			toff = 0x14;
			type = FILE_TYPE_CBS;
		}
		else if (endsWith(dir->d_name, ".XPS") || endsWith(dir->d_name, ".SPS"))
		{
			toff = 0x15;
			type = FILE_TYPE_XPS;
		}
		else if (endsWith(dir->d_name, ".MAX"))
		{
			toff = 0x10;
			type = FILE_TYPE_MAX;
		}
		else continue;

		snprintf(psvPath, sizeof(psvPath), "%s%s%s", path, folder, dir->d_name);
		LOG("Reading %s...", psvPath);

		FILE *fp = fopen(psvPath, "rb");
		if (!fp) {
			LOG("Unable to open '%s'", psvPath);
			continue;
		}
		fseek(fp, toff, SEEK_SET);
		if (type == FILE_TYPE_XPS)
		{
			// Skip the variable size header
			fread(&toff, 1, sizeof(int), fp);
			fseek(fp, toff, SEEK_CUR);
			fread(&toff, 1, sizeof(int), fp);
			fseek(fp, toff + 10, SEEK_CUR);
		}
		fread(data, 1, sizeof(data), fp);
		fclose(fp);

		cmd = _createCmdCode(PATCH_COMMAND, NULL, CMD_IMP_VMC2SAVE);
		cmd->file = strdup(psvPath);
		cmd->codes[1] = type;
		asprintf(&cmd->name, CHAR_ICON_COPY "%c (%.10s) %s", CHAR_TAG_PS2, data + 2, dir->d_name);
		list_append(list, cmd);

		LOG("[%s] F(%X) name '%s'", cmd->file, cmd->flags, cmd->name+2);
	}

	closedir(d);
}

int ReadVmc2Codes(save_entry_t * save)
{
	code_entry_t * cmd;
	option_value_t* optval;

	save->codes = list_alloc();

	if (save->type == FILE_TYPE_MENU)
	{
		add_vmc2_import_saves(save->codes, save->path, PS2_SAVES_PATH_USB);
		add_vmc2_import_saves(save->codes, save->path, PSV_SAVES_PATH_USB);
		if (!list_count(save->codes))
		{
			list_free(save->codes);
			save->codes = NULL;
			return 0;
		}

		list_bubbleSort(save->codes, &sortCodeList_Compare);

		return list_count(save->codes);
	}

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_WARN " Delete Save Game", CMD_DELETE_SAVE);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Save Game Backup " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save game to .PSU format", CMD_CODE_NULL);
	_createOptions(cmd, "Export .PSU save to USB", CMD_EXP_VMC2SAVE);
	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "Export .PSU save to HDD");
	asprintf(&optval->value, "%c%c", CMD_EXP_VMC2SAVE, STORAGE_HDD);
	list_append(cmd->options[0].opts, optval);
	cmd->options[0].id = FILE_TYPE_PSU;
	list_append(save->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export save game to .PSV format", CMD_CODE_NULL);
	_createOptions(cmd, "Export .PSV save to USB", CMD_EXP_VMC2SAVE);
	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "Export .PSV save to HDD");
	asprintf(&optval->value, "%c%c", CMD_EXP_VMC2SAVE, STORAGE_HDD);
	list_append(cmd->options[0].opts, optval);
	cmd->options[0].id = FILE_TYPE_PSV;
	list_append(save->codes, cmd);

	LOG("Loaded %ld codes", list_count(save->codes));

	return list_count(save->codes);
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
	option_value_t* optval;
	char path[256];
	snprintf(path, sizeof(path), APOLLO_LOCAL_CACHE "%s.txt", game->title_id);

	if (file_exists(path) == SUCCESS && strcmp(apollo_config.save_db, ONLINE_URL) == 0)
	{
		struct stat stats;
		stat(path, &stats);
		// re-download if file is +1 day old
		if ((stats.st_mtime + ONLINE_CACHE_TIMEOUT) < time(NULL))
			http_download(game->path, "saves.txt", path, 1);
	}
	else
	{
		if (!http_download(game->path, "saves.txt", path, 1))
			return 0;
	}

	long fsize;
	char *data = readTextFile(path, &fsize);
	if (!data)
		return 0;
	
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

			_createOptions(item, "Download to USB", CMD_DOWNLOAD_USB);
			optval = malloc(sizeof(option_value_t));
			asprintf(&optval->name, "Download to HDD");
			asprintf(&optval->value, "%c%c", CMD_DOWNLOAD_USB, STORAGE_HDD);
			list_append(item->options[0].opts, optval);
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
	free(data);

	if (!list_count(game->codes))
	{
		list_free(game->codes);
		game->codes = NULL;
		return 0;
	}

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

	item = _createSaveEntry(0, CHAR_ICON_NET " Network Tools (Downloader, Web Server)");
	item->path = strdup("/data/");
	item->type = FILE_TYPE_NET;
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

	case FILE_TYPE_NET:
		bup->codes = list_alloc();
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_NET " URL link Downloader (http, https, ftp, ftps)", CMD_URL_DOWNLOAD);
		list_append(bup->codes, cmd);
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_NET " Local Web Server (full system access)", CMD_NET_WEBSERVER);
		list_append(bup->codes, cmd);
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_NET " Toggle Web Browser history", CMD_BROWSER_HISTORY);
		list_append(bup->codes, cmd);
		return list_count(bup->codes);

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
			cmd = _createCmdCode(account ? PATCH_NULL : PATCH_COMMAND, tmp, account ? CMD_CODE_NULL : CMD_CREATE_ACT_DAT);
			cmd->codes[1] = i;
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

			LOG("[%s] name '%s'", cmd->file, cmd->name +2);
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
	list_node_t *node, *nc, *no;
	save_entry_t *item;
	code_entry_t *code;
	option_value_t* optval;

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
						for (no = list_head(code->options[z].opts); (optval = list_get(no)); no = list_next(no))
						{
							if (optval->name)
								free(optval->name);
							if (optval->value)
								free(optval->value);

							free(optval);
						}
						list_free(code->options[z].opts);

						if (code->options[z].line)
							free(code->options[z].line);
					}
					
					free (code->options);
				}

				free(code);
			}
			
			list_free(item->codes);
			item->codes = NULL;
		}

		free(item);
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

	int ret = strcasecmp(ta, tb);

	return (ret ? ret : sortSaveList_Compare(a, b));
}

static int parseTypeFlags(int flags)
{
	if (flags & SAVE_FLAG_PS4)
		return FILE_TYPE_PS4;
	else if (flags & SAVE_FLAG_PS1)
		return FILE_TYPE_PS1;
	else if (flags & SAVE_FLAG_PS2)
		return FILE_TYPE_PS2;
	else if (flags & SAVE_FLAG_VMC)
		return FILE_TYPE_VMC;

	return 0;
}

int sortSaveList_Compare_Type(const void* a, const void* b)
{
	int ta = parseTypeFlags(((save_entry_t*) a)->flags);
	int tb = parseTypeFlags(((save_entry_t*) b)->flags);

	if (ta == tb)
		return sortSaveList_Compare(a, b);
	else if (ta < tb)
		return -1;
	else
		return 1;
}

static void read_usb_encrypted_saves(const char* userPath, list_t *list, uint64_t account)
{
	DIR *d, *d2;
	struct dirent *dir, *dir2;
	save_entry_t *item, tmp;
	char savePath[256];
	char mount[ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE];

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

			snprintf(savePath, sizeof(savePath), "%s%s/", userPath, dir->d_name);
			tmp.path = savePath;
			tmp.title_id = dir->d_name;
			tmp.dir_name = dir2->d_name;
			if (!orbis_SaveMount(&tmp, SAVE_FLAG_LOCKED, mount))
			{
				snprintf(savePath, sizeof(savePath), CHAR_ICON_WARN "%s/%s", dir->d_name, dir2->d_name);
				item = _createSaveEntry(SAVE_FLAG_PS4 | SAVE_FLAG_LOCKED, savePath);
				item->type = FILE_TYPE_NULL;
				item->dir_name = strdup(dir2->d_name);
				asprintf(&item->path, "%s%s/", userPath, dir->d_name);
				asprintf(&item->title_id, "%.9s", dir->d_name);
				list_append(list, item);
				continue;
			}

			snprintf(savePath, sizeof(savePath), APOLLO_SANDBOX_PATH "sce_sys/param.sfo", mount);
			LOG("Reading %s...", savePath);

			sfo_context_t* sfo = sfo_alloc();
			if (sfo_read(sfo, savePath) < 0) {
				LOG("Unable to read from '%s'", savePath);
				sfo_free(sfo);
				continue;
			}

			snprintf(savePath, sizeof(savePath), "%s - %s", sfo_get_param_value(sfo, "MAINTITLE"), sfo_get_param_value(sfo, "SUBTITLE"));
			item = _createSaveEntry(SAVE_FLAG_PS4 | SAVE_FLAG_LOCKED, savePath);
			item->type = FILE_TYPE_PS4;
			item->dir_name = strdup(dir2->d_name);
			asprintf(&item->path, "%s%s/", userPath, dir->d_name);
			asprintf(&item->title_id, "%.9s", dir->d_name);

			uint64_t* int_data = (uint64_t*) sfo_get_param_value(sfo, "ACCOUNT_ID");
			if (int_data && (apollo_config.account_id == *int_data))
				item->flags |= SAVE_FLAG_OWNER;

			int_data = (uint64_t*) sfo_get_param_value(sfo, "SAVEDATA_BLOCKS");
			item->blocks = (*int_data);

			sfo_free(sfo);
			orbis_SaveUmount(mount);

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

static void scan_vmc_files(const char* userPath, const save_entry_t* parent, list_t *list)
{
	DIR *d;
	struct dirent *dir;
	save_entry_t *item;
	char psvPath[256];
	uint64_t size;
	uint16_t flag;

	d = opendir(userPath);
	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (dir->d_type != DT_REG || !(endsWith(dir->d_name, ".CARD") || endsWith(dir->d_name, ".VM2") || 
			endsWith(dir->d_name, ".BIN") || endsWith(dir->d_name, ".PS2") || endsWith(dir->d_name, ".VMC") ||
			// PS1 VMCs
			endsWith(dir->d_name, ".MCD") || endsWith(dir->d_name, ".MCR") || endsWith(dir->d_name, ".GME") ||
			endsWith(dir->d_name, ".VM1") || endsWith(dir->d_name, ".VMP") || endsWith(dir->d_name, ".VGS") ||
			endsWith(dir->d_name, ".SRM")))
			continue;

		snprintf(psvPath, sizeof(psvPath), "%s%s", userPath, dir->d_name);
		get_file_size(psvPath, &size);

		LOG("Checking %s...", psvPath);
		switch (size)
		{
		case PS1CARD_SIZE:
		case 0x20040:
		case 0x20080:
		case 0x200A0:
		case 0x20F40:
			flag = SAVE_FLAG_PS1;
			break;

		case 0x800000:
		case 0x840000:
		case 0x1000000:
		case 0x1080000:
		case 0x2000000:
		case 0x2100000:
		case 0x4000000:
		case 0x4200000:
			flag = SAVE_FLAG_PS2;
			break;

		default:
			continue;
		}

		item = _createSaveEntry(flag | SAVE_FLAG_VMC, dir->d_name);
		item->type = FILE_TYPE_VMC;

		if (parent)
		{
			item->flags |= (parent->flags & SAVE_FLAG_HDD);
			item->path = strdup((parent->flags & SAVE_FLAG_HDD) ? dir->d_name : psvPath);
			item->dir_name = strdup((parent->flags & SAVE_FLAG_HDD) ? parent->dir_name : userPath);
			item->title_id = strdup(parent->title_id);
			item->blocks = parent->blocks;

			free(item->name);
			asprintf(&item->name, "%s - %s", parent->name, dir->d_name);
		}
		else
		{
			item->title_id = strdup("VMC");
			item->path = strdup(psvPath);
			item->dir_name = strdup(userPath);
		}

		LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	closedir(d);
}

static void read_inner_vmc2_files(list_t *list)
{
	char mount[ORBIS_SAVE_DATA_DIRNAME_DATA_MAXSIZE];
	char save_path[256];
	list_node_t *node;
	save_entry_t *item;

	for (node = list_head(list); (item = list_get(node)); node = list_next(node))
	{
		if (item->type != FILE_TYPE_PS4 || (item->flags & SAVE_FLAG_LOCKED) || (strncmp("CUSA", item->title_id, 4) == 0))
			continue;

		if (item->flags & SAVE_FLAG_HDD)
		{
			if (!orbis_SaveMount(item, ORBIS_SAVE_DATA_MOUNT_MODE_RDONLY, mount))
				continue;

			snprintf(save_path, sizeof(save_path), APOLLO_SANDBOX_PATH, mount);
		}
		else
			snprintf(save_path, sizeof(save_path), "%s", item->path);

		scan_vmc_files(save_path, item, list);

		if (item->flags & SAVE_FLAG_HDD)
			orbis_SaveUmount(mount);
	}
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
	char path[64];

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

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_NET " Start local Web Server", CMD_SAVE_WEBSERVER);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Dump all decrypted Save Fingerprints", CMD_DUMP_FINGERPRINTS);
	list_append(item->codes, cmd);
	list_append(list, item);

	snprintf(path, sizeof(path), "%sPS4/APOLLO/", userPath);
	read_usb_savegames(path, list);
	read_inner_vmc2_files(list);

	snprintf(path, sizeof(path), "%sPS4/SAVEDATA/", userPath);
	read_usb_encrypted_savegames(path, list);

	snprintf(path, sizeof(path), "%s%s", userPath, VMC_PS2_PATH_USB);
	scan_vmc_files(path, NULL, list);

	snprintf(path, sizeof(path), "%s%s", userPath, VMC_PS1_PATH_USB);
	scan_vmc_files(path, NULL, list);

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
	_createOptions(cmd, "Copy Saves to USB", CMD_COPY_SAVES_USB);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy all Saves to USB", CMD_CODE_NULL);
	_createOptions(cmd, "Copy Saves to USB", CMD_COPY_ALL_SAVES_USB);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_NET " Start local Web Server", CMD_SAVE_WEBSERVER);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Dump all Save Fingerprints", CMD_DUMP_FINGERPRINTS);
	list_append(item->codes, cmd);
	list_append(list, item);

	appdb = open_sqlite_db(APP_DB_PATH_HDD);
	read_hdd_savegames(userPath, list, appdb);
	sqlite3_close(appdb);

	read_inner_vmc2_files(list);

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

	if (file_exists(path) == SUCCESS && strcmp(apollo_config.save_db, ONLINE_URL) == 0)
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
	if (!data)
		return;
	
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
			item->type = FILE_TYPE_ZIP;
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

	free(data);
}

list_t * ReadOnlineList(const char* urlPath)
{
	char url[256];
	list_t *list = list_alloc();

	// PS4 save-games (Zip folder)
	snprintf(url, sizeof(url), "%s" "PS4/", urlPath);
	_ReadOnlineListEx(url, SAVE_FLAG_PS4, list);

	// PS2 save-games (Zip PSV)
	snprintf(url, sizeof(url), "%s" "PS2/", urlPath);
	_ReadOnlineListEx(url, SAVE_FLAG_PS2, list);

	// PS1 save-games (Zip PSV)
	snprintf(url, sizeof(url), "%s" "PS1/", urlPath);
	_ReadOnlineListEx(url, SAVE_FLAG_PS1, list);

	if (!list_count(list))
	{
		list_free(list);
		return NULL;
	}

	return list;
}

list_t * ReadVmc1List(const char* userPath)
{
	char filePath[256];
	save_entry_t *item;
	code_entry_t *cmd;
	option_value_t* optval;
	list_t *list;
	ps1mcData_t* mcdata;

	if (!openMemoryCard(userPath, 0))
	{
		LOG("Error: no PS1 Memory Card detected! (%s)", userPath);
		return NULL;
	}

	mcdata = getMemoryCardData();
	if (!mcdata)
		return NULL;

	list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_PS1, CHAR_ICON_VMC " Memory Card Management");
	item->type = FILE_TYPE_MENU;
	item->path = strdup(userPath);
	item->title_id = strdup("VMC");
	item->codes = list_alloc();
	//bulk management hack
	item->dir_name = malloc(sizeof(void**));
	((void**)item->dir_name)[0] = list;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export selected Saves to USB", CMD_CODE_NULL);
	_createOptions(cmd, "Copy selected Saves to USB", CMD_EXP_SAVES_VMC);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export all Saves to USB", CMD_CODE_NULL);
	_createOptions(cmd, "Copy all Saves to USB", CMD_EXP_ALL_SAVES_VMC);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Virtual Memory Card " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export Memory Card to .VM1 format", CMD_CODE_NULL);
	cmd->file = strdup(strrchr(userPath, '/')+1);
	_createOptions(cmd, "Save .VM1 Memory Card to USB", CMD_EXP_PS1_VM1);
	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "Save .VM1 Memory Card to HDD");
	asprintf(&optval->value, "%c%c", CMD_EXP_PS1_VM1, STORAGE_HDD);
	list_append(cmd->options[0].opts, optval);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export Memory Card to .VMP format", CMD_CODE_NULL);
	cmd->file = strdup(strrchr(userPath, '/')+1);
	_createOptions(cmd, "Save .VMP Memory Card to USB", CMD_EXP_PS1_VMP);
	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "Save .VMP Memory Card to HDD");
	asprintf(&optval->value, "%c%c", CMD_EXP_PS1_VMP, STORAGE_HDD);
	list_append(cmd->options[0].opts, optval);
	list_append(item->codes, cmd);
	list_append(list, item);

	item = _createSaveEntry(SAVE_FLAG_PS1, CHAR_ICON_COPY " Import Saves to Virtual MemCard");
	item->path = strdup(FAKE_USB_PATH);
	item->title_id = strdup("HDD");
	item->dir_name = strdup(userPath);
	item->type = FILE_TYPE_MENU;
	list_append(list, item);

	for (int i = 0; i <= MAX_USB_DEVICES; i++)
	{
		snprintf(filePath, sizeof(filePath), USB_PATH PS1_SAVES_PATH_USB, i);
		if (i && dir_exists(filePath) != SUCCESS)
			continue;

		item = _createSaveEntry(SAVE_FLAG_PS1, CHAR_ICON_COPY " Import Saves to Virtual MemCard");
		asprintf(&item->path, USB_PATH, i);
		asprintf(&item->title_id, "USB %d", i);
		item->dir_name = strdup(userPath);
		item->type = FILE_TYPE_MENU;
		list_append(list, item);
	}

	for (int i = 0; i < PS1CARD_MAX_SLOTS; i++)
	{
		if (mcdata[i].saveType != PS1BLOCK_INITIAL)
			continue;

		LOG("Reading '%s'...", mcdata[i].saveName);

		char* tmp = sjis2utf8(mcdata[i].saveTitle);
		item = _createSaveEntry(SAVE_FLAG_PS1 | SAVE_FLAG_VMC, tmp);
		item->type = FILE_TYPE_PS1;
		item->blocks = i;
		item->title_id = strdup(mcdata[i].saveProdCode);
		item->dir_name =  strdup(mcdata[i].saveName);
		asprintf(&item->path, "%s\n%s", userPath, mcdata[i].saveName);
		free(tmp);

		LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	return list;
}

list_t * ReadVmc2List(const char* userPath)
{
	char filePath[256];
	save_entry_t *item;
	code_entry_t *cmd;
	option_value_t* optval;
	list_t *list;
	ps2_IconSys_t iconsys;
	int r, dd, fd;

	r = mcio_vmcInit(userPath);
	if (r < 0)
	{
		LOG("Error: no PS2 Memory Card detected! (%d)", r);
		return NULL;
	}

	list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_PS2, CHAR_ICON_VMC " Memory Card Management");
	item->type = FILE_TYPE_MENU;
	item->path = strdup(userPath);
	item->title_id = strdup("VMC");
	item->codes = list_alloc();
	//bulk management hack
	item->dir_name = malloc(sizeof(void**));
	((void**)item->dir_name)[0] = list;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export selected Saves to USB", CMD_CODE_NULL);
	_createOptions(cmd, "Copy selected Saves to USB", CMD_EXP_SAVES_VMC);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export all Saves to USB", CMD_CODE_NULL);
	_createOptions(cmd, "Copy all Saves to USB", CMD_EXP_ALL_SAVES_VMC);
	list_append(item->codes, cmd);
	list_append(list, item);

	cmd = _createCmdCode(PATCH_NULL, "----- " UTF8_CHAR_STAR " Virtual Memory Card " UTF8_CHAR_STAR " -----", CMD_CODE_NULL);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export Memory Card to .VM2 format", CMD_CODE_NULL);
	cmd->file = strdup(strrchr(userPath, '/')+1);
	_createOptions(cmd, "Save .VM2 Memory Card to USB", CMD_EXP_PS2_VM2);
	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "Save .VM2 Memory Card to HDD");
	asprintf(&optval->value, "%c%c", CMD_EXP_PS2_VM2, STORAGE_HDD);
	list_append(cmd->options[0].opts, optval);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export Memory Card to .VMC format (No ECC)", CMD_CODE_NULL);
	cmd->file = strdup(strrchr(userPath, '/')+1);
	_createOptions(cmd, "Save .VMC Memory Card to USB", CMD_EXP_PS2_RAW);
	optval = malloc(sizeof(option_value_t));
	asprintf(&optval->name, "Save .VMC Memory Card to HDD");
	asprintf(&optval->value, "%c%c", CMD_EXP_PS2_RAW, STORAGE_HDD);
	list_append(cmd->options[0].opts, optval);
	list_append(item->codes, cmd);

	item = _createSaveEntry(SAVE_FLAG_PS2, CHAR_ICON_COPY " Import Saves to Virtual MemCard");
	item->path = strdup(FAKE_USB_PATH);
	item->title_id = strdup("HDD");
	item->type = FILE_TYPE_MENU;
	list_append(list, item);

	for (int i = 0; i <= MAX_USB_DEVICES; i++)
	{
		snprintf(filePath, sizeof(filePath), USB_PATH PS2_SAVES_PATH_USB, i);
		if (i && dir_exists(filePath) != SUCCESS)
			continue;

		item = _createSaveEntry(SAVE_FLAG_PS2, CHAR_ICON_COPY " Import Saves to Virtual MemCard");
		asprintf(&item->path, USB_PATH, i);
		asprintf(&item->title_id, "USB %d", i);
		item->type = FILE_TYPE_MENU;
		list_append(list, item);
	}

	dd = mcio_mcDopen("/");
	if (dd < 0)
	{
		LOG("mcio Dopen Error %d", dd);
		return list;
	}

	struct io_dirent dirent;

	do {
		r = mcio_mcDread(dd, &dirent);
		if ((r) && (strcmp(dirent.name, ".")) && (strcmp(dirent.name, "..")))
		{
			snprintf(filePath, sizeof(filePath), "%s/icon.sys", dirent.name);
			LOG("Reading %s...", filePath);

			fd = mcio_mcOpen(filePath, sceMcFileAttrReadable | sceMcFileAttrFile);
			if (fd < 0) {
				LOG("Unable to read from '%s'", filePath);
				continue;
			}

			r = mcio_mcRead(fd, &iconsys, sizeof(ps2_IconSys_t));
			mcio_mcClose(fd);

			if (r != sizeof(ps2_IconSys_t))
				continue;

			if (iconsys.secondLineOffset)
			{
				memmove(&iconsys.title[iconsys.secondLineOffset+2], &iconsys.title[iconsys.secondLineOffset], sizeof(iconsys.title) - iconsys.secondLineOffset);
				iconsys.title[iconsys.secondLineOffset] = 0x81;
				iconsys.title[iconsys.secondLineOffset+1] = 0x50;
			}

			char* title = sjis2utf8(iconsys.title);
			item = _createSaveEntry(SAVE_FLAG_PS2 | SAVE_FLAG_VMC, title);
			item->type = FILE_TYPE_PS2;
			item->dir_name = strdup(dirent.name);
			asprintf(&item->title_id, "%.10s", dirent.name+2);
			asprintf(&item->path, "%s\n%s/\n%s", userPath, dirent.name, iconsys.copyIconName);
			free(title);

			LOG("[%s] F(%X) name '%s'", item->title_id, item->flags, item->name);
			list_append(list, item);
		}
	} while (r);

	mcio_mcDclose(dd);

	return list;
}

list_t * ReadTrophyList(const char* userPath)
{
	save_entry_t *item;
	code_entry_t *cmd;
	list_t *list;
	sqlite3 *db;
	sqlite3_stmt *res;

	if ((db = open_sqlite_db(userPath)) == NULL)
		return NULL;

	list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_PS4, CHAR_ICON_COPY " Export Trophies");
	item->type = FILE_TYPE_MENU;
	item->path = strdup(userPath);
	item->codes = list_alloc();
	//bulk management hack
	item->dir_name = malloc(sizeof(void**));
	((void**)item->dir_name)[0] = list;

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Backup selected Trophies to USB", CMD_CODE_NULL);
	_createOptions(cmd, "Save Trophies to USB", CMD_COPY_TROPHIES_USB);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Backup all Trophies to USB", CMD_CODE_NULL);
	_createOptions(cmd, "Save Trophies to USB", CMD_COPY_ALL_TROPHIES_USB);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_ZIP " Export all encrypted Trophies to .Zip", CMD_CODE_NULL);
	_createOptions(cmd, "Save .Zip to USB", CMD_ZIP_TROPHY_USB);
	list_append(item->codes, cmd);
	list_append(list, item);

	int rc = sqlite3_prepare_v2(db, "SELECT id, trophy_title_id, title FROM tbl_trophy_title WHERE status = 0", -1, &res, NULL);
	if (rc != SQLITE_OK)
	{
		LOG("Failed to fetch data: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return NULL;
	}

	while (sqlite3_step(res) == SQLITE_ROW)
	{
		item = _createSaveEntry(SAVE_FLAG_PS4 | SAVE_FLAG_TROPHY | SAVE_FLAG_HDD, (const char*) sqlite3_column_text(res, 2));
		item->blocks = sqlite3_column_int(res, 0);
		item->path = strdup(userPath);
		item->title_id = strdup((const char*) sqlite3_column_text(res, 1));
		item->dir_name = strdup(item->title_id);
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

	if(save->type == FILE_TYPE_PS1)
	{
		asprintf(details, "%s\n\n----- PS1 Save -----\n"
			"Game: %s\n"
			"Title ID: %s\n"
			"File: %s\n",
			save->path,
			save->name,
			save->title_id,
			save->dir_name);
		return 1;
	}

	if(save->type == FILE_TYPE_PS2)
	{
		asprintf(details, "%s\n\n----- PS2 Save -----\n"
			"Game: %s\n"
			"Title ID: %s\n"
			"Folder: %s\n"
			"Icon: %s\n",
			save->path,
			save->name,
			save->title_id,
			save->dir_name,
			strrchr(save->path, '\n')+1);
		return 1;
	}

	if(save->type == FILE_TYPE_VMC)
	{
		char *tmp = strrchr(save->path, '/');
		asprintf(details, "%s\n\n----- Virtual Memory Card -----\n"
			"File: %s\n"
			"Folder: %s\n",
			save->path,
			(tmp ? tmp+1 : save->path),
			save->dir_name);
		return 1;
	}

	if (save->flags & SAVE_FLAG_ONLINE)
	{
		asprintf(details, "%s\n----- Online Database -----\n"
			"Game: %s\n"
			"Title ID: %s\n",
			save->path,
			save->name,
			save->title_id);
		return 1;
	}

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
			strchr(save->path, 'T') + 3);

		return 1;
	}

	if(save->flags & SAVE_FLAG_HDD)
	{
		if ((db = open_sqlite_db(save->path)) == NULL)
			return 0;

		char* query = sqlite3_mprintf("SELECT sub_title, detail, free_blocks, size_kib, user_id, account_id, main_title, datetime(mtime) FROM savedata "
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
			"Date: %s\n"
			"Dir Name: %s\n"
			"Blocks: %d (%d Free)\n"
			"Size: %d Kb\n"
			"User ID: %08x\n"
			"Account ID: %016llx\n",
			save->path,
			sqlite3_column_text(res, 6),
			sqlite3_column_text(res, 0),
			sqlite3_column_text(res, 1),
			sqlite3_column_text(res, 7),
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
