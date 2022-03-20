/* 
	Apollo PS4 main.c
*/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include <orbis/Pad.h>
#include <orbis/Sysmodule.h>
#include <orbis/UserService.h>
#include <orbis/AudioOut.h>
#include <orbis/CommonDialog.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>

#include "saves.h"
#include "sfo.h"
#include "util.h"
#include "common.h"
#include "notifi.h"

//Menus
#include "menu.h"
#include "menu_gui.h"

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
#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"

// Audio handle
int32_t audio = 0;


#define load_menu_texture(name, type) \
			if (!LoadMenuTexture(APOLLO_APP_PATH "images/" #name "." #type , name##_##type##_index)) return 0;


//Pad stuff
#define ANALOG_CENTER       0x78
#define ANALOG_THRESHOLD    0x68
#define ANALOG_MIN          (ANALOG_CENTER - ANALOG_THRESHOLD)
#define ANALOG_MAX          (ANALOG_CENTER + ANALOG_THRESHOLD)
#define MAX_PADS            1

int padhandle;
pad_input_t pad_data;
OrbisPadData padA[MAX_PADS];


void drawScene();
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
									"", "",
									"PS3", "credits",
									"Berion", "GUI design",
									"Dnawrkshp", "Artemis code",
									"aldostools", "Bruteforce Save Data",
									NULL, NULL };

char user_id_str[9] = "00000000";
char psid_str[SFO_PSID_SIZE*2+2] = "0000000000000000 0000000000000000";
char account_id_str[SFO_ACCOUNT_ID_SIZE*2+1] = "0000000000000000";

const char * menu_about_strings_project[] = { "User ID", user_id_str,
											"Account ID", account_id_str,
											"Console PSID", psid_str,
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
    .ReadList = &ReadUsbList,
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
    .ReadList = &ReadTrophyList,
    .ReadCodes = &ReadTrophies,
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


int initPad()
{
	int userID;

	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_PAD) < 0)
		return 0;

    // Initialize the Pad library
    if (scePadInit() != 0)
    {
        LOG("[ERROR] Failed to initialize pad library!");
        return 0;
    }

    // Get the user ID
	OrbisUserServiceInitializeParams param;
	param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
	sceUserServiceInitialize(&param);
	sceUserServiceGetInitialUser(&userID);

    // Open a handle for the controller
    padhandle = scePadOpen(userID, 0, 0, NULL);
	apollo_config.user_id = userID;

    if (padhandle < 0)
    {
        LOG("[ERROR] Failed to open pad!");
        return 0;
    }
    
    return 1;
}

int g_padSync = 0;

int pad_sync()
{
	for (g_padSync++; g_padSync;);
	return 1;
}

int pad_input_update(void *data)
{
	pad_input_t* input = data;
	int button_frame_count = 0;

	while (!close_app)
	{
		scePadReadState(padhandle, &padA[0]);

		if(padA[0].connected && g_padSync)
		{
			uint32_t previous = input->down;

			input->down = padA[0].buttons;
			g_padSync = 0;

			if (!input->down)
				input->idle += 10;
			else
				input->idle = 0;

			if (padA[0].leftStick.y < ANALOG_MIN)
				input->down |= ORBIS_PAD_BUTTON_UP;

			if (padA[0].leftStick.y > ANALOG_MAX)
				input->down |= ORBIS_PAD_BUTTON_DOWN;

			if (padA[0].leftStick.x < ANALOG_MIN)
				input->down |= ORBIS_PAD_BUTTON_LEFT;

			if (padA[0].leftStick.x > ANALOG_MAX)
				input->down |= ORBIS_PAD_BUTTON_RIGHT;

			input->pressed = input->down & ~previous;
			input->active = input->pressed;

			if (input->down == previous)
			{
				if (button_frame_count > 30)
				{
					input->active = input->down;
				}
				button_frame_count++;
			}
			else
			{
				button_frame_count = 0;
			}
		}
	}
	
    return 1;
}

int pad_check_button(uint32_t button)
{
	if (pad_data.pressed & button)
	{
		pad_data.pressed ^= button;
		return 1;
	}

	return 0;
}

void LoadFileTexture(const char* fname, int idx)
{
	LOG("Loading '%s'", fname);
	if (menu_textures[idx].texture)
		SDL_DestroyTexture(menu_textures[idx].texture);

	menu_textures[idx].size = 0;
	menu_textures[idx].texture = NULL;
	LoadMenuTexture(fname, idx);
}

// Used only in initialization. Allocates 64 mb for textures and loads the font
int LoadTextures_Menu()
{
	texture_mem = malloc(256 * 32 * 32 * 4);
	
	if(!texture_mem)
		return 0; // fail!
	
	ResetFont();
	free_mem = (u32 *) AddFontFromBitmapArray((u8 *) data_font_Adonais, (u8 *) texture_mem, 0x20, 0x7e, 32, 31, 1, BIT7_FIRST_PIXEL);
	
	if (TTFLoadFont(0, "/preinst/common/font/DFHEI5-SONY.ttf", NULL, 0) != SUCCESS ||
		TTFLoadFont(1, "/system_ex/app/NPXS20113/bdjstack/lib/fonts/SCE-PS3-RD-R-LATIN.TTF", NULL, 0) != SUCCESS)
		return 0;
	free_mem = (u32*) init_ttf_table((u8*) free_mem);

	set_ttf_window(0, 0, SCREEN_WIDTH + apollo_config.marginH, SCREEN_HEIGHT + apollo_config.marginV, WIN_SKIP_LF);
	
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
	load_menu_texture(tag_ps4, png);
	load_menu_texture(tag_psp, png);
	load_menu_texture(tag_psv, png);
	load_menu_texture(tag_warning, png);
	load_menu_texture(tag_zip, png);
	load_menu_texture(tag_apply, png);
	load_menu_texture(tag_transfer, png);

	load_menu_texture(trp_sync, png);
	load_menu_texture(trp_bronze, png);
	load_menu_texture(trp_silver, png);
	load_menu_texture(trp_gold, png);
	load_menu_texture(trp_platinum, png);

	menu_textures[icon_png_file_index].texture = NULL;

	u32 tBytes = free_mem - texture_mem;
	LOG("LoadTextures_Menu() :: Allocated %db (%.02fkb, %.02fmb) for textures", tBytes, tBytes / (float)1024, tBytes / (float)(1024 * 1024));
	return 1;
}

int LoadSounds(void* data)
{
	uint8_t* play_audio = data;
	int32_t sOffs = 0;
	drmp3 wav;

	// Decode a mp3 file to play
	if (!drmp3_init_file(&wav, APOLLO_APP_PATH "audio/background_music.mp3", NULL))
	{
		LOG("[ERROR] Failed to decode audio file");
		return -1;
	}

	// Calculate the sample count and allocate a buffer for the sample data accordingly
	size_t sampleCount = drmp3_get_pcm_frame_count(&wav) * wav.channels;
	drmp3_int16 *pSampleData = (drmp3_int16 *)malloc(sampleCount * sizeof(uint16_t));

	// Decode the wav into pSampleData  wav.totalPCMFrameCount
	drmp3_read_pcm_frames_s16(&wav, drmp3_get_pcm_frame_count(&wav), pSampleData);

	// Play the song in a loop
	while (!close_app)
	{
		if (*play_audio == 0)
			continue;

		/* Output audio */
		sceAudioOutOutput(audio, NULL);	// NULL: wait for completion

		if (sceAudioOutOutput(audio, pSampleData + sOffs) < 0)
		{
			LOG("Failed to output audio");
			return -1;
		}

		sOffs += 256 * 2;

		if (sOffs >= sampleCount)
			sOffs = 0;
	}

	free(pSampleData);
	drmp3_uninit(&wav);

	return 0;
}

void update_usb_path(char* path)
{
	for (int i = 0; i <= MAX_USB_DEVICES; i++)
	{
		sprintf(path, USB_PATH "PS4/", i);

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
	snprintf(patchPath, sizeof(patchPath), APOLLO_DATA_PATH "%s.savepatch", selected_entry->title_id);
	read_buffer(patchPath, (u8**) &centry->codes, &len);
	centry->codes[len] = 0;

	return centry;
}

code_entry_t* LoadSaveDetails()
{
	code_entry_t* centry = calloc(1, sizeof(code_entry_t));
	centry->name = strdup(selected_entry->title_id);

	if (!get_save_details(selected_entry, &centry->codes))
		asprintf(&centry->codes, "Error getting details (%s)", selected_entry->name);

	LOG("%s", centry->codes);
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
			if (!online_saves.list && !ReloadUserSaves(&online_saves))
				return;

			if (apollo_config.doAni)
				Draw_UserCheatsMenu_Ani(&online_saves);
			break;

		case MENU_CREDITS: //About Menu
			// set to display the PSID on the About menu
			sprintf(psid_str, "%016lX %016lX", apollo_config.psid[0], apollo_config.psid[1]);
			sprintf(user_id_str, "%08x", apollo_config.user_id);
			sprintf(account_id_str, "%016lx", apollo_config.account_id);

			if (apollo_config.doAni)
				Draw_AboutMenu_Ani();
			break;

		case MENU_SETTINGS: //Options Menu
			if (apollo_config.doAni)
				Draw_OptionsMenu_Ani();
			break;

		case MENU_USER_BACKUP: //User Backup Menu
			if (!user_backup.list && !ReloadUserSaves(&user_backup))
				return;

			if (apollo_config.doAni)
				Draw_UserCheatsMenu_Ani(&user_backup);
			break;

		case MENU_PATCHES: //Cheat Selection Menu
			//if entering from game list, don't keep index, otherwise keep
			if (menu_id == MENU_USB_SAVES || menu_id == MENU_HDD_SAVES || menu_id == MENU_ONLINE_DB || menu_id == MENU_TROPHIES)
				menu_old_sel[MENU_PATCHES] = 0;

			char iconfile[256];
			snprintf(iconfile, sizeof(iconfile), "%s" "sce_sys/icon0.png", selected_entry->path);

			if (selected_entry->flags & SAVE_FLAG_ONLINE)
			{
				snprintf(iconfile, sizeof(iconfile), APOLLO_LOCAL_CACHE "%s.PNG", selected_entry->title_id);

				if (file_exists(iconfile) != SUCCESS)
					http_download(selected_entry->path, "icon0.png", iconfile, 0);
			}
			else if (selected_entry->flags & SAVE_FLAG_HDD)
				snprintf(iconfile, sizeof(iconfile), PS4_SAVES_PATH_HDD "%s/%s_icon0.png", apollo_config.user_id, selected_entry->title_id, selected_entry->dir_name);

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
    if (pad_sync())
    {
    	if(pad_data.active & ORBIS_PAD_BUTTON_UP)
    		move_selection_back(list_count(save_list->list), 1);
    
    	else if(pad_data.active & ORBIS_PAD_BUTTON_DOWN)
    		move_selection_fwd(list_count(save_list->list), 1);
    
    	else if (pad_data.active & ORBIS_PAD_BUTTON_LEFT)
    		move_selection_back(list_count(save_list->list), 5);
    
    	else if (pad_data.active & ORBIS_PAD_BUTTON_L1)
    		move_selection_back(list_count(save_list->list), 25);
    
    	else if (pad_data.active & ORBIS_PAD_BUTTON_L2)
    		move_letter_back(save_list->list);
    
    	else if (pad_data.active & ORBIS_PAD_BUTTON_RIGHT)
    		move_selection_fwd(list_count(save_list->list), 5);
    
    	else if (pad_data.active & ORBIS_PAD_BUTTON_R1)
    		move_selection_fwd(list_count(save_list->list), 25);
    
    	else if (pad_data.active & ORBIS_PAD_BUTTON_R2)
    		move_letter_fwd(save_list->list);
    
    	else if (pad_check_button(ORBIS_PAD_BUTTON_CIRCLE))
    	{
    		SetMenu(MENU_MAIN_SCREEN);
    		return;
    	}
    	else if (pad_check_button(ORBIS_PAD_BUTTON_CROSS))
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
    	else if (pad_check_button(ORBIS_PAD_BUTTON_TRIANGLE) && save_list->UpdatePath)
    	{
			selected_entry = list_get_item(save_list->list, menu_sel);
			if (selected_entry->type != FILE_TYPE_MENU)
			{
				selected_centry = LoadSaveDetails();
				SetMenu(MENU_SAVE_DETAILS);
				return;
			}
		}
		else if (pad_check_button(ORBIS_PAD_BUTTON_SQUARE))
		{
			ReloadUserSaves(save_list);
		}
	}

	Draw_UserCheatsMenu(save_list, menu_sel, 0xFF);
}

void doMainMenu()
{
	// Check the pads.
	if (pad_sync())
	{
		if(pad_data.active & ORBIS_PAD_BUTTON_LEFT)
			move_selection_back(MENU_CREDITS, 1);

		else if(pad_data.active & ORBIS_PAD_BUTTON_RIGHT)
			move_selection_fwd(MENU_CREDITS, 1);

		else if (pad_check_button(ORBIS_PAD_BUTTON_CROSS))
		{
		    SetMenu(menu_sel+1);
			drawScene();
			return;
		}

		else if(pad_check_button(ORBIS_PAD_BUTTON_CIRCLE) && show_dialog(1, "Exit to XMB?"))
			close_app = 1;
	}
	
	Draw_MainMenu();
}

void doAboutMenu()
{
	// Check the pads.
	if (pad_sync())
	{
		if (pad_check_button(ORBIS_PAD_BUTTON_CIRCLE))
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
	if (pad_sync())
	{
		if(pad_data.active & ORBIS_PAD_BUTTON_UP)
			move_selection_back(menu_options_maxopt, 1);

		else if(pad_data.active & ORBIS_PAD_BUTTON_DOWN)
			move_selection_fwd(menu_options_maxopt, 1);

		else if (pad_check_button(ORBIS_PAD_BUTTON_CIRCLE))
		{
			save_app_settings(&apollo_config);
			set_ttf_window(0, 0, SCREEN_WIDTH + apollo_config.marginH, SCREEN_HEIGHT + apollo_config.marginV, WIN_SKIP_LF);
			SetMenu(MENU_MAIN_SCREEN);
			return;
		}
		else if (pad_data.active & ORBIS_PAD_BUTTON_LEFT)
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
		else if (pad_data.active & ORBIS_PAD_BUTTON_RIGHT)
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
		else if (pad_check_button(ORBIS_PAD_BUTTON_CROSS))
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
	if (pad_sync())
	{
		if(pad_data.active & ORBIS_PAD_BUTTON_UP)
			move_selection_back(max, 1);

		else if(pad_data.active & ORBIS_PAD_BUTTON_DOWN)
			move_selection_fwd(max, 1);

		else if (pad_check_button(ORBIS_PAD_BUTTON_CIRCLE))
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
	if (pad_sync())
	{
		if(pad_data.active & ORBIS_PAD_BUTTON_UP)
			move_selection_back(selected_centry->options[option_index].size, 1);

		else if(pad_data.active & ORBIS_PAD_BUTTON_DOWN)
			move_selection_fwd(selected_centry->options[option_index].size, 1);

		else if (pad_check_button(ORBIS_PAD_BUTTON_CIRCLE))
		{
			code->activated = 0;
			SetMenu(last_menu_id[MENU_CODE_OPTIONS]);
			return;
		}
		else if (pad_check_button(ORBIS_PAD_BUTTON_CROSS))
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
	if (pad_sync())
	{
		if(pad_data.active & ORBIS_PAD_BUTTON_UP)
			move_selection_back(max, 1);

		else if(pad_data.active & ORBIS_PAD_BUTTON_DOWN)
			move_selection_fwd(max, 1);

		if (pad_check_button(ORBIS_PAD_BUTTON_CIRCLE))
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
	if (pad_sync())
	{
		if(pad_data.active & ORBIS_PAD_BUTTON_UP)
			move_selection_back(list_count(selected_entry->codes), 1);

		else if(pad_data.active & ORBIS_PAD_BUTTON_DOWN)
			move_selection_fwd(list_count(selected_entry->codes), 1);

		else if (pad_data.active & ORBIS_PAD_BUTTON_LEFT)
			move_selection_back(list_count(selected_entry->codes), 5);

		else if (pad_data.active & ORBIS_PAD_BUTTON_RIGHT)
			move_selection_fwd(list_count(selected_entry->codes), 5);

		else if (pad_data.active & ORBIS_PAD_BUTTON_L1)
			move_selection_back(list_count(selected_entry->codes), 25);

		else if (pad_data.active & ORBIS_PAD_BUTTON_R1)
			move_selection_fwd(list_count(selected_entry->codes), 25);

		else if (pad_check_button(ORBIS_PAD_BUTTON_CIRCLE))
		{
			SetMenu(last_menu_id[MENU_PATCHES]);
			return;
		}
		else if (pad_check_button(ORBIS_PAD_BUTTON_CROSS))
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
		else if (pad_check_button(ORBIS_PAD_BUTTON_TRIANGLE))
		{
			selected_centry = list_get_item(selected_entry->codes, menu_sel);

			if (selected_centry->type == PATCH_GAMEGENIE || selected_centry->type == PATCH_BSD ||
				selected_centry->type == PATCH_TROP_LOCK || selected_centry->type == PATCH_TROP_UNLOCK)
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

void registerSpecialChars()
{
	// Register save tags
	RegisterSpecialCharacter(CHAR_TAG_PS1, 2, 1.5, &menu_textures[tag_ps1_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PS2, 2, 1.5, &menu_textures[tag_ps2_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PS3, 2, 1.5, &menu_textures[tag_ps3_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PS4, 2, 1.5, &menu_textures[tag_ps4_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PSP, 2, 1.5, &menu_textures[tag_psp_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PSV, 2, 1.5, &menu_textures[tag_psv_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PCE, 2, 1.5, &menu_textures[tag_pce_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_LOCKED, 0, 1.3, &menu_textures[tag_lock_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_OWNER, 0, 1.3, &menu_textures[tag_own_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_WARNING, 0, 1.3, &menu_textures[tag_warning_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_APPLY, 2, 1.0, &menu_textures[tag_apply_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_ZIP, 0, 1.0, &menu_textures[tag_zip_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_TRANSFER, 0, 1.0, &menu_textures[tag_transfer_png_index]);

	// Register button icons
	RegisterSpecialCharacter(CHAR_BTN_X, 0, 1.2, &menu_textures[footer_ico_cross_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_S, 0, 1.2, &menu_textures[footer_ico_square_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_T, 0, 1.2, &menu_textures[footer_ico_triangle_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_O, 0, 1.2, &menu_textures[footer_ico_circle_png_index]);

	// Register trophy icons
	RegisterSpecialCharacter(CHAR_TRP_BRONZE, 2, 0.9f, &menu_textures[trp_bronze_png_index]);
	RegisterSpecialCharacter(CHAR_TRP_SILVER, 2, 0.9f, &menu_textures[trp_silver_png_index]);
	RegisterSpecialCharacter(CHAR_TRP_GOLD, 2, 0.9f, &menu_textures[trp_gold_png_index]);
	RegisterSpecialCharacter(CHAR_TRP_PLATINUM, 0, 0.9f, &menu_textures[trp_platinum_png_index]);
	RegisterSpecialCharacter(CHAR_TRP_SYNC, 0, 1.2f, &menu_textures[trp_sync_png_index]);
}

void terminate()
{
	LOG("Exiting...");
	// Unload loaded libraries
	if (unpatch_SceShellCore())
		notifi(NULL, "PS4 Save patches removed from memory");

	terminate_jbc();
	sceSystemServiceLoadExec("exit", NULL);
}

int appdata_check(const char* vfile)
{
	uint32_t version = 0;

	return (read_file(vfile, (uint8_t*) &version, sizeof(uint32_t)) != SUCCESS ||  version < APOLLO_DATA_VERSION);
}

static int initInternal()
{
    // load common modules
    int ret = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SYSTEM_SERVICE);
    if (ret != SUCCESS) {
        LOG("load module failed: SYSTEM_SERVICE (0x%08x)\n", ret);
        return 0;
    }

    ret = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_USER_SERVICE);
    if (ret != SUCCESS) {
        LOG("load module failed: USER_SERVICE (0x%08x)\n", ret);
        return 0;
    }

    ret = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_SAVE_DATA);
    if (ret != SUCCESS) {
        LOG("load module failed: SAVE_DATA (0x%08x)\n", ret);
        return 0;
    }

    return 1;
}


/*
	Program start
*/
s32 main(s32 argc, const char* argv[])
{
#ifdef APOLLO_ENABLE_LOGGING
	// Frame tracking info for debugging
	uint32_t lastFrameTicks  = 0;
	uint32_t startFrameTicks = 0;
	uint32_t deltaFrameTicks = 0;

	dbglogger_init();
#endif

	initInternal();
	http_init();
	initPad();

	// Initialize audio output library
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_AUDIOOUT) < 0 ||
		sceAudioOutInit() != SUCCESS)
	{
		LOG("[ERROR] Failed to initialize audio output");
		return (-1);
	}

	// Open a handle to audio output device
	audio = sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, 256, 48000, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);

	if (audio <= 0)
	{
		LOG("[ERROR] Failed to open audio on main port");
		return audio;
	}

	// Initialize SDL functions
	LOG("Initializing SDL");

	if (SDL_Init(SDL_INIT_VIDEO) != SUCCESS)
	{
		LOG("Failed to initialize SDL: %s", SDL_GetError());
		return (-1);
	}

	// Create a window context
	LOG( "Creating a window");
	window = SDL_CreateWindow("main", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, 0);
	if (!window) {
		LOG("SDL_CreateWindow: %s", SDL_GetError());
		return (-1);
	}

	// Create a renderer (OpenGL ES2)
	renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	if (!renderer) {
		LOG("SDL_CreateRenderer: %s", SDL_GetError());
		return (-1);
	}

	// Initialize jailbreak
	if (!initialize_jbc())
		terminate();

	mkdirs(APOLLO_DATA_PATH);
	mkdirs(APOLLO_LOCAL_CACHE);
	
	// Load freetype
	if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) < 0)
	{
		LOG("Failed to load freetype!");
		return (-1);
	}

	// Load MsgDialog
	if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG) < 0)
	{
		LOG("Failed to load dialog!");
		return (-1);
	}

	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG) < 0 ||
		sceCommonDialogInitialize() < 0)
	{
		LOG("Failed to init CommonDialog!");
		return (-1);
	}

	// register exit callback
	atexit(terminate);
	
	// Load texture
	if (!LoadTextures_Menu())
	{
		LOG("Failed to load menu textures!");
		return (-1);
	}

	// Unpack application data on first run
	if (appdata_check(APOLLO_DATA_PATH "version.dat"))
	{
//		clean_directory(APOLLO_DATA_PATH);
		if (extract_zip(APOLLO_APP_PATH "misc/appdata.zip", APOLLO_DATA_PATH))
			show_message("Successfully installed local application data");
	}

	// Splash screen logo (fade-in)
	drawSplashLogo(1);

	// Apply save-mounter patches
	patch_save_libraries();

	// Load application settings
	load_app_settings(&apollo_config);

	menu_options[8].options = get_logged_users();
 
	// Setup font
	SetExtraSpace(-15);
	SetCurrentFont(0);

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

	// Splash screen logo (fade-out)
	drawSplashLogo(-1);
	SDL_DestroyTexture(menu_textures[buk_scr_png_index].texture);
	
	//Set options
	music_callback(!apollo_config.music);
	update_callback(!apollo_config.update);

	SetMenu(MENU_MAIN_SCREEN);

	SDL_CreateThread(&pad_input_update, "input_thread", &pad_data);
	SDL_CreateThread(&LoadSounds, "audio_thread", &apollo_config.music);

	while (!close_app)
	{
#ifdef APOLLO_ENABLE_LOGGING
        startFrameTicks = SDL_GetTicks();
        deltaFrameTicks = startFrameTicks - lastFrameTicks;
        lastFrameTicks  = startFrameTicks;
#endif
		// Clear the canvas
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
		SDL_RenderClear(renderer);

		drawScene();

		//Draw help
		if (menu_pad_help[menu_id])
		{
			u8 alpha = 0xFF;
			if (pad_data.idle > 80)
			{
				int dec = (pad_data.idle - 80) * 4;
				if (dec > alpha)
					dec = alpha;
				alpha -= dec;
			}
			
			SetFontSize(APP_FONT_SIZE_DESCRIPTION);
			SetCurrentFont(0);
			SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
			SetFontColor(APP_FONT_COLOR | alpha, 0);
			DrawString(0, SCREEN_HEIGHT - 94, (char *)menu_pad_help[menu_id]);
			SetFontAlign(FONT_ALIGN_LEFT);
		}

#ifdef APOLLO_ENABLE_LOGGING
		// Calculate FPS and ms/frame
		SetFontColor(APP_FONT_COLOR | 0xFF, 0);
		DrawFormatString(50, 960, "FPS: %d", (1000 / deltaFrameTicks));
#endif
		// Propagate the updated window to the screen
		SDL_RenderPresent(renderer);
	}

	drawEndLogo();

    // Cleanup resources
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    // Stop all SDL sub-systems
    SDL_Quit();
	http_end();

	return 0;
}
