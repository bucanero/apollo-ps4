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

#define UTF8_CHAR_GROUP		"\xe2\x97\x86"
#define UTF8_CHAR_ITEM		"\xe2\x94\x97"
#define UTF8_CHAR_STAR		"\xE2\x98\x85"

#define CHAR_ICON_ZIP		"\x0C"
#define CHAR_ICON_COPY		"\x0B"
#define CHAR_ICON_SIGN		"\x06"
#define CHAR_ICON_USER		"\x07"
#define CHAR_ICON_LOCK		"\x08"
#define CHAR_ICON_WARN		"\x0F"

const void* sqlite3_get_sqlite3Apis();
int sqlite3_memvfs_init(sqlite3 *db, char **pzErrMsg, const sqlite3_api_routines *pApi);

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
	LOG("Opening '%s'...", memuri);

	if (sqlite3_open_v2(memuri, &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_URI, "memvfs") != SQLITE_OK)
	{
		LOG("Error open memvfs: %s", sqlite3_errmsg(db));
		return NULL;
	}
	sqlite3_free(memuri);

	return db;
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
		LOG("UMOUNT_ERROR (%X)", umountErrorCode);

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

int orbis_UpdateSaveParams(const char* mountPath, const char* title, const char* subtitle, const char* details)
{
	OrbisSaveDataParam saveParams;
	OrbisSaveDataMountPoint mount;

	memset(&saveParams, 0, sizeof(OrbisSaveDataParam));
	memset(&mount, 0, sizeof(OrbisSaveDataMountPoint));

	strncpy(mount.data, mountPath, sizeof(mount.data));
	strncpy(saveParams.title, title, ORBIS_SAVE_DATA_TITLE_MAXSIZE);
	strncpy(saveParams.subtitle, subtitle, ORBIS_SAVE_DATA_SUBTITLE_MAXSIZE);
	strncpy(saveParams.details, details, ORBIS_SAVE_DATA_DETAIL_MAXSIZE);
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
	asprintf(&options->value[0], "%c%c", value, 0);
	asprintf(&options->name[1], "%s %d", name, 1);
	asprintf(&options->value[1], "%c%c", value, 1);

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
}

int set_psx_import_codes(save_entry_t* item)
{
	code_entry_t* cmd;
	item->codes = list_alloc();

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Convert to .PSV", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(2, "Save .PSV file to USB", CMD_CONVERT_TO_PSV);
	list_append(item->codes, cmd);

	return list_count(item->codes);
}

int set_pfs_codes(save_entry_t* item)
{
	code_entry_t* cmd;
	item->codes = list_alloc();

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(item->codes, cmd);
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy Save game to HDD", CMD_COPY_PFS);
	list_append(item->codes, cmd);

	return list_count(item->codes);
}

int set_psv_codes(save_entry_t* item)
{
	code_entry_t* cmd;
	item->codes = list_alloc();

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Resign .PSV file", CMD_RESIGN_PSV);
	list_append(item->codes, cmd);

	if (item->flags & SAVE_FLAG_PS1)
	{
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export PS1 save to .MCS", CMD_CODE_NULL);
		cmd->options_count = 1;
		cmd->options = _createOptions(2, "Save .MCS file to USB", CMD_EXP_PSV_MCS);
	}
	else
	{
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export PS2 save to .PSU", CMD_CODE_NULL);
		cmd->options_count = 1;
		cmd->options = _createOptions(2, "Save .PSU file to USB", CMD_EXP_PSV_PSU);
	}
	list_append(item->codes, cmd);

	return list_count(item->codes);
}

int set_ps2_codes(save_entry_t* item)
{
	code_entry_t* cmd;
	item->codes = list_alloc();

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_USER " View Save Details", CMD_VIEW_DETAILS);
	list_append(item->codes, cmd);
/*
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy dummy .PSV Save", CMD_CODE_NULL);
	asprintf(&cmd->file, "APOLLO-99PS2.PSV");
	cmd->options_count = 1;
	cmd->options = _createOptions(2, "Copy APOLLO-99PS2.PSV to USB", CMD_COPY_DUMMY_PSV);
	list_append(item->codes, cmd);
*/
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
	long bufferLen;
	char * buffer = NULL;
	char mount[32];
	char *tmp;

	if (save->flags & SAVE_FLAG_PSV)
		return set_psv_codes(save);

	if (save->flags & SAVE_FLAG_PS2)
		return set_ps2_codes(save);

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
	buffer = readTextFile(filePath, &bufferLen);
	load_patch_code_list(buffer, save->codes, &get_file_entries, save->path);
	free (buffer);

	LOG("Loaded %d codes", list_count(save->codes));

skip_end:
	if (save->flags & SAVE_FLAG_HDD)
	{
		orbis_SaveUmount(mount);
		free(save->path);
		save->path = tmp;
	}

	return list_count(save->codes);
}

/*
char* _get_xml_node_value(xmlNode * a_node, const xmlChar* node_name)
{
	xmlNode *cur_node = NULL;
	char *value = NULL;

	for (cur_node = a_node; cur_node && !value; cur_node = cur_node->next)
	{
		if (cur_node->type != XML_ELEMENT_NODE)
			continue;

		if (xmlStrcasecmp(cur_node->name, node_name) == 0)
		{
			value = (char*) xmlNodeGetContent(cur_node);
//			LOG("xml value=%s", value);
		}
	}

	return value;
}
*/

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
	char *end = data + fsize + 1;

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
			item->options = _createOptions(2, "Download to USB", CMD_DOWNLOAD_USB);
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

	return (list_count(game->codes));
}

list_t * ReadBackupList(const char* userPath)
{
	char tmp[32];
	save_entry_t * item;
	code_entry_t * cmd;
	list_t *list = list_alloc();

/*
	item = _createSaveEntry(SAVE_FLAG_PS4, CHAR_ICON_COPY " Export Licenses");
	asprintf(&item->path, EXDATA_PATH_HDD, apollo_config.user_id);
	item->type = FILE_TYPE_RIF;
	list_append(list, item);
*/

	item = _createSaveEntry(SAVE_FLAG_PS4, CHAR_ICON_USER " Activate PS4 Accounts");
	asprintf(&item->path, EXDATA_PATH_HDD, apollo_config.user_id);
	item->type = FILE_TYPE_ACT;
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
	char fext[5] = "";

	switch(bup->type)
	{
	case FILE_TYPE_RIF:
		sprintf(fext, ".rif");
		break;

	case FILE_TYPE_RAP:
		sprintf(fext, ".rap");
		break;

	case FILE_TYPE_ACT:
		bup->codes = list_alloc();

		LOG("Getting Users...");
		for (int i = 1; i <= 16; i++)
		{
			uint64_t account;
			char userName[0x20];
			char tmp[0x80];

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
				cmd->options->size = get_xml_owners(APOLLO_PATH OWNER_XML_FILE, CMD_CREATE_ACT_DAT, &cmd->options->name, &cmd->options->value);
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

	LOG("Loading %s files from '%s'...", fext, bup->path);
/*
	if (bup->type == FILE_TYPE_RIF)
	{
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_ZIP " Backup All Licenses to .Zip", CMD_CODE_NULL);
		cmd->options_count = 1;
		cmd->options = _createOptions(2, "Save .Zip to USB", CMD_EXP_EXDATA_USB);
		list_append(bup->codes, cmd);

		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Export All Licenses as .RAPs", CMD_CODE_NULL);
		cmd->options_count = 1;
		cmd->options = _createOptions(3, "Save .RAPs to USB", CMD_EXP_LICS_RAPS);
		asprintf(&cmd->options->name[2], "Save .RAPs to HDD");
		asprintf(&cmd->options->value[2], "%c%c", CMD_EXP_LICS_RAPS, 0x10);
		list_append(bup->codes, cmd);
	}

	if (bup->type == FILE_TYPE_RAP)
	{
		cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Import All .RAPs as .RIF Licenses", CMD_IMP_EXDATA_USB);
		list_append(bup->codes, cmd);
	}

	DIR *d;
	struct dirent *dir;
	d = opendir(bup->path);

	if (d)
	{
		while ((dir = readdir(d)) != NULL)
		{
			if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0  &&
				endsWith(dir->d_name, fext))
			{
				cmd = _createCmdCode(PATCH_COMMAND, dir->d_name, CMD_CODE_NULL);
				*strrchr(cmd->name, '.') = 0;

				if (bup->type == FILE_TYPE_RIF)
				{
					cmd->options_count = 1;
					cmd->options = _createOptions(3, "Save .RAP to USB", CMD_EXP_LICS_RAPS);
					asprintf(&cmd->options->name[2], "Save .RAP to HDD");
					asprintf(&cmd->options->value[2], "%c%c", CMD_EXP_LICS_RAPS, 0x10);
				}
				else if (bup->type == FILE_TYPE_RAP)
				{
					sprintf(cmd->codes, "%c", CMD_IMP_EXDATA_USB);
				}

				cmd->file = strdup(dir->d_name);
				list_append(bup->codes, cmd);

				LOG("Added File '%s'", cmd->file);
			}
		}
		closedir(d);
	}
*/
	LOG("%d items loaded", list_count(bup->codes));

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

			LOG("[%s] F(%d) name '%s'", item->title_id, item->flags, item->name);
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
			
		LOG("[%s] F(%d) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	closedir(d);
}

static void read_hdd_savegames(const char* userPath, list_t *list)
{
	save_entry_t *item;
	sqlite3_stmt *res;
	sqlite3 *db = open_sqlite_db(userPath);

	if (!db)
		return;

	int rc = sqlite3_prepare_v2(db, "SELECT title_id, dir_name, main_title, blocks, account_id, user_id FROM savedata", -1, &res, NULL);
	if (rc != SQLITE_OK)
	{
		LOG("Failed to fetch data: %s", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	while (sqlite3_step(res) == SQLITE_ROW)
	{
		item = _createSaveEntry(SAVE_FLAG_PS4 | SAVE_FLAG_HDD, (const char*) sqlite3_column_text(res, 2));
		item->type = FILE_TYPE_PS4;
		item->path = strdup(userPath);
		item->dir_name = strdup((const char*) sqlite3_column_text(res, 1));
		item->title_id = strdup((const char*) sqlite3_column_text(res, 0));
		item->blocks = sqlite3_column_int(res, 3);
		item->flags |= (apollo_config.account_id == (uint64_t)sqlite3_column_int64(res, 4) ? SAVE_FLAG_OWNER : 0);

		LOG("[%s] F(%d) {%d} '%s'", item->title_id, item->flags, item->blocks, item->name);
		list_append(list, item);
	}

	sqlite3_finalize(res);
	sqlite3_close(db);
}

void read_psv_savegames(const char* userPath, list_t *list)
{
	DIR *d;
	struct dirent *dir;
	save_entry_t *item;
	char psvPath[256];
	char data[0x100];

	d = opendir(userPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (dir->d_type != DT_REG || !endsWith(dir->d_name, ".PSV"))
			continue;

		snprintf(psvPath, sizeof(psvPath), "%s%s", userPath, dir->d_name);
		LOG("Reading %s...", psvPath);

		FILE *psvf = fopen(psvPath, "rb");
		if (!psvf) {
			LOG("Unable to open '%s'", psvPath);
			continue;
		}

		fread(data, 1, sizeof(data), psvf);
		if (memcmp(data, "\x00VSP", 4) != 0)
		{
			LOG("Not a PSV file");
			fclose(psvf);
			continue;
		}

		uint8_t type = data[0x3C];
		uint32_t pos = ES32(*(uint32_t*)(data + 0x44));

		fseek(psvf, pos, SEEK_SET);
		fread(data, 1, sizeof(data), psvf);
		fclose(psvf);

		item = (save_entry_t *)malloc(sizeof(save_entry_t));
		item->codes = NULL;
		item->flags = SAVE_FLAG_PSV | (type == 1 ? SAVE_FLAG_PS1 : SAVE_FLAG_PS2);

		asprintf(&item->path, "%s%s", userPath, dir->d_name);
		asprintf(&item->title_id, "%.12s", dir->d_name);

		//PS2 Title offset 0xC0
		//PS1 Title offset 0x04
		item->name = sjis2utf8(data + (type == 1 ? 0x04 : 0xC0));
			
		LOG("[%s] F(%d) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	closedir(d);
}

void read_psx_savegames(const char* userPath, list_t *list)
{
	DIR *d;
	struct dirent *dir;
	save_entry_t *item;
	char psvPath[256];
	char data[64];
	int type, toff;

	d = opendir(userPath);

	if (!d)
		return;

	while ((dir = readdir(d)) != NULL)
	{
		if (dir->d_type != DT_REG)
			continue;

		if (endsWith(dir->d_name, ".PSX"))
		{
			toff = 0;
			type = FILE_TYPE_PSX;
		}
		else if (endsWith(dir->d_name, ".MCS"))
		{
			toff = 0x0A;
			type = FILE_TYPE_MCS;
		}
		else if (endsWith(dir->d_name, ".MAX"))
		{
			toff = 0x10;
			type = FILE_TYPE_MAX;
		}
		else if (endsWith(dir->d_name, ".CBS"))
		{
			toff = 0x14;
			type = FILE_TYPE_CBS;
		}
		else if (endsWith(dir->d_name, ".PSU"))
		{
			toff = 0x40;
			type = FILE_TYPE_PSU;
		}
		else if (endsWith(dir->d_name, ".XPS") || endsWith(dir->d_name, ".SPS"))
		{
			toff = 0x04;
			type = FILE_TYPE_XPS;
		}
		else
			continue;

		snprintf(psvPath, sizeof(psvPath), "%s%s", userPath, dir->d_name);
		LOG("Reading %s...", psvPath);

		FILE *fp = fopen(psvPath, "rb");
		if (!fp) {
			LOG("Unable to open '%s'", psvPath);
			continue;
		}

		fseek(fp, toff, SEEK_SET);
		fread(data, 1, sizeof(data), fp);
		fclose(fp);

		item = _createSaveEntry(SAVE_FLAG_PS2, dir->d_name);
		set_psx_import_codes(item);

		if (type == FILE_TYPE_PSX || type == FILE_TYPE_MCS)
			item->flags = SAVE_FLAG_PS1;

		item->type = type;
		asprintf(&item->path, "%s%s", userPath, dir->d_name);
		asprintf(&item->title_id, "%.12s", data);
			
		LOG("[%s] F(%d) name '%s'", item->title_id, item->flags, item->name);
		list_append(list, item);
	}

	closedir(d);
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
	item->path = strdup(pathDec);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Resign all Saves", CMD_RESIGN_ALL_SAVES);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy all Saves to HDD", CMD_COPY_SAVES_HDD);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Dump all Save Fingerprints", CMD_DUMP_FINGERPRINTS);
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

	if (file_exists(userPath) != SUCCESS)
		return NULL;

	list = list_alloc();

	item = _createSaveEntry(SAVE_FLAG_PS4, CHAR_ICON_COPY " Bulk Save Management");
	item->type = FILE_TYPE_MENU;
	item->codes = list_alloc();
	item->path = strdup(userPath);
/*
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_SIGN " Resign & Unlock all Saves", CMD_RESIGN_ALL_SAVES);
	list_append(item->codes, cmd);
*/
	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_COPY " Copy all Saves to USB", CMD_CODE_NULL);
	cmd->options_count = 1;
	cmd->options = _createOptions(2, "Copy Saves to USB", CMD_COPY_SAVES_USB);
	list_append(item->codes, cmd);

	cmd = _createCmdCode(PATCH_COMMAND, CHAR_ICON_LOCK " Dump all Save Fingerprints", CMD_DUMP_FINGERPRINTS);
	list_append(item->codes, cmd);
	list_append(list, item);

	read_hdd_savegames(userPath, list);

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
	char *end = data + fsize + 1;

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
	int rc = sqlite3_prepare_v2(db, "SELECT id, trophy_title_id, title FROM tbl_trophy_title", -1, &res, NULL);
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

		LOG("[%s] F(%d) name '%s'", item->title_id, item->flags, item->name);
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

		char* query = sqlite3_mprintf("SELECT sub_title, detail, free_blocks, size_kib, user_id, account_id FROM savedata "
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
			save->path, save->name, 
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
