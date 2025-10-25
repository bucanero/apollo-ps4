/* 
	Apollo PS4 main.c
*/

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <stdbool.h>
#include <orbis/Sysmodule.h>
#include <orbis/AudioOut.h>
#include <orbis/CommonDialog.h>
#include <orbis/Sysmodule.h>
#include <orbis/SystemService.h>
#include <mini18n.h>

#include "saves.h"
#include "sfo.h"
#include "util.h"
#include "common.h"
#include "orbisPad.h"

//Menus
#include "menu.h"
#include "menu_gui.h"

//Font
#include "libfont.h"
#include "ttf_render.h"
#include "font_adonais.h"
#include "font-10x20.h"

//Sound
#include <s3m.h>
#define SAMPLING_FREQ          48000 /* 48khz. */
#define AUDIO_SAMPLES          256   /* audio samples */

// Audio handle
static int32_t audio = 0;


#define load_menu_texture(name, type) \
			if (!LoadMenuTexture(APOLLO_APP_PATH "images/" #name "." #type , name##_##type##_index)) return 0;


void update_usb_path(char *p);
void update_hdd_path(char *p);
void update_trophy_path(char *p);
void update_db_path(char *p);
void update_vmc_path(char *p);

app_config_t apollo_config = {
    .app_name = "APOLLO",
    .app_ver = {0},
    .save_db = ONLINE_URL,
    .ftp_url = "",
    .music = 1,
    .doSort = 1,
    .doAni = 1,
    .update = 1,
    .online_opt = 0,
    .dbglog = 0,
    .usb_dev = (MAX_USB_DEVICES+1),
    .user_id = 0,
    .psid = {0, 0},
    .account_id = 0,
};

int close_app = 0;
int idle_time = 0;                          // Set by readPad

png_texture * menu_textures;                // png_texture array for main menu, initialized in LoadTexture
SDL_Window* window;                         // SDL window
SDL_Renderer* renderer;                     // SDL software renderer
uint32_t* texture_mem;                      // Pointers to texture memory
uint32_t* free_mem;                         // Pointer after last texture

/*
* HDD save list
*/
save_list_t hdd_saves = {
    .id = MENU_HDD_SAVES,
    .title = NULL,
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
    .id = MENU_USB_SAVES,
    .title = NULL,
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
    .id = MENU_TROPHIES,
    .title = NULL,
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
    .id = MENU_ONLINE_DB,
    .title = NULL,
    .list = NULL,
    .path = ONLINE_URL,
    .ReadList = &ReadOnlineList,
    .ReadCodes = &ReadOnlineSaves,
    .UpdatePath = &update_db_path,
};

/*
* User Backup code list
*/
save_list_t user_backup = {
    .id = MENU_USER_BACKUP,
    .title = NULL,
    .list = NULL,
    .path = "",
    .ReadList = &ReadBackupList,
    .ReadCodes = &ReadBackupCodes,
    .UpdatePath = NULL,
};

/*
* PS1 VMC list
*/
save_list_t vmc1_saves = {
    .id = MENU_PS1VMC_SAVES,
    .title = NULL,
    .list = NULL,
    .path = "",
    .ReadList = &ReadVmc1List,
    .ReadCodes = &ReadVmc1Codes,
    .UpdatePath = &update_vmc_path,
};

/*
* PS2 VMC list
*/
save_list_t vmc2_saves = {
    .id = MENU_PS2VMC_SAVES,
    .title = NULL,
    .list = NULL,
    .path = "",
    .ReadList = &ReadVmc2List,
    .ReadCodes = &ReadVmc2Codes,
    .UpdatePath = &update_vmc_path,
};

static const char* get_button_prompts(char* prompt)
{
	switch (menu_id)
	{
		case MENU_TROPHIES:
		case MENU_USB_SAVES:
		case MENU_HDD_SAVES:
		case MENU_ONLINE_DB:
		case MENU_PS1VMC_SAVES:
		case MENU_PS2VMC_SAVES:
			snprintf(prompt, 0xFF, "\x10 %s    \x13 %s    \x12 %s    \x11 %s", _("Select"), _("Back"), _("Details"), _("Refresh"));
			break;

		case MENU_USER_BACKUP:
			snprintf(prompt, 0xFF, "\x10 %s    \x13 %s    \x11 %s", _("Select"), _("Back"), _("Refresh"));
			break;

		case MENU_SETTINGS:
		case MENU_CODE_OPTIONS:
			snprintf(prompt, 0xFF, "\x10 %s    \x13 %s", _("Select"), _("Back"));
			break;

		case MENU_CREDITS:
		case MENU_PATCH_VIEW:
		case MENU_SAVE_DETAILS:
			snprintf(prompt, 0xFF, "\x13 %s", _("Back"));
			break;

		case MENU_PATCHES:
			snprintf(prompt, 0xFF, "\x10 %s    \x12 %s    \x13 %s", _("Select"), _("View Code"), _("Back"));
			break;

		case MENU_HEX_EDITOR:
			snprintf(prompt, 0xFF, "\x10 %s    \x11 %s   \x13 %s", _("Value Up"), _("Value Down"), _("Exit"));
			break;

		case MENU_MAIN_SCREEN:
		default:
			prompt[0] = 0;
			break;
	}

	return prompt;
}

static void helpFooter(void)
{
	char footer[256];
	u8 alpha = 0xFF;

	if (apollo_config.doAni && orbisPadGetConf()->idle > 0x100)
	{
		int dec = (orbisPadGetConf()->idle - 0x100) * 2;
		alpha = (dec > alpha) ? 0 : (alpha - dec);
	}

	SetFontSize(APP_FONT_SIZE_DESCRIPTION);
	SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
	SetFontColor(APP_FONT_COLOR | alpha, 0);
	DrawString(0, SCREEN_HEIGHT - 94, get_button_prompts(footer));
	SetFontAlign(FONT_ALIGN_LEFT);
}

static int initPad(void)
{
	if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_PAD) < 0)
		return 0;

	// Initialize the Pad library
	if (orbisPadInit() < 0)
	{
		LOG("[ERROR] Failed to initialize pad library!");
		return 0;
	}

	// Get the user ID
	apollo_config.user_id = orbisPadGetConf()->userId;

	return 1;
}

// Used only in initialization. Allocates 64 mb for textures and loads the font
static int LoadTextures_Menu(void)
{
	texture_mem = malloc(256 * 32 * 4);
	menu_textures = (png_texture *)calloc(TOTAL_MENU_TEXTURES, sizeof(png_texture));
	
	if(!texture_mem || !menu_textures)
		return 0; // fail!
	
	ResetFont();
	free_mem = (u32 *) AddFontFromBitmapArray((u8 *) data_font_Adonais, (u8 *) texture_mem, 0x20, 0x7e, 32, 31, 1, BIT7_FIRST_PIXEL);
	free_mem = (u32 *) AddFontFromBitmapArray((u8 *) console_font_10x20, (u8 *) free_mem, 0, 0xFF, 10, 20, 1, BIT7_FIRST_PIXEL);

	if (TTFLoadFont(0, "/preinst/common/font/DFHEI5-SONY.ttf", NULL, 0) != SUCCESS ||
		TTFLoadFont(1, "/system_ex/app/NPXS20113/bdjstack/lib/fonts/SCE-PS3-RD-R-LATIN.TTF", NULL, 0) != SUCCESS)
		return 0;

	free_mem = (u32*) init_ttf_table((u8*) free_mem);
	set_ttf_window(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, WIN_SKIP_LF);
	
	//Init Main Menu textures
	load_menu_texture(bgimg, jpg);
	load_menu_texture(cheat, png);
	load_menu_texture(leon_luna, jpg);

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
	load_menu_texture(tag_vmc, png);
	load_menu_texture(tag_ps1, png);
	load_menu_texture(tag_ps2, png);
	load_menu_texture(tag_ps3, png);
	load_menu_texture(tag_ps4, png);
	load_menu_texture(tag_psp, png);
	load_menu_texture(tag_psv, png);
	load_menu_texture(tag_warning, png);
	load_menu_texture(tag_zip, png);
	load_menu_texture(tag_net, png);
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

static int LoadSounds(void* data)
{
	s3m_t s3m;

	s3m_initialize(&s3m, SAMPLING_FREQ);
	// Decode a mp3 file to play
	if (s3m_load(&s3m, APOLLO_APP_PATH "audio/haiku.s3m") < 0)
	{
		LOG("[ERROR] Failed to decode audio file");
		return -1;
	}
	LOG("Loaded audio file: %s", s3m.header->song_name);

	// Calculate the sample count and allocate a buffer for the sample data accordingly
	uint8_t *pSampleData = (uint8_t*) malloc(AUDIO_SAMPLES * 2 * sizeof(int16_t));

	// Play the song in a loop
	while (!close_app)
	{
		if (!apollo_config.music)
		{
			usleep(0x1000);
			continue;
		}

		if (!s3m.rt.playing)
		{
			// If we reach the end of the file, seek back to the beginning.
			s3m_play(&s3m);
		}

		// Decode the wav into pSampleData
		s3m_sound_callback(NULL, pSampleData, AUDIO_SAMPLES * 2 * sizeof(int16_t));

		/* Output audio */
		sceAudioOutOutput(audio, NULL);	// NULL: wait for completion

		if (sceAudioOutOutput(audio, pSampleData) < 0)
		{
			LOG("Failed to output audio");
			return -1;
		}
	}

	s3m_stop(&s3m);
	s3m_unload(&s3m);
	free(pSampleData);

	return 0;
}

void update_usb_path(char* path)
{
	if (apollo_config.usb_dev < MAX_USB_DEVICES)
	{
		sprintf(path, USB_PATH, apollo_config.usb_dev);
		return;
	}

	if (apollo_config.usb_dev == MAX_USB_DEVICES)
	{
		sprintf(path, FAKE_USB_PATH);
		return;
	}

	for (int i = 0; i < MAX_USB_DEVICES; i++)
	{
		sprintf(path, USB_PATH ".apollo", i);

		FILE *fp = fopen(path, "w");
		if (!fp)
			continue;

		fclose(fp);
		remove(path);
		*strrchr(path, '.') = 0;

		return;
	}

	sprintf(path, FAKE_USB_PATH);
	if (dir_exists(path) == SUCCESS)
		return;

	path[0] = 0;
}

void update_hdd_path(char* path)
{
	sprintf(path, SAVES_DB_PATH, apollo_config.user_id);
}

void update_trophy_path(char* path)
{
	sprintf(path, TROPHY_DB_PATH, apollo_config.user_id);
}

void update_db_path(char* path)
{
	if (apollo_config.online_opt)
	{
		sprintf(path, "%s%016lX/", apollo_config.ftp_url, apollo_config.account_id);
		return;
	}

	strcpy(path, apollo_config.save_db);
}

void update_vmc_path(char* path)
{
	if (file_exists(path) == SUCCESS)
		return;

	path[0] = 0;
}

static void initLocalization(void)
{
	char path[256];

	snprintf(path, sizeof(path), APOLLO_DATA_PATH "lang_%s.po", get_user_language());
	if (mini18n_set_locale(path) != SUCCESS)
	{
		snprintf(path, sizeof(path), APOLLO_APP_PATH "misc/lang_%s.po", get_user_language());
		if (mini18n_set_locale(path) != SUCCESS)
			LOG("Localization file not found: %s", path);
	}

	hdd_saves.title = _("HDD Saves");
	usb_saves.title = _("USB Saves");
	trophies.title = _("Trophies");
	user_backup.title = _("User Tools");
	online_saves.title = _("Online Database");
	vmc1_saves.title = _("PS1 Virtual Memory Card");
	vmc2_saves.title = _("PS2 Virtual Memory Card");
}

static void registerSpecialChars(void)
{
	// Register save tags
	RegisterSpecialCharacter(CHAR_TAG_PS1, 0, 1.5, &menu_textures[tag_ps1_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PS2, 0, 1.5, &menu_textures[tag_ps2_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PS3, 2, 1.5, &menu_textures[tag_ps3_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PS4, 2, 1.5, &menu_textures[tag_ps4_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PSP, 2, 1.5, &menu_textures[tag_psp_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_PSV, 2, 1.5, &menu_textures[tag_psv_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_VMC, 2, 1.0, &menu_textures[tag_vmc_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_LOCKED, 0, 1.3, &menu_textures[tag_lock_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_OWNER, 0, 1.3, &menu_textures[tag_own_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_WARNING, 0, 1.3, &menu_textures[tag_warning_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_APPLY, 2, 1.0, &menu_textures[tag_apply_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_ZIP, 0, 1.0, &menu_textures[tag_zip_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_NET, 0, 1.0, &menu_textures[tag_net_png_index]);
	RegisterSpecialCharacter(CHAR_TAG_TRANSFER, 0, 1.0, &menu_textures[tag_transfer_png_index]);

	// Register button icons
	RegisterSpecialCharacter(orbisPadGetConf()->crossButtonOK ? CHAR_BTN_X : CHAR_BTN_O, 0, 1.2, &menu_textures[footer_ico_cross_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_S, 0, 1.2, &menu_textures[footer_ico_square_png_index]);
	RegisterSpecialCharacter(CHAR_BTN_T, 0, 1.2, &menu_textures[footer_ico_triangle_png_index]);
	RegisterSpecialCharacter(orbisPadGetConf()->crossButtonOK ? CHAR_BTN_O : CHAR_BTN_X, 0, 1.2, &menu_textures[footer_ico_circle_png_index]);

	// Register trophy icons
	RegisterSpecialCharacter(CHAR_TRP_BRONZE, 2, 0.9f, &menu_textures[trp_bronze_png_index]);
	RegisterSpecialCharacter(CHAR_TRP_SILVER, 2, 0.9f, &menu_textures[trp_silver_png_index]);
	RegisterSpecialCharacter(CHAR_TRP_GOLD, 2, 0.9f, &menu_textures[trp_gold_png_index]);
	RegisterSpecialCharacter(CHAR_TRP_PLATINUM, 0, 0.9f, &menu_textures[trp_platinum_png_index]);
	RegisterSpecialCharacter(CHAR_TRP_SYNC, 0, 0.9f, &menu_textures[trp_sync_png_index]);
}

static void terminate(void)
{
	LOG("Exiting...");
	sceAudioOutClose(audio);
	mini18n_close();
	terminate_jbc();
	sceSystemServiceLoadExec("exit", NULL);
}

static int initInternal(void)
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

	// Initialize SDL functions
	LOG("Initializing SDL");
	if (SDL_Init(SDL_INIT_VIDEO) != SUCCESS)
	{
		LOG("Failed to initialize SDL: %s", SDL_GetError());
		return (-1);
	}

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
	audio = sceAudioOutOpen(ORBIS_USER_SERVICE_USER_ID_SYSTEM, ORBIS_AUDIO_OUT_PORT_TYPE_MAIN, 0, AUDIO_SAMPLES, SAMPLING_FREQ, ORBIS_AUDIO_OUT_PARAM_FORMAT_S16_STEREO);

	if (audio <= 0)
	{
		LOG("[ERROR] Failed to open audio on main port");
		return audio;
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
	if (!initialize_jbc() || !initVshDataMount())
	{
		notify_popup(NOTIFICATION_ICON_BAN, "Failed to initialize jailbreak!");
		terminate();
	}

	mkdirs(APOLLO_DATA_PATH);
	mkdirs(APOLLO_LOCAL_CACHE);
	
	// Load freetype
	if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_FREETYPE_OL) < 0)
	{
		LOG("Failed to load freetype!");
		return (-1);
	}

	// Load MsgDialog
	if (sceSysmoduleLoadModule(ORBIS_SYSMODULE_MESSAGE_DIALOG) < 0 ||
		sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG) < 0)
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

	initLocalization();
	// Load application settings
	load_app_settings(&apollo_config);

	if (apollo_config.dbglog)
	{
		dbglogger_init_mode(FILE_LOGGER, APOLLO_PATH "apollo.log", 0);
		notify_popup(NOTIFICATION_ICON_DEFAULT, "%s\n%s", _("Debug Logging Enabled"), APOLLO_PATH "apollo.log");
	}

	// Unpack application data on first run
	if (strncmp(apollo_config.app_ver, APOLLO_VERSION, sizeof(apollo_config.app_ver)) != 0)
	{
		LOG("Unpacking application data...");
//		clean_directory(APOLLO_DATA_PATH);
		if (extract_zip(APOLLO_APP_PATH "misc/appdata.zip", APOLLO_DATA_PATH))
			notify_popup(NOTIFICATION_ICON_DEFAULT, _("Successfully installed local application data"));

		strncpy(apollo_config.app_ver, APOLLO_VERSION, sizeof(apollo_config.app_ver));
		save_app_settings(&apollo_config);
	}

	// dedicated to Leon & Luna ~ in loving memory

#ifndef APOLLO_ENABLE_LOGGING
	// Splash screen logo (fade-in)
	drawSplashLogo(1);
#endif

	// Setup font
	SetExtraSpace(-15);
	SetCurrentFont(font_adonais_regular);

	registerSpecialChars();
	initMenuOptions();

#ifndef APOLLO_ENABLE_LOGGING
	// Splash screen logo (fade-out)
	drawSplashLogo(-1);
#endif
	SDL_DestroyTexture(menu_textures[buk_scr_png_index].texture);
	
	//Set options
	update_callback(!apollo_config.update);

	// Start BGM audio thread
	SDL_CreateThread(&LoadSounds, "audio_thread", NULL);

#ifndef APOLLO_ENABLE_LOGGING
	Draw_MainMenu_Ani();
#endif

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

		orbisPadUpdate();
		drawScene();

		//Draw help
		if (menu_id)
			helpFooter();

#ifdef APOLLO_ENABLE_LOGGING
		// Calculate FPS and ms/frame
		SetFontColor(APP_FONT_COLOR | 0xFF, 0);
		DrawFormatString(50, 960, "FPS: %d", (1000 / deltaFrameTicks));
#endif
		// Propagate the updated window to the screen
		SDL_RenderPresent(renderer);
	}

	if (apollo_config.doAni)
		drawEndLogo();

    // Cleanup resources
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    // Stop all SDL sub-systems
    SDL_Quit();
	http_end();
	orbisPadFinish();

	return 0;
}
