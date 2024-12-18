#ifndef __ARTEMIS_MENU_H__
#define __ARTEMIS_MENU_H__

#include <SDL2/SDL.h>

#include "types.h"
#include "settings.h"

// SDL window and software renderer
extern SDL_Window* window;
extern SDL_Renderer* renderer;

enum menu_screen_ids
{
	MENU_MAIN_SCREEN,		/* 0 - Main Menu */
	MENU_TROPHIES,			/* 1 - Trophy Menu */
	MENU_USB_SAVES,			/* 2 - USB Menu (User List) */
	MENU_HDD_SAVES,			/* 3 - HDD Menu (User List) */
	MENU_ONLINE_DB,			/* 4 - Online Menu (Online List) */
	MENU_USER_BACKUP,		/* 5 - User Backup */
	MENU_SETTINGS,			/* 6 - Options Menu */
	MENU_CREDITS,			/* 7 - About Menu */
	MENU_PATCHES,			/* 8 - Code Menu (Select Cheats) */
	MENU_PATCH_VIEW,		/* 9 - Code Menu (View Cheat) */
	MENU_CODE_OPTIONS,		/* 10 - Code Menu (View Cheat Options) */
	MENU_SAVE_DETAILS,
	MENU_HEX_EDITOR,
	MENU_PS1VMC_SAVES,		/* 13 - PS1 VMC Menu */	
	MENU_PS2VMC_SAVES,		/* 14 - PS2 VMC Menu */
	TOTAL_MENU_IDS
};

//Textures
enum texture_index
{
	leon_luna_jpg_index,
	bgimg_jpg_index,
	column_1_png_index,
	column_2_png_index,
	column_3_png_index,
	column_4_png_index,
	column_5_png_index,
	column_6_png_index,
	column_7_png_index,
	jar_trophy_png_index,
	jar_usb_png_index,
	jar_hdd_png_index,
	jar_db_png_index,
	jar_bup_png_index,
	jar_opt_png_index,
	jar_about_png_index,
	jar_trophy_hover_png_index,
	jar_usb_hover_png_index,
	jar_hdd_hover_png_index,
	jar_db_hover_png_index,
	jar_bup_hover_png_index,
	jar_opt_hover_png_index,
	jar_about_hover_png_index,
	logo_png_index,
	logo_text_png_index,
	cat_about_png_index,
	cat_cheats_png_index,
	cat_empty_png_index,
	cat_opt_png_index,
	cat_usb_png_index,
	cat_bup_png_index,
	cat_db_png_index,
	cat_hdd_png_index,
	cat_sav_png_index,
	cat_warning_png_index,
	tag_lock_png_index,
	tag_own_png_index,
	tag_vmc_png_index,
	tag_ps1_png_index,
	tag_ps2_png_index,
	tag_ps3_png_index,
	tag_ps4_png_index,
	tag_psp_png_index,
	tag_psv_png_index,
	tag_warning_png_index,
	tag_transfer_png_index,
	tag_zip_png_index,
	tag_net_png_index,
	tag_apply_png_index,
	buk_scr_png_index,
	footer_ico_circle_png_index,
	footer_ico_cross_png_index,
	footer_ico_square_png_index,
	footer_ico_triangle_png_index,
	footer_ico_lt_png_index,
	footer_ico_rt_png_index,
	opt_off_png_index,
	opt_on_png_index,
	icon_png_file_index,

//Imagefont.bin assets
	trp_sync_png_index,
	trp_bronze_png_index,
	trp_silver_png_index,
	trp_gold_png_index,
	trp_platinum_png_index,

//Artemis assets
	help_png_index,
	edit_shadow_png_index,
	circle_loading_bg_png_index,
	circle_loading_seek_png_index,
	scroll_bg_png_index,
	scroll_lock_png_index,
	mark_arrow_png_index,
	mark_line_png_index,
	header_dot_png_index,
	header_line_png_index,
	cheat_png_index,

	TOTAL_MENU_TEXTURES
};

#define RGBA_R(c)		(uint8_t)((c & 0xFF000000) >> 24)
#define RGBA_G(c)		(uint8_t)((c & 0x00FF0000) >> 16)
#define RGBA_B(c)		(uint8_t)((c & 0x0000FF00) >> 8)
#define RGBA_A(c)		(uint8_t) (c & 0x000000FF)

#define DIALOG_TYPE_OK						0
#define DIALOG_TYPE_YESNO					1

//Fonts
#define font_adonais_regular				0
#define font_console_10x20					1

#define APP_FONT_COLOR						0xFFFFFF00
#define APP_FONT_TAG_COLOR					0xFFFFFF00
#define APP_FONT_MENU_COLOR					0x00000000
#define APP_FONT_TITLE_COLOR				0xFFFFFF00
#define APP_FONT_SIZE_TITLE					84, 72
#define APP_FONT_SIZE_SUBTITLE				68, 60
#define APP_FONT_SIZE_SUBTEXT				36, 36
#define APP_FONT_SIZE_ABOUT					56, 50
#define APP_FONT_SIZE_SELECTION				56, 48
#define APP_FONT_SIZE_DESCRIPTION			54, 48
#define APP_FONT_SIZE_MENU					56, 54
#define APP_FONT_SIZE_JARS					46, 44
#define APP_LINE_OFFSET						40

#define SCREEN_WIDTH						1920
#define SCREEN_HEIGHT						1080

//Asset sizes
#define	logo_png_w							478
#define	logo_png_h							468
#define bg_water_png_w						1920
#define bg_water_png_h						230


#define scroll_bg_png_x						1810
#define scroll_bg_png_y						169


#define help_png_x							80
#define help_png_y							150
#define help_png_w							1730
#define help_png_h							800


//Asset positions
#define bg_water_png_x						0
#define bg_water_png_y						851
#define list_bg_png_x						0
#define list_bg_png_y						169
#define logo_png_x							722
#define logo_png_y							45
#define column_1_png_x						131
#define column_1_png_y						908
#define column_2_png_x						401
#define column_2_png_y						831
#define column_3_png_x						638
#define column_3_png_y						871
#define column_4_png_x						870
#define column_4_png_y						831
#define column_5_png_x						1094
#define column_5_png_y						942
#define column_6_png_x						1313
#define column_6_png_y						828
#define column_7_png_x						1665
#define column_7_png_y						955
#define jar_empty_png_x						159
#define jar_empty_png_y						777
#define jar_usb_png_x						441
#define jar_usb_png_y						699
#define jar_hdd_png_x						669
#define jar_hdd_png_y						739
#define jar_db_png_x						898
#define jar_db_png_y						700
#define jar_bup_png_x						1125
#define jar_bup_png_y						810
#define jar_opt_png_x						1353
#define jar_opt_png_y						696
#define jar_about_png_x						1698
#define jar_about_png_y						782
#define cat_any_png_x						40
#define cat_any_png_y						45
#define app_ver_png_x						1828
#define app_ver_png_y						67


typedef struct 
{
	uint8_t* data;
	size_t size;
	int start;
	int pos;
	uint8_t low_nibble;
	char filepath[256];
} hexedit_data_t;

typedef struct t_png_texture
{
	uint32_t *buffer;
	int width;
	int height;
	u32 size;
	SDL_Texture *texture;
} png_texture;

extern u32 * texture_mem;      // Pointers to texture memory
extern u32 * free_mem;         // Pointer after last texture

extern png_texture * menu_textures;				// png_texture array for main menu, initialized in LoadTexture

extern int highlight_alpha;						// Alpha of the selected
extern int idle_time;							// Set by readPad

extern int menu_id;
extern int menu_sel;
extern int menu_old_sel[]; 
extern int last_menu_id[];
extern const char * menu_pad_help[];

extern struct save_entry * selected_entry;
extern struct code_entry * selected_centry;
extern int option_index;

extern void DrawBackground2D(u32 rgba);
extern void DrawTexture(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba);
extern void DrawTextureCentered(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba);
extern void DrawTextureCenteredX(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba);
extern void DrawTextureCenteredY(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba);
extern void DrawHeader(int icon, int xOff, const char * headerTitle, const char * headerSubTitle, u32 rgba, u32 bgrgba, int mode);
extern void DrawHeader_Ani(int icon, const char * headerTitle, const char * headerSubTitle, u32 rgba, u32 bgrgba, int ani, int div);
extern void DrawBackgroundTexture(int x, u8 alpha);
extern void DrawTextureRotated(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba, float angle);
extern void Draw_MainMenu(void);
extern void Draw_MainMenu_Ani(void);
extern void Draw_HexEditor(const hexedit_data_t* hex);
extern void Draw_HexEditor_Ani(const hexedit_data_t* hex);
int LoadMenuTexture(const char* path, int idx);
void LoadVmcTexture(int width, int height, uint8_t* icon);
void initMenuOptions(void);

void drawScene(void);
void drawSplashLogo(int m);
void drawEndLogo(void);

int load_app_settings(app_config_t* config);
int save_app_settings(app_config_t* config);

int initialize_jbc(void);
void terminate_jbc(void);
int initVshDataMount(void);
int get_max_pfskey_ver(void);

#endif
