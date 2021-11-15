/* 
	Apollo PS3 main.c
*/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <orbis/Pad.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>

#include "saves.h"
#include "sfo.h"
#include "pfd.h"
#include "util.h"
#include "common.h"

//Menus
#include "menu.h"
#include "menu_about.h"
#include "menu_options.h"
#include "menu_cheats.h"

enum menu_screen_ids
{
	MENU_MAIN_SCREEN,
	MENU_TROPHIES,
	MENU_USB_SAVES,
	MENU_HDD_SAVES,
	MENU_ONLINE_DB,
	MENU_USER_BACKUP,
	MENU_SETTINGS,
	MENU_CREDITS,
	MENU_PATCHES,
	MENU_PATCH_VIEW,
	MENU_CODE_OPTIONS,
	MENU_SAVE_DETAILS,
	TOTAL_MENU_IDS
};

//Font
#include "libfont.h"
#include "ttf_render.h"
#include "font_adonais.h"

//Sound
//#include <soundlib/audioplayer.h>

// SPU
u32 inited;
u32 spu = 0;

#define INITED_CALLBACK     1
#define INITED_SPU          2
#define INITED_SOUNDLIB     4
#define INITED_AUDIOPLAYER  8

#define SPU_SIZE(x) (((x)+127) & ~127)

#define load_menu_texture(name, type) \
	   LoadMenuTexture("/app0/assets/images/" #name "." #type , name##_##type##_index);


//Pad stuff
#define ANALOG_CENTER       0x78
#define ANALOG_THRESHOLD    0x68
#define ANALOG_MIN          (ANALOG_CENTER - ANALOG_THRESHOLD)
#define ANALOG_MAX          (ANALOG_CENTER + ANALOG_THRESHOLD)
#define PAD_DPAD            (ORBIS_PAD_BUTTON_UP | ORBIS_PAD_BUTTON_DOWN | ORBIS_PAD_BUTTON_LEFT | ORBIS_PAD_BUTTON_RIGHT)
#define PAD_REST            (ORBIS_PAD_BUTTON_CROSS | ORBIS_PAD_BUTTON_CIRCLE | ORBIS_PAD_BUTTON_SQUARE | \
                             ORBIS_PAD_BUTTON_TRIANGLE | ORBIS_PAD_BUTTON_L1 | ORBIS_PAD_BUTTON_L2 | \
							 ORBIS_PAD_BUTTON_R1 | ORBIS_PAD_BUTTON_R2 | ORBIS_PAD_BUTTON_OPTIONS)
#define MAX_PADS            1

int padhandle, pad_time = 0, rest_time = 0, pad_held_time = 0, rest_held_time = 0;
OrbisPadData paddata[MAX_PADS];
OrbisPadData padA[MAX_PADS];
OrbisPadData padB[MAX_PADS];


void update_usb_path(char *p);
void update_hdd_path(char *p);
void update_trophy_path(char *p);

app_config_t apollo_config = {
    .music = 1,
    .doSort = 1,
    .doAni = 1,
    .update = 1,
    .marginH = 0,
    .marginV = 0,
    .user_id = 0,
    .idps = {0, 0},
    .psid = {0, 0},
    .account_id = 0,
};

int menu_options_maxopt = 0;
int * menu_options_maxsel;

int close_app = 0;
int idle_time = 0;                          // Set by readPad

png_texture * menu_textures;                // png_texture array for main menu, initialized in LoadTexture


const char * menu_about_strings[] = { "Bucanero", "Developer",
									"Berion", "GUI design",
									"Dnawrkshp", "Artemis code",
									"flatz", "PFD/SFO tools",
									"aldostools", "Bruteforce Save Data",
									NULL, NULL };

char user_id_str[9] = "00000000";
char idps_str[SFO_PSID_SIZE*2+2] = "0000000000000000 0000000000000000";
char psid_str[SFO_PSID_SIZE*2+2] = "0000000000000000 0000000000000000";
char account_id_str[SFO_ACCOUNT_ID_SIZE+1] = "0000000000000000";

const char * menu_about_strings_project[] = { "User ID", user_id_str,
											"Account ID", account_id_str,
											idps_str, psid_str,
											NULL, NULL };

/*
* 0 - Main Menu
* 1 - Trophies
* 2 - USB Menu (User List)
* 3 - HDD Menu (User List)
* 4 - Online Menu (Online List)
* 5 - User Backup
* 6 - Options Menu
* 7 - About Menu
* 8 - Code Menu (Select Cheats)
* 9 - Code Menu (View Cheat)
* 10 - Code Menu (View Cheat Options)
*/
int menu_id = 0;												// Menu currently in
int menu_sel = 0;												// Index of selected item (use varies per menu)
int menu_old_sel[TOTAL_MENU_IDS] = { 0 };						// Previous menu_sel for each menu
int last_menu_id[TOTAL_MENU_IDS] = { 0 };						// Last menu id called (for returning)

const char * menu_pad_help[TOTAL_MENU_IDS] = { NULL,												//Main
								"\x10 Select    \x13 Back    \x12 Details    \x11 Refresh",			//Trophy list
								"\x10 Select    \x13 Back    \x12 Details    \x11 Refresh",			//USB list
								"\x10 Select    \x13 Back    \x12 Details    \x11 Refresh",			//HDD list
								"\x10 Select    \x13 Back    \x11 Refresh",							//Online list
								"\x10 Select    \x13 Back    \x11 Refresh",							//User backup
								"\x10 Select    \x13 Back",											//Options
								"\x13 Back",														//About
								"\x10 Select    \x12 View Code    \x13 Back",						//Select Cheats
								"\x13 Back",														//View Cheat
								"\x10 Select    \x13 Back",											//Cheat Option
								"\x13 Back",														//View Details
								};

/*
* HDD save list
*/
save_list_t hdd_saves = {
	.icon_id = cat_hdd_png_index,
	.title = "HDD Saves",
    .list = NULL,
    .path = "",
    .ReadList = &ReadUserList,
    .ReadCodes = &ReadCodes,
    .UpdatePath = &update_hdd_path,
};

/*
* USB save list
*/
save_list_t usb_saves = {
	.icon_id = cat_usb_png_index,
	.title = "USB Saves",
    .list = NULL,
    .path = "",
    .ReadList = &ReadUserList,
    .ReadCodes = &ReadCodes,
    .UpdatePath = &update_usb_path,
};

/*
* Trophy list
*/
save_list_t trophies = {
	.icon_id = cat_warning_png_index,
	.title = "Trophies",
    .list = NULL,
    .path = "",
    .ReadList = &ReadUserList,
    .ReadCodes = &ReadCodes,
//    .ReadList = &ReadTrophyList,
//    .ReadCodes = &ReadTrophies,
    .UpdatePath = &update_trophy_path,
};

/*
* Online code list
*/
save_list_t online_saves = {
	.icon_id = cat_db_png_index,
	.title = "Online Database",
    .list = NULL,
    .path = ONLINE_URL,
    .ReadList = &ReadOnlineList,
    .ReadCodes = &ReadOnlineSaves,
    .UpdatePath = NULL,
};

/*
* User Backup code list
*/
save_list_t user_backup = {
    .icon_id = cat_bup_png_index,
    .title = "User Data Backup",
    .list = NULL,
    .path = "",
    .ReadList = &ReadBackupList,
    .ReadCodes = &ReadBackupCodes,
    .UpdatePath = NULL,
};

save_entry_t* selected_entry;
code_entry_t* selected_centry;
int option_index = 0;

void release_all()
{
	/*
	if(inited & INITED_CALLBACK)
		sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);

	if(inited & INITED_AUDIOPLAYER)
		StopAudio();
	
	if(inited & INITED_SOUNDLIB)
		SND_End();

	if(inited & INITED_SPU) {
		sysSpuRawDestroy(spu);
		sysSpuImageClose(&spu_image);
	}
*/
	inited=0;
}

static void sys_callback(uint64_t status, uint64_t param, void* userdata)
{
	/*
	 switch (status) {
		case SYSUTIL_EXIT_GAME: //0x0101
			release_all();
			if (file_exists("/dev_hdd0/mms/db.err") == SUCCESS)
				sys_reboot();

			sysProcessExit(1);
			break;
	  
		case SYSUTIL_MENU_OPEN: //0x0131
			break;

		case SYSUTIL_MENU_CLOSE: //0x0132
			break;

	   default:
		   break;
		 
	}
	*/
}

int initPad(int controllerUserID)
{
	int userID;

    // Initialize the Pad library
    if (scePadInit() != 0)
    {
        LOG("[DEBUG] [ERROR] Failed to initialize pad library!");
        return 0;
    }

    // Get the user ID
    if (controllerUserID < 0)
    {
    	OrbisUserServiceInitializeParams param;
    	param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    	sceUserServiceInitialize(&param);
    	sceUserServiceGetInitialUser(&userID);
    }
    else
    {
    	userID = controllerUserID;
    }

    // Open a handle for the controller
    padhandle = scePadOpen(userID, 0, 0, NULL);

    if (padhandle < 0)
    {
        LOG("[DEBUG] [ERROR] Failed to open pad!");
        return 0;
    }
    
    return 1;
}

int readPad(int port)
{
	int off = 2;
	int retDPAD = 0, retREST = 0;
	u32 dpad = 0, _dpad = 0;
	u32 rest = 0, _rest = 0;

	scePadReadState(padhandle, &padA[port]);

//	if(padinfo.status[port])
	if(padA[port].connected || 1)
	{
//		ioPadGetData(port, &padA[port]);
		
		if (padA[port].leftStick.y < ANALOG_MIN)
			padA[port].buttons |= ORBIS_PAD_BUTTON_UP;
			
		if (padA[port].leftStick.y > ANALOG_MAX)
			padA[port].buttons |= ORBIS_PAD_BUTTON_DOWN;
			
		if (padA[port].leftStick.x < ANALOG_MIN)
			padA[port].buttons |= ORBIS_PAD_BUTTON_LEFT;
			
		if (padA[port].leftStick.x > ANALOG_MAX)
			padA[port].buttons |= ORBIS_PAD_BUTTON_RIGHT;

		//new
		dpad = padA[port].buttons & PAD_DPAD;
		rest = padA[port].buttons & PAD_REST;
		
		//old
		_dpad = padB[port].buttons & PAD_DPAD;
		_rest = padB[port].buttons & PAD_REST;
		
		if (dpad == 0 && rest == 0)
			idle_time++;
		else
			idle_time = 0;
		
		//Copy new to old
		memcpy(paddata, padA, sizeof(OrbisPadData));
		memcpy(padB, padA, sizeof(OrbisPadData));
		
		//DPad has 3 mode input delay
		if (dpad == _dpad && dpad != 0)
		{
			if (pad_time > 0) //dpad delay
			{
				pad_held_time++;
				pad_time--;
				retDPAD = 0;
			}
			else
			{
				//So the pad speeds up after a certain amount of time being held
				if (pad_held_time > 180)
				{
					pad_time = 2;
				}
				else if (pad_held_time > 60)
				{
					pad_time = 5;
				}
				else
					pad_time = 20;
				
				retDPAD = 1;
			}
		}
		else
		{
			pad_held_time = 0;
			pad_time = 0;
		}
		
		//rest has its own delay
		if (rest == _rest && rest != 0)
		{
			if (rest_time > 0) //rest delay
			{
				rest_held_time++;
				rest_time--;
				retREST = 0;
			}
			else
			{
				//So the pad speeds up after a certain amount of time being held
				if (rest_held_time > 180)
				{
					rest_time = 2;
				}
				else if (rest_held_time > 60)
				{
					rest_time = 5;
				}
				else
					rest_time = 20;
				
				retREST = 1;
			}
		}
		else
		{
			rest_held_time = 0;
			rest_time = 0;
		}
		
	}
	
	if (!retDPAD && !retREST)
		return 0;
	
	if (!retDPAD)
	{
		paddata[port].buttons |= PAD_DPAD;
		paddata[port].buttons &= ~PAD_DPAD;
//		paddata[port].buttons ^= ORBIS_PAD_BUTTON_UP;
//		paddata[port].buttons ^= ORBIS_PAD_BUTTON_DOWN;
//		paddata[port].buttons ^= ORBIS_PAD_BUTTON_LEFT;
//		paddata[port].buttons ^= ORBIS_PAD_BUTTON_RIGHT;
	}
	
	if (!retREST)
	{
		paddata[port].buttons |= PAD_REST;
		paddata[port].buttons &= ~PAD_REST;
/*
		paddata[port].buttons ^= ORBIS_PAD_BUTTON_CROSS;
		paddata[port].buttons ^= ORBIS_PAD_BUTTON_CIRCLE;
		paddata[port].buttons ^= ORBIS_PAD_BUTTON_SQUARE;
		paddata[port].buttons ^= ORBIS_PAD_BUTTON_TRIANGLE;
		paddata[port].buttons ^= ORBIS_PAD_BUTTON_L1;
		paddata[port].buttons ^= ORBIS_PAD_BUTTON_L2;
		paddata[port].buttons ^= ORBIS_PAD_BUTTON_R1;
		paddata[port].buttons ^= ORBIS_PAD_BUTTON_R2;
		paddata[port].buttons ^= ORBIS_PAD_BUTTON_OPTIONS;
	*/
/*
		paddata[port].BTN_SELECT = 0;
		paddata[port].BTN_L3 = 0;
		paddata[port].BTN_R3 = 0;
		paddata[port].BTN_START = 0;
*/
	}

	return 1;
}

	/*
void copyTexture(int cnt)
{
	// copy texture datas from PNG to the RSX memory allocated for textures
	if (menu_textures[cnt].texture.bmp_out)
	{
		memcpy(free_mem, menu_textures[cnt].texture.bmp_out, menu_textures[cnt].texture.pitch * menu_textures[cnt].texture.height);
		free(menu_textures[cnt].texture.bmp_out); // free the PNG because i don't need this datas
		menu_textures[cnt].texture_off = tiny3d_TextureOffset(free_mem);      // get the offset (RSX use offset instead address)
		free_mem += ((menu_textures[cnt].texture.pitch * menu_textures[cnt].texture.height + 15) & ~15) / 4; // aligned to 16 bytes (it is u32) and update the pointer
	}
}
	*/

void LoadMenuTexture(const char* path, int idx)
{
//	LOG("Loading texture (%s)...", path);
	menu_textures[idx].texture = orbis2dLoadImageFromSandBox(path);
//	pngLoadFromBuffer(menu_textures[idx].buffer, menu_textures[idx].size, &menu_textures[idx].texture);
//	copyTexture(idx);
	menu_textures[idx].size = 1;
	menu_textures[idx].buffer = NULL;
}

void LoadImageFontTexture(const u8* rawData, uint16_t unicode, int idx)
{
//	menu_textures[idx].size = LoadImageFontEntry(rawData, unicode, &menu_textures[idx].texture);
//	copyTexture(idx);
}

void LoadFileTexture(const char* fname, int idx)
{
	if (!menu_textures[idx].buffer)
		menu_textures[idx].buffer = free_mem;

//	pngLoadFromFile(fname, &menu_textures[idx].texture);
//	copyTexture(idx);

	menu_textures[idx].size = 1;
	free_mem = (u32*) menu_textures[idx].buffer;
}

// Used only in initialization. Allocates 64 mb for textures and loads the font
void LoadTextures_Menu()
{
//	texture_mem = tiny3d_AllocTexture(64*1024*1024); // alloc 64MB of space for textures (this pointer can be global)
	texture_mem = malloc(2048 * 32 * 32 * 4);
	
	if(!texture_mem) return; // fail!
	
	ResetFont();
	free_mem = (u32 *) AddFontFromBitmapArray((u8 *) data_font_Adonais, (u8 *) texture_mem, 0x20, 0x7e, 32, 31, 1, BIT7_FIRST_PIXEL);
	
	TTFUnloadFont();
	TTFLoadFont(0, "/data/apollo/SCE-PS3-SR-R-LATIN2.TTF", NULL, 0);
	TTFLoadFont(1, "/data/apollo/SCE-PS3-DH-R-CGB.TTF", NULL, 0);
	TTFLoadFont(2, "/data/apollo/SCE-PS3-SR-R-JPN.TTF", NULL, 0);
	TTFLoadFont(3, "/data/apollo/SCE-PS3-YG-R-KOR.TTF", NULL, 0);
/*
	TTFLoadFont(0, "/app0/assets/fonts/SCE-PS3-SR-R-LATIN2.TTF", NULL, 0);
	TTFLoadFont(1, "/app0/assets/fonts/SCE-PS3-DH-R-CGB.TTF", NULL, 0);
	TTFLoadFont(2, "/app0/assets/fonts/SCE-PS3-SR-R-JPN.TTF", NULL, 0);
	TTFLoadFont(3, "/app0/assets/fonts/SCE-PS3-YG-R-KOR.TTF", NULL, 0);
*/
	free_mem = (u32*) init_ttf_table((u16*) free_mem);
	
	set_ttf_window(0, 0, 848 + apollo_config.marginH, 512 + apollo_config.marginV, WIN_SKIP_LF);
//	TTFUnloadFont();
	
	if (!menu_textures)
		menu_textures = (png_texture *)malloc(sizeof(png_texture) * TOTAL_MENU_TEXTURES);
	
	//Init Main Menu textures

	load_menu_texture(bgimg, jpg);
	load_menu_texture(cheat, png);

	load_menu_texture(circle_loading_bg, png);
	load_menu_texture(circle_loading_seek, png);
	load_menu_texture(edit_shadow, png);

	load_menu_texture(footer_ico_circle, png);
	load_menu_texture(footer_ico_cross, png);
	load_menu_texture(footer_ico_square, png);
	load_menu_texture(footer_ico_triangle, png);
	load_menu_texture(header_dot, png);
	load_menu_texture(header_line, png);

	load_menu_texture(mark_arrow, png);
	load_menu_texture(mark_line, png);
	load_menu_texture(opt_off, png);
	load_menu_texture(opt_on, png);
	load_menu_texture(scroll_bg, png);
	load_menu_texture(scroll_lock, png);
	load_menu_texture(help, png);
	load_menu_texture(buk_scr, png);
	load_menu_texture(cat_about, png);
	load_menu_texture(cat_cheats, png);
	load_menu_texture(cat_opt, png);
	load_menu_texture(cat_usb, png);
	load_menu_texture(cat_bup, png);
	load_menu_texture(cat_db, png);
	load_menu_texture(cat_hdd, png);
	load_menu_texture(cat_sav, png);
	load_menu_texture(cat_warning, png);
	load_menu_texture(column_1, png);
	load_menu_texture(column_2, png);
	load_menu_texture(column_3, png);
	load_menu_texture(column_4, png);
	load_menu_texture(column_5, png);
	load_menu_texture(column_6, png);
	load_menu_texture(column_7, png);
	load_menu_texture(jar_about, png);
	load_menu_texture(jar_about_hover, png);
	load_menu_texture(jar_bup, png);
	load_menu_texture(jar_bup_hover, png);
	load_menu_texture(jar_db, png);
	load_menu_texture(jar_db_hover, png);
	load_menu_texture(jar_trophy, png);
	load_menu_texture(jar_trophy_hover, png);
	load_menu_texture(jar_hdd, png);
	load_menu_texture(jar_hdd_hover, png);
	load_menu_texture(jar_opt, png);
	load_menu_texture(jar_opt_hover, png);
	load_menu_texture(jar_usb, png);
	load_menu_texture(jar_usb_hover, png);
	load_menu_texture(logo, png);
	load_menu_texture(logo_text, png);
	load_menu_texture(tag_lock, png);
	load_menu_texture(tag_own, png);
	load_menu_texture(tag_pce, png);
	load_menu_texture(tag_ps1, png);
	load_menu_texture(tag_ps2, png);
	load_menu_texture(tag_ps3, png);
	load_menu_texture(tag_psp, png);
	load_menu_texture(tag_psv, png);
	load_menu_texture(tag_warning, png);
	load_menu_texture(tag_zip, png);
	load_menu_texture(tag_apply, png);
	load_menu_texture(tag_transfer, png);

/*
	u8* imagefont;
	if (read_buffer("/dev_flash/vsh/resource/imagefont.bin", &imagefont, NULL) == SUCCESS)
	{
		LoadImageFontTexture(imagefont, 0xF888, footer_ico_lt_png_index);
		LoadImageFontTexture(imagefont, 0xF88B, footer_ico_rt_png_index);
		LoadImageFontTexture(imagefont, 0xF6AD, trp_sync_img_index);
		LoadImageFontTexture(imagefont, 0xF8AC, trp_bronze_img_index);
		LoadImageFontTexture(imagefont, 0xF8AD, trp_silver_img_index);
		LoadImageFontTexture(imagefont, 0xF8AE, trp_gold_img_index);
		LoadImageFontTexture(imagefont, 0xF8AF, trp_platinum_img_index);

		free(imagefont);
	}
*/

	menu_textures[icon_png_file_index].buffer = NULL;
	LoadFileTexture(APOLLO_PATH "../ICON0.PNG", icon_png_file_index);

	u32 tBytes = free_mem - texture_mem;
	LOG("LoadTextures_Menu() :: Allocated %db (%.02fkb, %.02fmb) for textures", tBytes, tBytes / (float)1024, tBytes / (float)(1024 * 1024));
}

short *background_music = NULL;
int background_music_size = 48000*72*4; // initial size of buffer to decode (for 48000 samples x 72 seconds and 16 bit stereo as reference)
int effect_freq;
int effect_is_stereo;

void LoadSounds()
{
	/*
	//Initialize SPU
	u32 entry = 0;
	u32 segmentcount = 0;
	sysSpuSegment* segments;
	
	sysSpuInitialize(6, 5);
	sysSpuRawCreate(&spu, NULL);
	sysSpuElfGetInformation(spu_soundmodule_bin, &entry, &segmentcount);
	
	size_t segmentsize = sizeof(sysSpuSegment) * segmentcount;
	segments = (sysSpuSegment*)memalign(128, SPU_SIZE(segmentsize)); // must be aligned to 128 or it break malloc() allocations
	memset(segments, 0, segmentsize);

	sysSpuElfGetSegments(spu_soundmodule_bin, segments, segmentcount);
	sysSpuImageImport(&spu_image, spu_soundmodule_bin, 0);
	sysSpuRawImageLoad(spu, &spu_image);
	
	inited |= INITED_SPU;
	if(SND_Init(spu)==0)
		inited |= INITED_SOUNDLIB;
	
	background_music   = (short *) malloc(background_music_size);

	// decode the mp3 effect file included to memory. It stops by EOF or when samples exceed size_effects_samples
	DecodeAudio( (void *) background_music_mp3, background_music_mp3_size, background_music, &background_music_size, &effect_freq, &effect_is_stereo);

	// adjust the sound buffer sample correctly to the background_music_size
	// SPU dma works aligned to 128 bytes. SPU module is designed to read unaligned buffers and it is better thing aligned buffers)
	short *temp = (short *)memalign(128, SPU_SIZE(background_music_size));
	memcpy((void *) temp, (void *) background_music, background_music_size);
	free(background_music);
	background_music = temp;
	
	SND_Pause(0);
	*/
}

void update_usb_path(char* path)
{
	for (int i = 0; i <= MAX_USB_DEVICES; i++)
	{
		sprintf(path, USB_PATH, i);

		if (dir_exists(path) == SUCCESS)
			return;
	}

	strcpy(path, "");
}

void update_hdd_path(char* path)
{
	sprintf(path, USER_PATH_HDD, apollo_config.user_id);
}

void update_trophy_path(char* path)
{
	sprintf(path, TROPHY_PATH_HDD, apollo_config.user_id);
}

int ReloadUserSaves(save_list_t* save_list)
{
    init_loading_screen("Loading save games...");

	if (save_list->list)
	{
		UnloadGameList(save_list->list);
		save_list->list = NULL;
	}

	if (save_list->UpdatePath)
		save_list->UpdatePath(save_list->path);

	save_list->list = save_list->ReadList(save_list->path);
	if (apollo_config.doSort)
		list_bubbleSort(save_list->list, &sortSaveList_Compare);

    stop_loading_screen();

	if (!save_list->list)
	{
		show_message("No save-games found");
		return 0;
	}

	return list_count(save_list->list);
}

code_entry_t* LoadRawPatch()
{
	size_t len;
	char patchPath[256];
	code_entry_t* centry = calloc(1, sizeof(code_entry_t));

	centry->name = strdup(selected_entry->title_id);
	snprintf(patchPath, sizeof(patchPath), APOLLO_DATA_PATH "%s.ps3savepatch", selected_entry->title_id);
	read_buffer(patchPath, (u8**) &centry->codes, &len);
	centry->codes[len] = 0;

	return centry;
}

code_entry_t* LoadSaveDetails()
{
	char sfoPath[256];
	code_entry_t* centry = calloc(1, sizeof(code_entry_t));

	centry->name = strdup(selected_entry->title_id);

	if (!(selected_entry->flags & SAVE_FLAG_PS3))
	{
		asprintf(&centry->codes, "%s\n\nTitle: %s\n", selected_entry->path, selected_entry->name);
		return(centry);
	}

	snprintf(sfoPath, sizeof(sfoPath), "%s" "PARAM.SFO", selected_entry->path);
	LOG("Save Details :: Reading %s...", sfoPath);

	sfo_context_t* sfo = sfo_alloc();
	if (sfo_read(sfo, sfoPath) < 0) {
		LOG("Unable to read from '%s'", sfoPath);
		sfo_free(sfo);
		return centry;
	}

	if (selected_entry->flags & SAVE_FLAG_TROPHY)
	{
		char* account = (char*) sfo_get_param_value(sfo, "ACCOUNTID");
		asprintf(&centry->codes, "%s\n\n"
			"Title: %s\n"
			"NP Comm ID: %s\n"
			"Account ID: %.16s\n", selected_entry->path, selected_entry->name, selected_entry->title_id, account);
		LOG(centry->codes);

		sfo_free(sfo);
		return(centry);
	}

	char* subtitle = (char*) sfo_get_param_value(sfo, "SUB_TITLE");
	sfo_params_ids_t* param_ids = (sfo_params_ids_t*)(sfo_get_param_value(sfo, "PARAMS") + 0x1C);
	param_ids->user_id = ES32(param_ids->user_id);

    asprintf(&centry->codes, "%s\n\n"
        "Title: %s\n"
        "Sub-Title: %s\n"
        "Lock: %s\n\n"
        "User ID: %08d\n"
        "Account ID: %.16s (%s)\n"
        "PSID: %016lX %016lX\n", selected_entry->path, selected_entry->name, subtitle, 
        (selected_entry->flags & SAVE_FLAG_LOCKED ? "Copying Prohibited" : "Unlocked"),
        param_ids->user_id, param_ids->account_id, 
        (selected_entry->flags & SAVE_FLAG_OWNER ? "Owner" : "Not Owner"),
		param_ids->psid[0], param_ids->psid[1]);
	LOG(centry->codes);

	sfo_free(sfo);
	return (centry);
}

void SetMenu(int id)
{   
	switch (menu_id) //Leaving menu
	{
		case MENU_MAIN_SCREEN: //Main Menu
		case MENU_TROPHIES:
		case MENU_USB_SAVES: //USB Saves Menu
		case MENU_HDD_SAVES: //HHD Saves Menu
		case MENU_ONLINE_DB: //Cheats Online Menu
		case MENU_USER_BACKUP: //Backup Menu
			menu_textures[icon_png_file_index].size = 0;
			break;

		case MENU_SETTINGS: //Options Menu
		case MENU_CREDITS: //About Menu
		case MENU_PATCHES: //Cheat Selection Menu
			break;

		case MENU_SAVE_DETAILS:
		case MENU_PATCH_VIEW: //Cheat View Menu
			if (apollo_config.doAni)
				Draw_CheatsMenu_View_Ani_Exit();
			break;

		case MENU_CODE_OPTIONS: //Cheat Option Menu
			if (apollo_config.doAni)
				Draw_CheatsMenu_Options_Ani_Exit();
			break;
	}
	
	switch (id) //going to menu
	{
		case MENU_MAIN_SCREEN: //Main Menu
			if (apollo_config.doAni || menu_id == MENU_MAIN_SCREEN) //if load animation
				Draw_MainMenu_Ani();
			break;

		case MENU_TROPHIES: //Trophies Menu
			if (!trophies.list && !ReloadUserSaves(&trophies))
				return;

			if (apollo_config.doAni)
				Draw_UserCheatsMenu_Ani(&trophies);
			break;

		case MENU_USB_SAVES: //USB saves Menu
			if (!usb_saves.list && !ReloadUserSaves(&usb_saves))
				return;
			
			if (apollo_config.doAni)
				Draw_UserCheatsMenu_Ani(&usb_saves);
			break;

		case MENU_HDD_SAVES: //HDD saves Menu
			if (!hdd_saves.list)
				ReloadUserSaves(&hdd_saves);
			
			if (apollo_config.doAni)
				Draw_UserCheatsMenu_Ani(&hdd_saves);
			break;

		case MENU_ONLINE_DB: //Cheats Online Menu
			if (!online_saves.list)
				ReloadUserSaves(&online_saves);

			if (apollo_config.doAni)
				Draw_UserCheatsMenu_Ani(&online_saves);
			break;

		case MENU_CREDITS: //About Menu
			// set to display the PSID on the About menu
			sprintf(idps_str, "%016lX %016lX", apollo_config.idps[0], apollo_config.idps[1]);
			sprintf(psid_str, "%016lX %016lX", apollo_config.psid[0], apollo_config.psid[1]);
			sprintf(user_id_str, "%08d", apollo_config.user_id);
			sprintf(account_id_str, "%016lx", apollo_config.account_id);

			if (apollo_config.doAni)
				Draw_AboutMenu_Ani();
			break;

		case MENU_SETTINGS: //Options Menu
			if (apollo_config.doAni)
				Draw_OptionsMenu_Ani();
			break;

		case MENU_USER_BACKUP: //User Backup Menu
			if (!user_backup.list)
				ReloadUserSaves(&user_backup);

			if (apollo_config.doAni)
				Draw_UserCheatsMenu_Ani(&user_backup);
			break;

		case MENU_PATCHES: //Cheat Selection Menu
			//if entering from game list, don't keep index, otherwise keep
			if (menu_id == MENU_USB_SAVES || menu_id == MENU_HDD_SAVES || menu_id == MENU_ONLINE_DB || menu_id == MENU_TROPHIES)
				menu_old_sel[MENU_PATCHES] = 0;

			char iconfile[256];
			snprintf(iconfile, sizeof(iconfile), "%s" "ICON0.PNG", selected_entry->path);

			if (selected_entry->flags & SAVE_FLAG_ONLINE)
			{
				snprintf(iconfile, sizeof(iconfile), APOLLO_TMP_PATH "%s.PNG", selected_entry->title_id);

				if (file_exists(iconfile) != SUCCESS)
;//					http_download(selected_entry->path, "ICON0.PNG", iconfile, 0);
			}

			if (file_exists(iconfile) == SUCCESS)
				LoadFileTexture(iconfile, icon_png_file_index);
			else
				menu_textures[icon_png_file_index].size = 0;

			if (apollo_config.doAni && menu_id != MENU_PATCH_VIEW && menu_id != MENU_CODE_OPTIONS)
				Draw_CheatsMenu_Selection_Ani();
			break;

		case MENU_PATCH_VIEW: //Cheat View Menu
			menu_old_sel[MENU_PATCH_VIEW] = 0;
			if (apollo_config.doAni)
				Draw_CheatsMenu_View_Ani("Patch view");
			break;

		case MENU_SAVE_DETAILS: //Save Detail View Menu
			if (apollo_config.doAni)
				Draw_CheatsMenu_View_Ani(selected_entry->name);
			break;

		case MENU_CODE_OPTIONS: //Cheat Option Menu
			menu_old_sel[MENU_CODE_OPTIONS] = 0;
			if (apollo_config.doAni)
				Draw_CheatsMenu_Options_Ani();
			break;
	}
	
	menu_old_sel[menu_id] = menu_sel;
	if (last_menu_id[menu_id] != id)
		last_menu_id[id] = menu_id;
	menu_id = id;
	
	menu_sel = menu_old_sel[menu_id];
}

void move_letter_back(list_t * games)
{
	int i;
	save_entry_t *game = list_get_item(games, menu_sel);
	char ch = toupper(game->name[0]);

	if ((ch > '\x29') && (ch < '\x40'))
	{
		menu_sel = 0;
		return;
	}

	for (i = menu_sel; (i > 0) && (ch == toupper(game->name[0])); i--)
	{
		game = list_get_item(games, i-1);
	}

	menu_sel = i;
}

void move_letter_fwd(list_t * games)
{
	int i;
	int game_count = list_count(games) - 1;
	save_entry_t *game = list_get_item(games, menu_sel);
	char ch = toupper(game->name[0]);

	if (ch == 'Z')
	{
		menu_sel = game_count;
		return;
	}
	
	for (i = menu_sel; (i < game_count) && (ch == toupper(game->name[0])); i++)
	{
		game = list_get_item(games, i+1);
	}

	menu_sel = i;
}

void move_selection_back(int game_count, int steps)
{
	menu_sel -= steps;
	if ((menu_sel == -1) && (steps == 1))
		menu_sel = game_count - 1;
	else if (menu_sel < 0)
		menu_sel = 0;
}

void move_selection_fwd(int game_count, int steps)
{
	menu_sel += steps;
	if ((menu_sel == game_count) && (steps == 1))
		menu_sel = 0;
	else if (menu_sel >= game_count)
		menu_sel = game_count - 1;
}

void doSaveMenu(save_list_t * save_list)
{
    if(readPad(0))
    {
    	if(paddata[0].buttons & ORBIS_PAD_BUTTON_UP)
    		move_selection_back(list_count(save_list->list), 1);
    
    	else if(paddata[0].buttons & ORBIS_PAD_BUTTON_DOWN)
    		move_selection_fwd(list_count(save_list->list), 1);
    
    	else if (paddata[0].buttons & ORBIS_PAD_BUTTON_LEFT)
    		move_selection_back(list_count(save_list->list), 5);
    
    	else if (paddata[0].buttons & ORBIS_PAD_BUTTON_L1)
    		move_selection_back(list_count(save_list->list), 25);
    
    	else if (paddata[0].buttons & ORBIS_PAD_BUTTON_L2)
    		move_letter_back(save_list->list);
    
    	else if (paddata[0].buttons & ORBIS_PAD_BUTTON_RIGHT)
    		move_selection_fwd(list_count(save_list->list), 5);
    
    	else if (paddata[0].buttons & ORBIS_PAD_BUTTON_R1)
    		move_selection_fwd(list_count(save_list->list), 25);
    
    	else if (paddata[0].buttons & ORBIS_PAD_BUTTON_R2)
    		move_letter_fwd(save_list->list);
    
    	else if (paddata[0].buttons & ORBIS_PAD_BUTTON_CIRCLE)
    	{
    		SetMenu(MENU_MAIN_SCREEN);
    		return;
    	}
    	else if (paddata[0].buttons & ORBIS_PAD_BUTTON_CROSS)
    	{
			selected_entry = list_get_item(save_list->list, menu_sel);

    		if (!selected_entry->codes && !save_list->ReadCodes(selected_entry))
    		{
    			show_message("No data found in folder:\n%s", selected_entry->path);
    			return;
    		}

    		if (apollo_config.doSort && 
				((save_list->icon_id == cat_bup_png_index) || (save_list->icon_id == cat_db_png_index)))
    			list_bubbleSort(selected_entry->codes, &sortCodeList_Compare);

    		SetMenu(MENU_PATCHES);
    		return;
    	}
    	else if (paddata[0].buttons & ORBIS_PAD_BUTTON_TRIANGLE && save_list->UpdatePath)
    	{
			selected_entry = list_get_item(save_list->list, menu_sel);
			selected_centry = LoadSaveDetails();
    		SetMenu(MENU_SAVE_DETAILS);
    		return;
    	}
		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_SQUARE)
		{
			ReloadUserSaves(save_list);
		}
	}

	Draw_UserCheatsMenu(save_list, menu_sel, 0xFF);
}

void doMainMenu()
{
	// Check the pads.
	if (readPad(0))
	{
		if(paddata[0].buttons & ORBIS_PAD_BUTTON_LEFT)
			move_selection_back(MENU_CREDITS, 1);

		else if(paddata[0].buttons & ORBIS_PAD_BUTTON_RIGHT)
			move_selection_fwd(MENU_CREDITS, 1);

		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_CROSS)
		    SetMenu(menu_sel+1);

		else if(paddata[0].buttons & ORBIS_PAD_BUTTON_CIRCLE && show_dialog(1, "Exit to XMB?"))
			close_app = 1;
	}
	
	Draw_MainMenu();
}

void doAboutMenu()
{
	// Check the pads.
	if (readPad(0))
	{
		if (paddata[0].buttons & ORBIS_PAD_BUTTON_CIRCLE)
		{
			SetMenu(MENU_MAIN_SCREEN);
			return;
		}
	}

	Draw_AboutMenu();
}

void doOptionsMenu()
{
	// Check the pads.
	if (readPad(0))
	{
		if(paddata[0].buttons & ORBIS_PAD_BUTTON_UP)
			move_selection_back(menu_options_maxopt, 1);

		else if(paddata[0].buttons & ORBIS_PAD_BUTTON_DOWN)
			move_selection_fwd(menu_options_maxopt, 1);

		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_CIRCLE)
		{
//			save_app_settings(&apollo_config);
			set_ttf_window(0, 0, 848 + apollo_config.marginH, 512 + apollo_config.marginV, WIN_SKIP_LF);
			SetMenu(MENU_MAIN_SCREEN);
			return;
		}
		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_LEFT)
		{
			if (menu_options[menu_sel].type == APP_OPTION_LIST)
			{
				if (*menu_options[menu_sel].value > 0)
					(*menu_options[menu_sel].value)--;
				else
					*menu_options[menu_sel].value = menu_options_maxsel[menu_sel] - 1;
			}
			else if (menu_options[menu_sel].type == APP_OPTION_INC)
				(*menu_options[menu_sel].value)--;
			
			if (menu_options[menu_sel].type != APP_OPTION_CALL)
				menu_options[menu_sel].callback(*menu_options[menu_sel].value);
		}
		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_RIGHT)
		{
			if (menu_options[menu_sel].type == APP_OPTION_LIST)
			{
				if (*menu_options[menu_sel].value < (menu_options_maxsel[menu_sel] - 1))
					*menu_options[menu_sel].value += 1;
				else
					*menu_options[menu_sel].value = 0;
			}
			else if (menu_options[menu_sel].type == APP_OPTION_INC)
				*menu_options[menu_sel].value += 1;

			if (menu_options[menu_sel].type != APP_OPTION_CALL)
				menu_options[menu_sel].callback(*menu_options[menu_sel].value);
		}
		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_CROSS)
		{
			if (menu_options[menu_sel].type == APP_OPTION_BOOL)
				menu_options[menu_sel].callback(*menu_options[menu_sel].value);

			else if (menu_options[menu_sel].type == APP_OPTION_CALL)
				menu_options[menu_sel].callback(0);
		}
	}
	
	Draw_OptionsMenu();
}

int count_code_lines()
{
	//Calc max
	int max = 0;
	const char * str;

	for(str = selected_centry->codes; *str; ++str)
		max += (*str == '\n');

	if (max <= 0)
		max = 1;

	return max;
}

void doPatchViewMenu()
{
	int max = count_code_lines();
	
	// Check the pads.
	if (readPad(0))
	{
		if(paddata[0].buttons & ORBIS_PAD_BUTTON_UP)
			move_selection_back(max, 1);

		else if(paddata[0].buttons & ORBIS_PAD_BUTTON_DOWN)
			move_selection_fwd(max, 1);

		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_CIRCLE)
		{
			SetMenu(last_menu_id[MENU_PATCH_VIEW]);
			return;
		}
	}
	
	Draw_CheatsMenu_View("Patch view");
}

void doCodeOptionsMenu()
{
    code_entry_t* code = list_get_item(selected_entry->codes, menu_old_sel[last_menu_id[MENU_CODE_OPTIONS]]);
	// Check the pads.
	if (readPad(0))
	{
		if(paddata[0].buttons & ORBIS_PAD_BUTTON_UP)
			move_selection_back(selected_centry->options[option_index].size, 1);

		else if(paddata[0].buttons & ORBIS_PAD_BUTTON_DOWN)
			move_selection_fwd(selected_centry->options[option_index].size, 1);

		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_CIRCLE)
		{
			code->activated = 0;
			SetMenu(last_menu_id[MENU_CODE_OPTIONS]);
			return;
		}
		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_CROSS)
		{
			code->options[option_index].sel = menu_sel;

			if (code->type == PATCH_COMMAND)
				execCodeCommand(code, code->options[option_index].value[menu_sel]);

			option_index++;
			
			if (option_index >= code->options_count)
			{
				SetMenu(last_menu_id[MENU_CODE_OPTIONS]);
				return;
			}
			else
				menu_sel = 0;
		}
	}
	
	Draw_CheatsMenu_Options();
}

void doSaveDetailsMenu()
{
	int max = count_code_lines();

	// Check the pads.
	if (readPad(0))
	{
		if(paddata[0].buttons & ORBIS_PAD_BUTTON_UP)
			move_selection_back(max, 1);

		else if(paddata[0].buttons & ORBIS_PAD_BUTTON_DOWN)
			move_selection_fwd(max, 1);

		if (paddata[0].buttons & ORBIS_PAD_BUTTON_CIRCLE)
		{
			if (selected_centry->name)
				free(selected_centry->name);
			if (selected_centry->codes)
				free(selected_centry->codes);
			free(selected_centry);

			SetMenu(last_menu_id[MENU_SAVE_DETAILS]);
			return;
		}
	}
	
	Draw_CheatsMenu_View(selected_entry->name);
}

void doPatchMenu()
{
	// Check the pads.
	if (readPad(0))
	{
		if(paddata[0].buttons & ORBIS_PAD_BUTTON_UP)
			move_selection_back(list_count(selected_entry->codes), 1);

		else if(paddata[0].buttons & ORBIS_PAD_BUTTON_DOWN)
			move_selection_fwd(list_count(selected_entry->codes), 1);

		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_LEFT)
			move_selection_back(list_count(selected_entry->codes), 5);

		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_RIGHT)
			move_selection_fwd(list_count(selected_entry->codes), 5);

		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_L1)
			move_selection_back(list_count(selected_entry->codes), 25);

		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_R1)
			move_selection_fwd(list_count(selected_entry->codes), 25);

		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_CIRCLE)
		{
			SetMenu(last_menu_id[MENU_PATCHES]);
			return;
		}
		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_CROSS)
		{
			selected_centry = list_get_item(selected_entry->codes, menu_sel);

			if (selected_centry->type != PATCH_NULL)
				selected_centry->activated = !selected_centry->activated;

			if (selected_centry->type == PATCH_COMMAND)
				execCodeCommand(selected_centry, selected_centry->codes);

			if (selected_centry->activated)
			{
				// Only activate Required codes if a cheat is selected
				if (selected_centry->type == PATCH_GAMEGENIE || selected_centry->type == PATCH_BSD)
				{
					code_entry_t* code;
					list_node_t* node;

					for (node = list_head(selected_entry->codes); (code = list_get(node)); node = list_next(node))
						if (wildcard_match_icase(code->name, "*(REQUIRED)*"))
							code->activated = 1;
				}
				/*
				if (!selected_centry->options)
				{
					int size;
					selected_entry->codes[menu_sel].options = ReadOptions(selected_entry->codes[menu_sel], &size);
					selected_entry->codes[menu_sel].options_count = size;
				}
				*/
				
				if (selected_centry->options)
				{
					option_index = 0;
					SetMenu(MENU_CODE_OPTIONS);
					return;
				}

				if (selected_centry->codes[0] == CMD_VIEW_RAW_PATCH)
				{
					selected_centry->activated = 0;
					selected_centry = LoadRawPatch();
					SetMenu(MENU_SAVE_DETAILS);
					return;
				}

				if (selected_centry->codes[0] == CMD_VIEW_DETAILS)
				{
					selected_centry->activated = 0;
					selected_centry = LoadSaveDetails();
					SetMenu(MENU_SAVE_DETAILS);
					return;
				}
			}
		}
		else if (paddata[0].buttons & ORBIS_PAD_BUTTON_TRIANGLE)
		{
			selected_centry = list_get_item(selected_entry->codes, menu_sel);

			if (selected_centry->type == PATCH_GAMEGENIE || selected_centry->type == PATCH_BSD ||
				(selected_entry->type == FILE_TYPE_TRP && selected_entry->flags & SAVE_FLAG_TROPHY))
			{
				SetMenu(MENU_PATCH_VIEW);
				return;
			}
		}
	}
	
	Draw_CheatsMenu_Selection(menu_sel, 0xFFFFFFFF);
}

// Resets new frame
void drawScene()
{
	switch (menu_id)
	{
		case MENU_MAIN_SCREEN:
			doMainMenu();
			break;

		case MENU_TROPHIES: //Trophies Menu
			doSaveMenu(&trophies);
			break;

		case MENU_USB_SAVES: //USB Saves Menu
			doSaveMenu(&usb_saves);
			break;

		case MENU_HDD_SAVES: //HDD Saves Menu
			doSaveMenu(&hdd_saves);
			break;

		case MENU_ONLINE_DB: //Online Cheats Menu
			doSaveMenu(&online_saves);
			break;

		case MENU_CREDITS: //About Menu
			doAboutMenu();
			break;

		case MENU_SETTINGS: //Options Menu
			doOptionsMenu();
			break;

		case MENU_USER_BACKUP: //User Backup Menu
			doSaveMenu(&user_backup);
			break;

		case MENU_PATCHES: //Cheats Selection Menu
			doPatchMenu();
			break;

		case MENU_PATCH_VIEW: //Cheat View Menu
			doPatchViewMenu();
			break;

		case MENU_CODE_OPTIONS: //Cheat Option Menu
			doCodeOptionsMenu();
			break;

		case MENU_SAVE_DETAILS: //Save Details Menu
			doSaveDetailsMenu();
			break;
	}
}

void exiting()
{
//	http_end();
//	sysModuleUnload(SYSMODULE_PNGDEC);
}

void registerSpecialChars()
{
	// Register save tags
	RegisterSpecialCharacter(CHAR_TAG_PS1, 2, 1.5, &menu_textures[tag_ps1_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PS2, 2, 1.5, &menu_textures[tag_ps2_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PS3, 2, 1.5, &menu_textures[tag_ps3_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PSP, 2, 1.5, &menu_textures[tag_psp_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PSV, 2, 1.5, &menu_textures[tag_psv_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PCE, 2, 1.5, &menu_textures[tag_pce_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_LOCKED, 0, 1.5, &menu_textures[tag_lock_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_OWNER, 0, 1.5, &menu_textures[tag_own_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_WARNING, 0, 1.5, &menu_textures[tag_warning_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_APPLY, 2, 1.1, &menu_textures[tag_apply_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_ZIP, 0, 1.2, &menu_textures[tag_zip_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_TRANSFER, 0, 1.2, &menu_textures[tag_transfer_png_index]);

	// Register button icons
	RegisterSpecialCharacter(CHAR_BTN_X, 0, 1.2, &menu_textures[footer_ico_cross_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_S, 0, 1.2, &menu_textures[footer_ico_square_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_T, 0, 1.2, &menu_textures[footer_ico_triangle_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_O, 0, 1.2, &menu_textures[footer_ico_circle_png_index]);

	// Register trophy icons
	RegisterSpecialCharacter(CHAR_TRP_BRONZE, 2, 1.0, &menu_textures[trp_bronze_img_index]);
	RegisterSpecialCharacter(CHAR_TRP_SILVER, 2, 1.0, &menu_textures[trp_silver_img_index]);
	RegisterSpecialCharacter(CHAR_TRP_GOLD, 2, 1.0, &menu_textures[trp_gold_img_index]);
	RegisterSpecialCharacter(CHAR_TRP_PLATINUM, 0, 1.2, &menu_textures[trp_platinum_img_index]);
	RegisterSpecialCharacter(CHAR_TRP_SYNC, 0, 1.2, &menu_textures[trp_sync_img_index]);
}

int64_t flipArg=0;

/*
	Program start
*/
s32 main(s32 argc, const char* argv[])
{
#ifdef APOLLO_ENABLE_LOGGING
	dbglogger_init();
//	dbglogger_failsafe("9999");
#endif

//	http_init();

//	tiny3d_Init(1024*1024);
    orbis2dInit();

//	ioPadInit(7);
	initPad(-1);
	
	// Load freetype
	if (sceSysmoduleLoadModule(0x009A) < 0)
	{
		LOG("Failed to load freetype!");
		return (-1);
	}

	atexit(exiting); // Tiny3D register the event 3 and do exit() call when you exit  to the menu

	// register exit callback
//	if(sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sys_callback, NULL)==0) inited |= INITED_CALLBACK;
	
	// Load texture
	LoadTextures_Menu();
	LoadSounds();

	LOG("x1");

	// Unpack application data on first run
	if (file_exists(APOLLO_LOCAL_CACHE "appdata.zip") == SUCCESS)
	{
		clean_directory(APOLLO_DATA_PATH);
		unzip_app_data(APOLLO_LOCAL_CACHE "appdata.zip");
	}

	LOG("22");

	// Splash screen logo (fade-in)
//	drawSplashLogo(1, &flipArg);

	// Load application settings
//	load_app_settings(&apollo_config);

//	if (file_exists(APOLLO_PATH OWNER_XML_FILE) == SUCCESS)
//		save_xml_owner(APOLLO_PATH OWNER_XML_FILE, NULL);

	LOG("333");

//-
        char** retm = calloc(1, sizeof(char*) * 2);
        retm[0] = strdup("DEFAULT_USERNAME");

	menu_options[8].options = retm;
//-
 
	// Setup font
	SetExtraSpace(5);
	SetCurrentFont(0);

	LOG("555");

	registerSpecialChars();

	menu_options_maxopt = 0;
	while (menu_options[menu_options_maxopt].name)
		menu_options_maxopt++;
	
	menu_options_maxsel = (int *)calloc(1, menu_options_maxopt * sizeof(int));
	
	for (int i = 0; i < menu_options_maxopt; i++)
	{
		menu_options_maxsel[i] = 0;
		if (menu_options[i].type == APP_OPTION_LIST)
		{
			while (menu_options[i].options[menu_options_maxsel[i]])
				menu_options_maxsel[i]++;
		}
	}

	LOG("666");

	// Splash screen logo (fade-out)
//	drawSplashLogo(-1, &flipArg);

//	SND_SetInfiniteVoice(2, (effect_is_stereo) ? VOICE_STEREO_16BIT : VOICE_MONO_16BIT, effect_freq, 0, background_music, background_music_size, 255, 255);
	
	//Set options
	music_callback(!apollo_config.music);
	update_callback(!apollo_config.update);
	*menu_options[8].value = menu_options_maxsel[8] - 1;

	SetMenu(MENU_MAIN_SCREEN);

	LOG("77");

 	dbglogger_init_mode(TCP_LOGGER, "192.168.1.249", 19999);

/*
FC3: BF6723C1 / 23d93d8a4bd443f719dd086b2e9c9c07
ABC: 2109CF2B
MvC3:703B3123 <xor> 8FC4CEDC

*/

	while (!close_app)
	{
		//wait for current display buffer
		orbis2dStartDrawing();

		// clear the current display buffer
		orbis2dClearBuffer(1);  // (don't use cached dumpBuf)

/*
		tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);

		// Enable alpha Test
		tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);

		// Enable alpha blending.
		tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
			TINY3D_BLEND_FUNC_SRC_ALPHA_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_RGB_ONE_MINUS_SRC_ALPHA,
			TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
		
		// change to 2D context (remember you it works with 848 x 512 as virtual coordinates)
		tiny3d_Project2D();
*/
		drawScene();

#ifdef APOLLO_ENABLE_LOGGING
		if(paddata[0].buttons & ORBIS_PAD_BUTTON_OPTIONS)
		{
			LOG("Screen");
//			dbglogger_screenshot_tmp(0);
			LOG("Shot");
		}
#endif

		//Draw help
		if (menu_pad_help[menu_id])
		{
			u8 alpha = 0xFF;
			if (idle_time > 80)
			{
				int dec = (idle_time - 80) * 4;
				if (dec > alpha)
					dec = alpha;
				alpha -= dec;
			}
			
			SetFontSize(APP_FONT_SIZE_DESCRIPTION);
			SetCurrentFont(0);
			SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
			SetFontColor(APP_FONT_COLOR | alpha, 0);
			DrawString(0, 480, (char *)menu_pad_help[menu_id]);
			SetFontAlign(FONT_ALIGN_LEFT);
		}

		//flush and flip
		orbis2dFinishDrawing(flipArg);

		//swap buffers
		orbis2dSwapBuffers();
		flipArg++;
//		tiny3d_Flip();
	}

	release_all();

	return 0;
}
