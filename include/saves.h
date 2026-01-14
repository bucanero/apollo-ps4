#include <apollo.h>
#include <dbglogger.h>
#define LOG dbglogger_log

#define APOLLO_PATH				"/data/apollo/"
#define APOLLO_SANDBOX_PATH		"/data/apollo/mount/%s/"

#ifdef APOLLO_ENABLE_LOGGING
#define APOLLO_APP_PATH			"/data/apollo/debug/"
#define APOLLO_SETTING_PATH		"/mnt/sandbox/LOAD00044_000%s/"
#else
#define APOLLO_APP_PATH			"/mnt/sandbox/APOL00004_000/app0/assets/"
#define APOLLO_SETTING_PATH		"/mnt/sandbox/APOL00004_000%s/"
#endif

#define APOLLO_USER_PATH		APOLLO_PATH "%08x/"
#define APOLLO_DATA_PATH		APOLLO_PATH "data/"
#define APOLLO_LOCAL_CACHE		APOLLO_PATH "cache/"
#define APOLLO_UPDATE_URL		"https://api.github.com/repos/bucanero/apollo-ps4/releases/latest"

#define MAX_USB_DEVICES         8
#define USB0_PATH               "/mnt/usb0/"
#define USB1_PATH               "/mnt/usb1/"
#define USB_PATH                "/mnt/usb%d/"
#define FAKE_USB_PATH           "/data/fakeusb/"
#define SAVES_DB_PATH           "/system_data/savedata/%08x/db/user/savedata.db"

#define PS4_SAVES_PATH_USB      "PS4/APOLLO/"
#define PS2_SAVES_PATH_USB      "PS2/SAVEDATA/"
#define PS1_SAVES_PATH_USB      "PS1/SAVEDATA/"
#define PSV_SAVES_PATH_USB      "PS3/EXPORT/PSV/"
#define TROPHIES_PATH_USB       "PS4/EXPORT/TROPHY/"

#define SAVES_PATH_HDD          "/user/home/%08x/savedata/"
#define SAVE_ICON_PATH_HDD      "/user/home/%08x/savedata_meta/user/"
#define TROPHY_PATH_HDD         "/user/home/%08x/trophy/data/"

#define SAVES_PATH_USB0         USB0_PATH PS4_SAVES_PATH_USB
#define SAVES_PATH_USB1         USB1_PATH PS4_SAVES_PATH_USB

#define TROPHY_PATH_USB0        USB0_PATH TROPHIES_PATH_USB
#define TROPHY_PATH_USB1        USB1_PATH TROPHIES_PATH_USB
#define TROPHY_DB_PATH          "/user/home/%08x/trophy/db/trophy_local.db"
#define APP_DB_PATH_HDD         "/system_data/priv/mms/app.db"

#define EXPORT_PATH_USB0        USB0_PATH "PS4/EXPORT/"
#define EXPORT_PATH_USB1        USB1_PATH "PS4/EXPORT/"
#define EXPORT_DB_PATH          APOLLO_PATH "export/db/"

#define EXP_PSV_PATH_USB0       USB0_PATH PSV_SAVES_PATH_USB
#define EXP_PSV_PATH_USB1       USB1_PATH PSV_SAVES_PATH_USB

#define VMC_PS2_PATH_USB        "PS2/VMC/"
#define VMC_PS1_PATH_USB        "PS1/VMC/"

#define IMP_PS2VMC_PATH_USB     USB_PATH "PS2/VMC/"

#define ONLINE_URL              "https://bucanero.github.io/apollo-saves/"
#define ONLINE_PATCH_URL        "https://bucanero.github.io/apollo-patches/PS4/"
#define ONLINE_CACHE_TIMEOUT    24*3600     // 1-day local cache

#define OWNER_XML_FILE          "owners.xml"

enum storage_enum
{
    STORAGE_USB0,
    STORAGE_USB1,
    STORAGE_HDD,
};

enum cmd_code_enum
{
    CMD_CODE_NULL,

// Trophy commands
    CMD_UPDATE_TROPHY,
    CMD_EXP_TROPHY_USB,
    CMD_COPY_TROPHIES_USB,
    CMD_COPY_ALL_TROPHIES_USB,
    CMD_ZIP_TROPHY_USB,

// Save commands
    CMD_DECRYPT_FILE,
    CMD_RESIGN_SAVE,
    CMD_DOWNLOAD_USB,
    CMD_DOWNLOAD_HDD,
    CMD_COPY_SAVE_USB,
    CMD_COPY_SAVE_HDD,
    CMD_EXPORT_ZIP_USB,
    CMD_EXPORT_ZIP_HDD,
    CMD_VIEW_DETAILS,
    CMD_VIEW_RAW_PATCH,
    CMD_RESIGN_PSV,
    CMD_EXP_FINGERPRINT,
    CMD_HEX_EDIT_FILE,
    CMD_COPY_PFS,
    CMD_IMPORT_DATA_FILE,
    CMD_DELETE_SAVE,
    CMD_UPLOAD_SAVE,

// Bulk commands
    CMD_RESIGN_SAVES,
    CMD_RESIGN_ALL_SAVES,
    CMD_COPY_SAVES_USB,
    CMD_COPY_ALL_SAVES_USB,
    CMD_COPY_SAVES_HDD,
    CMD_COPY_ALL_SAVES_HDD,
    CMD_DUMP_FINGERPRINTS,
    CMD_SAVE_WEBSERVER,
    CMD_EXP_SAVES_VMC,
    CMD_EXP_ALL_SAVES_VMC,

// Export commands
    CMD_EXP_KEYSTONE,
    CMD_EXP_PS2_VM2,
    CMD_EXP_PS2_RAW,
    CMD_EXP_VMC2SAVE,
    CMD_EXP_VMC1SAVE,
    CMD_EXP_PS1_VMP,
    CMD_EXP_PS1_VM1,
    CMD_EXP_DATABASE,
    CMD_DB_REBUILD,
    CMD_DB_DLC_REBUILD,

// Import commands
    CMD_IMP_KEYSTONE,
    CMD_IMP_DATABASE,
    CMD_IMP_VMC2SAVE,
    CMD_IMP_VMC1SAVE,
    CMD_CREATE_ACT_DAT,
    CMD_EXTRACT_ARCHIVE,
    CMD_URL_DOWNLOAD,
    CMD_NET_WEBSERVER,
    CMD_BROWSER_HISTORY,

// SFO patches
    SFO_UNLOCK_COPY,
    SFO_CHANGE_ACCOUNT_ID,
    SFO_REMOVE_PSID,
    SFO_CHANGE_TITLE_ID,
};

// Save flags
#define SAVE_FLAG_ONLINE        1
#define SAVE_FLAG_OWNER         2
#define SAVE_FLAG_SELECTED      4
#define SAVE_FLAG_ZIP           8
#define SAVE_FLAG_PS2           16
#define SAVE_FLAG_PS1           32
#define SAVE_FLAG_PSV           64
#define SAVE_FLAG_TROPHY        128
#define SAVE_FLAG_LOCKED        256
#define SAVE_FLAG_PS4           512
#define SAVE_FLAG_HDD           1024
#define SAVE_FLAG_VMC           2048
#define SAVE_FLAG_UPDATED       4096

enum save_type_enum
{
    FILE_TYPE_NULL,
    FILE_TYPE_PS1,
    FILE_TYPE_PS2,
    FILE_TYPE_MENU,
    FILE_TYPE_PS4,
    FILE_TYPE_TRP,
    FILE_TYPE_VMC,

    // PS1 File Types
    FILE_TYPE_PSX,
    FILE_TYPE_MCS,

    // Misc
    FILE_TYPE_ZIP,
    FILE_TYPE_SQL,
    FILE_TYPE_NET,

    // License Files
    FILE_TYPE_RIF,
    FILE_TYPE_RAP,
    FILE_TYPE_ACT,

    // PS2 File Types
    FILE_TYPE_PSU,
    FILE_TYPE_MAX,
    FILE_TYPE_CBS,
    FILE_TYPE_XPS,
    FILE_TYPE_PSV,
};

enum char_flag_enum
{
    CHAR_TAG_NULL,
    CHAR_TAG_PS1,
    CHAR_TAG_PS2,
    CHAR_TAG_PS3,
    CHAR_TAG_PSP,
    CHAR_TAG_PSV,
    CHAR_TAG_APPLY,
    CHAR_TAG_OWNER,
    CHAR_TAG_LOCKED,
    CHAR_TAG_NET,
    CHAR_RES_LF,
    CHAR_TAG_VMC,
    CHAR_TAG_ZIP,
    CHAR_RES_CR,
    CHAR_TAG_TRANSFER,
    CHAR_TAG_WARNING,
    CHAR_BTN_X,
    CHAR_BTN_S,
    CHAR_BTN_T,
    CHAR_BTN_O,
    CHAR_TRP_BRONZE,
    CHAR_TRP_SILVER,
    CHAR_TRP_GOLD,
    CHAR_TRP_PLATINUM,
    CHAR_TRP_SYNC,
    CHAR_TAG_PS4,
};

enum code_type_enum
{
    PATCH_NULL,
    PATCH_GAMEGENIE = APOLLO_CODE_GAMEGENIE,
    PATCH_BSD = APOLLO_CODE_BSD,
    PATCH_PYTHON = APOLLO_CODE_PYTHON,
    PATCH_COMMAND,
    PATCH_SFO,
    PATCH_TROP_UNLOCK,
    PATCH_TROP_LOCK,
};

enum save_sort_enum
{
    SORT_DISABLED,
    SORT_BY_NAME,
    SORT_BY_TITLE_ID,
    SORT_BY_TYPE,
};

typedef struct save_entry
{
    char * name;
    char * title_id;
    char * path;
    char * dir_name;
    uint32_t blocks;
    uint16_t flags;
    uint16_t type;
    list_t * codes;
} save_entry_t;

typedef struct
{
    list_t * list;
    char path[128];
    const char* title;
    uint8_t id;
    void (*UpdatePath)(char *);
    int (*ReadCodes)(save_entry_t *);
    list_t* (*ReadList)(const char*);
} save_list_t;

list_t * ReadUsbList(const char* userPath);
list_t * ReadUserList(const char* userPath);
list_t * ReadOnlineList(const char* urlPath);
list_t * ReadBackupList(const char* userPath);
list_t * ReadTrophyList(const char* userPath);
list_t * ReadVmc1List(const char* userPath);
list_t * ReadVmc2List(const char* userPath);
void UnloadGameList(list_t * list);
char * readTextFile(const char * path);
int sortSaveList_Compare(const void* A, const void* B);
int sortSaveList_Compare_TitleID(const void* A, const void* B);
int sortSaveList_Compare_Type(const void* A, const void* B);
int sortCodeList_Compare(const void* A, const void* B);
int ReadCodes(save_entry_t * save);
int ReadTrophies(save_entry_t * game);
int ReadOnlineSaves(save_entry_t * game);
int ReadBackupCodes(save_entry_t * bup);
int ReadVmc1Codes(save_entry_t * save);
int ReadVmc2Codes(save_entry_t * save);

int http_init(void);
void http_end(void);
int http_download(const char* url, const char* filename, const char* local_dst, int show_progress);
int ftp_upload(const char* local_file, const char* url, const char* filename, int show_progress);

int extract_7zip(const char* zip_file, const char* dest_path);
int extract_rar(const char* rar_file, const char* dest_path);
int extract_zip(const char* zip_file, const char* dest_path);
int zip_directory(const char* basedir, const char* inputdir, const char* output_zipfile);
int zip_file(const char* input, const char* output_zipfile);
int extract_sfo(const char* zip_file, const char* dest_path);

int show_dialog(int dialog_type, const char * format, ...);
int osk_dialog_get_text(const char* title, char* text, uint32_t size);
void init_progress_bar(const char* msg);
void update_progress_bar(uint64_t progress, const uint64_t total_size, const char* msg);
void end_progress_bar(void);
#define show_message(...)	show_dialog(DIALOG_TYPE_OK, __VA_ARGS__)

int init_loading_screen(const char* message);
void stop_loading_screen(void);

void execCodeCommand(code_entry_t* code, const char* codecmd);

void* open_sqlite_db(const char* db_path);
int save_sqlite_db(void* db, const char* db_path);
int get_appdb_title(void* db, const char* titleid, char* name);
int get_name_title_id(const char* titleid, char* name);
int appdb_rebuild(const char* db_path, uint32_t userid);
int appdb_fix_delete(const char* db_path, uint32_t userid);
int addcont_dlc_rebuild(const char* db_path);

int regMgr_GetParentalPasscode(char* passcode);
int regMgr_GetUserName(int userNumber, char* outString);
int regMgr_GetAccountId(int userNumber, uint64_t* psnAccountId);
int regMgr_SetAccountId(int userNumber, uint64_t* psnAccountId);

int get_save_details(const save_entry_t *save, char** details);
int orbis_SaveUmount(const char* mountPath);
int orbis_SaveMount(const save_entry_t *save, uint32_t mode, char* mountPath);
int orbis_SaveDelete(const save_entry_t *save);
int orbis_UpdateSaveParams(const save_entry_t* save, const char* title, const char* subtitle, const char* details, uint32_t up);

int trophy_lock(const save_entry_t* game, int trp_id, int grp_id, int type);
int trophy_unlock(const save_entry_t* game, int trp_id, int grp_id, int type);
int trophySet_delete(const save_entry_t* game);

int vmc_export_psv(const char* save, const char* out_path);
int vmc_export_psu(const char* path, const char* output);
int vmc_import_psv(const char *input);
int vmc_import_psu(const char *input);
int vmc_delete_save(const char* path);

int ps2_xps2psv(const char *save, const char *psv_path);
int ps2_cbs2psv(const char *save, const char *psv_path);
int ps2_max2psv(const char *save, const char *psv_path);

int psv_resign(const char *src_psv);
int vmp_resign(const char *src_vmp);

char* sjis2utf8(char* input);
uint8_t* getIconPS2(const char* folder, const char* iconfile);
const char* get_fw_by_pfskey_ver(int key_ver);
