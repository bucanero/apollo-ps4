#define APOLLO_VERSION          "1.4.0"     //Apollo PS4 version (about menu)

#define MENU_TITLE_OFF			45			//Offset of menu title text from menu mini icon
#define MENU_ICON_OFF 			105         //X Offset to start printing menu mini icon
#define MENU_ANI_MAX 			0x80        //Max animation number
#define MENU_SPLIT_OFF			400			//Offset from left of sub/split menu to start drawing
#define OPTION_ITEM_OFF         1095        //Offset from left of settings item/value

enum app_option_type
{
    APP_OPTION_NONE,
    APP_OPTION_BOOL,
    APP_OPTION_LIST,
    APP_OPTION_INC,
    APP_OPTION_CALL,
};

typedef struct
{
	char * name;
	char * * options;
	int type;
	uint8_t * value;
	void(*callback)(int);
} menu_option_t;

typedef struct
{
    char app_name[8];
    char app_ver[8];
    uint8_t music;
    uint8_t doSort;
    uint8_t doAni;
    uint8_t update;
    uint32_t user_id;
    uint64_t psid[2];
    uint64_t account_id;
    char save_db[256];
} app_config_t;

extern menu_option_t menu_options[];
extern app_config_t apollo_config;

void music_callback(int sel);
void update_callback(int sel);

int get_firmware_version();
int save_xml_owner(const char *xmlfile);
int read_xml_owner(const char *xmlfile, const char *owner);
int get_xml_owners(const char *xmlfile, int cmd, char*** nam, char*** val);
