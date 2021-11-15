#ifndef __ARTEMIS_MENU_H__
#define __ARTEMIS_MENU_H__

#include <orbis2d.h>

#include "settings.h"

//Textures
enum texture_index
{
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
	tag_pce_png_index,
	tag_ps1_png_index,
	tag_ps2_png_index,
	tag_ps3_png_index,
	tag_psp_png_index,
	tag_psv_png_index,
	tag_warning_png_index,
	tag_transfer_png_index,
	tag_zip_png_index,
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
	trp_sync_img_index,
	trp_bronze_img_index,
	trp_silver_img_index,
	trp_gold_img_index,
	trp_platinum_img_index,

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

//Fonts
#define  font_adonais_regular								0

#define APP_FONT_COLOR						0xFFFFFF00
#define APP_FONT_TAG_COLOR					0xFFFFFF00
#define APP_FONT_MENU_COLOR					0x00000000
#define APP_FONT_TITLE_COLOR				0xFFFFFF00
#define APP_FONT_SIZE_TITLE					24, 24
#define APP_FONT_SIZE_SUBTITLE				20, 20
#define APP_FONT_SIZE_SUBTEXT				12, 12
#define APP_FONT_SIZE_ABOUT					19, 17
#define APP_FONT_SIZE_SELECTION				14, 16
#define APP_FONT_SIZE_DESCRIPTION			18, 16
#define APP_FONT_SIZE_MENU					19, 18

//Screen adjustment (Orbis2d/Assets)
#define SCREEN_W_ADJ 						1280/1920
#define SCREEN_H_ADJ 						720/1080

//Asset sizes
#define	logo_png_w							478 * SCREEN_W_ADJ
#define	logo_png_h							468 * SCREEN_H_ADJ
#define bg_water_png_w						1920 * SCREEN_W_ADJ
#define bg_water_png_h						230 * SCREEN_H_ADJ


#define scroll_bg_png_x						1810 * SCREEN_W_ADJ
#define scroll_bg_png_y						169 * SCREEN_H_ADJ


#define help_png_x							80
#define help_png_y							150 * SCREEN_H_ADJ
#define help_png_w							730
#define help_png_h							400


//Asset positions
#define bg_water_png_x						0
#define bg_water_png_y						851 * SCREEN_H_ADJ
#define list_bg_png_x						0
#define list_bg_png_y						169 * SCREEN_H_ADJ
#define logo_png_x							722 * SCREEN_W_ADJ
#define logo_png_y							45 * SCREEN_H_ADJ
#define column_1_png_x						131 * SCREEN_W_ADJ
#define column_1_png_y						908 * SCREEN_H_ADJ
#define column_2_png_x						401 * SCREEN_W_ADJ
#define column_2_png_y						831 * SCREEN_H_ADJ
#define column_3_png_x						638 * SCREEN_W_ADJ
#define column_3_png_y						871 * SCREEN_H_ADJ
#define column_4_png_x						870 * SCREEN_W_ADJ
#define column_4_png_y						831 * SCREEN_H_ADJ
#define column_5_png_x						1094 * SCREEN_W_ADJ
#define column_5_png_y						942 * SCREEN_H_ADJ
#define column_6_png_x						1313 * SCREEN_W_ADJ
#define column_6_png_y						828 * SCREEN_H_ADJ
#define column_7_png_x						1665 * SCREEN_W_ADJ
#define column_7_png_y						955 * SCREEN_H_ADJ
#define jar_empty_png_x						159 * SCREEN_W_ADJ
#define jar_empty_png_y						777 * SCREEN_H_ADJ
#define jar_usb_png_x						441 * SCREEN_W_ADJ
#define jar_usb_png_y						699 * SCREEN_H_ADJ
#define jar_hdd_png_x						669 * SCREEN_W_ADJ
#define jar_hdd_png_y						739 * SCREEN_H_ADJ
#define jar_db_png_x						898 * SCREEN_W_ADJ
#define jar_db_png_y						700 * SCREEN_H_ADJ
#define jar_bup_png_x						1125 * SCREEN_W_ADJ
#define jar_bup_png_y						810 * SCREEN_H_ADJ
#define jar_opt_png_x						1353 * SCREEN_W_ADJ
#define jar_opt_png_y						696 * SCREEN_H_ADJ
#define jar_about_png_x						1698 * SCREEN_W_ADJ
#define jar_about_png_y						782 * SCREEN_H_ADJ
#define cat_any_png_x						40 * SCREEN_W_ADJ
#define cat_any_png_y						45 * SCREEN_H_ADJ
#define app_ver_png_x						1828 * SCREEN_W_ADJ
#define app_ver_png_y						67 * SCREEN_H_ADJ


typedef struct t_png_texture
{
	const void *buffer;
	u32 size;
	Orbis2dTexture *texture;
	u32 texture_off;
} png_texture;

u32 * texture_mem;      // Pointers to texture memory
u32 * free_mem;         // Pointer after last texture

extern png_texture * menu_textures;				// png_texture array for main menu, initialized in LoadTexture

extern int highlight_alpha;						// Alpha of the selected
extern int idle_time;							// Set by readPad

extern const char * menu_about_strings[];
extern const char * menu_about_strings_project[];

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
extern void DrawSelector(int x, int y, int w, int h, int hDif, u8 alpha);
extern void DrawHeader(int icon, int xOff, const char * headerTitle, const char * headerSubTitle, u32 rgba, u32 bgrgba, int mode);
extern void DrawHeader_Ani(int icon, const char * headerTitle, const char * headerSubTitle, u32 rgba, u32 bgrgba, int ani, int div);
extern void DrawBackgroundTexture(int x, u8 alpha);
extern void DrawTextureRotated(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba, float angle);
extern void Draw_MainMenu();
extern void Draw_MainMenu_Ani();

void drawJars(uint8_t alpha);
void drawColumns(uint8_t alpha);
void drawSplashLogo(int m, int64_t *flip);

int load_app_settings(app_config_t* config);
int save_app_settings(app_config_t* config);
int reset_app_settings(app_config_t* config);

#endif
