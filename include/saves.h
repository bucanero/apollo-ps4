#include <apollo.h>
//#include <dbglogger.h>
#define LOG printf

#define APOLLO_PATH				"/data/apollo/"
#define APOLLO_APP_PATH			"/mnt/sandbox/APOL00004_000/app0/assets/"
#define APOLLO_SANDBOX_PATH		"/mnt/sandbox/APOL00004_000%s/"
#define APOLLO_USER_PATH		APOLLO_PATH "%08x/"
#define APOLLO_DATA_PATH		APOLLO_PATH "data/"
#define APOLLO_LOCAL_CACHE		APOLLO_PATH "cache/"
#define APOLLO_UPDATE_URL		"https://api.github.com/repos/bucanero/apollo-ps4/releases/latest"

#define MAX_USB_DEVICES         6
#define USB0_PATH               "/mnt/usb0/"
#define USB1_PATH               "/mnt/usb1/"
#define USB_PATH                "/mnt/usb%d/"
#define USER_PATH_HDD           "/system_data/savedata/%08x/db/user/savedata.db"

#define PS4_SAVES_PATH_USB      "PS4/APOLLO/"
#define PS2_SAVES_PATH_USB      "PS3/EXPORT/PS2SD/"
#define PSP_SAVES_PATH_USB      "PSP/SAVEDATA/"
#define PSV_SAVES_PATH_USB      "PS3/EXPORT/PSV/"
#define TROPHIES_PATH_USB       "PS4/EXPORT/TROPHY/"

#define PS3_LICENSE_PATH        "exdata/"
#define PS4_SAVES_PATH_HDD      "/user/home/%08x/savedata_meta/user/"
#define PS2_SAVES_PATH_HDD      "ps2emu2_savedata/"
#define PSP_SAVES_PATH_HDD      "minis_savedata/"

#define PS1_IMP_PATH_USB        "PS1/SAVEDATA/"
#define PS2_IMP_PATH_USB        "PS2/SAVEDATA/"

#define SAVES_PATH_USB0         USB0_PATH PS4_SAVES_PATH_USB
#define SAVES_PATH_USB1         USB1_PATH PS4_SAVES_PATH_USB

#define TROPHY_PATH_USB0        USB0_PATH TROPHIES_PATH_USB
#define TROPHY_PATH_USB1        USB1_PATH TROPHIES_PATH_USB
#define TROPHY_PATH_HDD         "/user/home/%08x/trophy/db/trophy_local.db"
#define EXDATA_PATH_HDD			USER_PATH_HDD PS3_LICENSE_PATH

#define EXPORT_PATH_USB0        USB0_PATH "PS4/EXPORT/"
#define EXPORT_PATH_USB1        USB1_PATH "PS4/EXPORT/"
#define EXPORT_RAP_PATH_USB     USB_PATH PS3_LICENSE_PATH
#define EXPORT_RAP_PATH_HDD     "/dev_hdd0/" PS3_LICENSE_PATH

#define EXP_PSV_PATH_USB0       USB0_PATH PSV_SAVES_PATH_USB
#define EXP_PSV_PATH_USB1       USB1_PATH PSV_SAVES_PATH_USB

#define EXP_PS2_PATH_USB0       USB0_PATH "PS2/VMC/"
#define EXP_PS2_PATH_USB1       USB1_PATH "PS2/VMC/"
#define EXP_PS2_PATH_HDD        "/dev_hdd0/savedata/vmc/"

#define IMP_PS2VMC_PATH_USB     USB_PATH "PS2/VMC/"
#define IMPORT_RAP_PATH_USB     USB_PATH PS3_LICENSE_PATH

#define ONLINE_URL				"https://bucanero.github.io/apollo-saves/"
#define ONLINE_CACHE_TIMEOUT    24*3600     // 1-day local cache

#define OWNER_XML_FILE          "owners.xml"

enum cmd_code_enum
{
    CMD_CODE_NULL,

// Trophy commands
    CMD_RESIGN_TROPHY,
    CMD_EXP_TROPHY_USB,
    CMD_COPY_TROPHIES_USB,
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
    CMD_CONVERT_TO_PSV,
    CMD_COPY_PFS,
    CMD_IMPORT_DATA_FILE,

// Bulk commands
    CMD_RESIGN_ALL_SAVES,
    CMD_COPY_SAVES_USB,
    CMD_COPY_SAVES_HDD,
    CMD_DUMP_FINGERPRINTS,

// Export commands
    CMD_EXP_KEYSTONE,
    CMD_EXP_LICS_RAPS,
    CMD_EXP_FLASH2_USB,
    CMD_EXP_PSV_MCS,
    CMD_EXP_PSV_PSU,

// Import commands
    CMD_IMP_KEYSTONE,
    CMD_CREATE_ACT_DAT,

// SFO patches
    SFO_UNLOCK_COPY,
    SFO_CHANGE_ACCOUNT_ID,
    SFO_REMOVE_PSID,
    SFO_CHANGE_TITLE_ID,
};

// Save flags
#define SAVE_FLAG_LOCKED        1
#define SAVE_FLAG_OWNER         2
#define SAVE_FLAG_PS3           4
#define SAVE_FLAG_PS1           8
#define SAVE_FLAG_PS2           16
#define SAVE_FLAG_PSP           32
#define SAVE_FLAG_PSV           64
#define SAVE_FLAG_TROPHY        128
#define SAVE_FLAG_ONLINE        256
#define SAVE_FLAG_PS4           512
#define SAVE_FLAG_HDD           1024

enum save_type_enum
{
    FILE_TYPE_NULL,
    FILE_TYPE_PSV,
    FILE_TYPE_TRP,
    FILE_TYPE_MENU,
    FILE_TYPE_PS4,

    // PS1 File Types
    FILE_TYPE_PSX,
    FILE_TYPE_MCS,

    // PS2 File Types
    FILE_TYPE_PSU,
    FILE_TYPE_MAX,
    FILE_TYPE_CBS,
    FILE_TYPE_XPS,
    FILE_TYPE_VM2,
    FILE_TYPE_PS2RAW,

    // License Files
    FILE_TYPE_RIF,
    FILE_TYPE_RAP,
    FILE_TYPE_ACT,

    // ISO Files
    FILE_TYPE_ISO,
    FILE_TYPE_BINENC,
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
    CHAR_RES_TAB,
    CHAR_RES_LF,
    CHAR_TAG_TRANSFER,
    CHAR_TAG_ZIP,
    CHAR_RES_CR,
    CHAR_TAG_PCE,
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
    PATCH_COMMAND,
    PATCH_SFO,
    PATCH_TROP_UNLOCK,
    PATCH_TROP_LOCK,
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
    char* title;
    uint8_t icon_id;
    void (*UpdatePath)(char *);
    int (*ReadCodes)(save_entry_t *);
    list_t* (*ReadList)(const char*);
} save_list_t;

list_t * ReadUsbList(const char* userPath);
list_t * ReadUserList(const char* userPath);
list_t * ReadOnlineList(const char* urlPath);
list_t * ReadBackupList(const char* userPath);
list_t * ReadTrophyList(const char* userPath);
void UnloadGameList(list_t * list);
char * readTextFile(const char * path, long* size);
int sortSaveList_Compare(const void* A, const void* B);
int sortCodeList_Compare(const void* A, const void* B);
int ReadCodes(save_entry_t * save);
int ReadTrophies(save_entry_t * game);
int ReadOnlineSaves(save_entry_t * game);
int ReadBackupCodes(save_entry_t * bup);

int http_init(void);
void http_end(void);
int http_download(const char* url, const char* filename, const char* local_dst, int show_progress);

int extract_zip(const char* zip_file, const char* dest_path);
int zip_directory(const char* basedir, const char* inputdir, const char* output_zipfile);

int show_dialog(int dialog_type, const char * format, ...);
void init_progress_bar(const char* msg);
void update_progress_bar(uint64_t progress, const uint64_t total_size, const char* msg);
void end_progress_bar(void);
#define show_message(...)	show_dialog(0, __VA_ARGS__)

int init_loading_screen(const char* msg);
void stop_loading_screen();

void execCodeCommand(code_entry_t* code, const char* codecmd);

int patch_trophy_account(const char* trp_path, const char* account_id);
int apply_trophy_patch(const char* trp_path, uint32_t trophy_id, int unlock);

int regMgr_GetParentalPasscode(char* passcode);
int regMgr_GetUserName(int userNumber, char* outString);
int regMgr_GetAccountId(int userNumber, uint64_t* psnAccountId);
int regMgr_SetAccountId(int userNumber, uint64_t* psnAccountId);

int create_savegame_folder(const char* folder);
int get_save_details(const save_entry_t *save, char** details);
int orbis_SaveUmount(const char* mountPath);
int orbis_SaveMount(const save_entry_t *save, uint32_t mode, char* mountPath);
int orbis_UpdateSaveParams(const char* mountPath, const char* title, const char* subtitle, const char* details);

int psv_resign(const char *src_psv);

int ps1_mcs2psv(const char* save, const char* psv_path);
int ps1_psx2psv(const char* save, const char* psv_path);
int ps2_psu2psv(const char *save, const char* psv_path);
int ps2_max2psv(const char *save, const char* psv_path);
int ps2_cbs2psv(const char *save, const char *psv_path);
int ps2_xps2psv(const char *save, const char *psv_path);
int ps1_psv2mcs(const char* save, const char* mcs_path);
int ps2_psv2psu(const char *save, const char* psu_path);

char* sjis2utf8(char* input);
