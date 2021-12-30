#include <string.h>
#include <threads.h>
#include <unistd.h>
#include <stdio.h>

#define STBI_ASSERT(x)
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include "types.h"
#include "libfont.h"
#include "menu.h"

#include <dbglogger.h>
#define LOG dbglogger_log

#define JAR_COLUMNS 7


int LoadMenuTexture(const char* path, int idx)
{
	int d;

	menu_textures[idx].buffer = (uint32_t*) stbi_load(path, &menu_textures[idx].width, &menu_textures[idx].height, &d, STBI_rgb_alpha);

	if (!menu_textures[idx].buffer)
	{
		LOG("Error Loading texture (%s)!", path);
		return 0;
	}

	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(menu_textures[idx].buffer, menu_textures[idx].width, menu_textures[idx].height, 32, 4 * menu_textures[idx].width,
												0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);

	menu_textures[idx].texture = SDL_CreateTextureFromSurface(renderer, surface);

	SDL_FreeSurface(surface);
	stbi_image_free(menu_textures[idx].buffer);

	menu_textures[idx].size = menu_textures[idx].width * menu_textures[idx].height * 4;
	menu_textures[idx].buffer = NULL;
	return 1;
}

// draw one background color in virtual 2D coordinates
void DrawBackground2D(u32 rgba)
{
	SDL_SetRenderDrawColor(renderer, RGBA_R(rgba), RGBA_G(rgba), RGBA_B(rgba), RGBA_A(rgba));

	SDL_RenderClear(renderer);
}

void DrawSelector(int x, int y, int w, int h, int hDif, u8 alpha)
{
	int i = 0;
	for (i = 0; i < SCREEN_WIDTH; i++)
		DrawTexture(&menu_textures[mark_line_png_index], i, y, 0, menu_textures[mark_line_png_index].width, menu_textures[mark_line_png_index].height + hDif, 0xFFFFFF00 | alpha);

	DrawTextureCentered(&menu_textures[mark_arrow_png_index], x, y, 0, w, h, 0xFFFFFF00 | alpha);
}

void _drawListBackground(int off, int icon)
{
	switch (icon)
	{
		case cat_db_png_index:
		case cat_usb_png_index:
		case cat_hdd_png_index:
		case cat_opt_png_index:
		case cat_bup_png_index:
		case cat_warning_png_index:
			DrawTexture(&menu_textures[help_png_index], help_png_x, help_png_y, 0, help_png_w, help_png_h, 0xFFFFFF00 | 0xFF);
			break;

		case cat_sav_png_index:
			DrawTexture(&menu_textures[help_png_index], help_png_x, help_png_y, 0, help_png_w, help_png_h, 0xFFFFFF00 | 0xFF);

			if (menu_textures[icon_png_file_index].size)
			{
				DrawTexture(&menu_textures[help_png_index], 1036, help_png_y + 4, 0, 168, 98, 0xFFFFFF00 | 0xFF);
				DrawTexture(&menu_textures[icon_png_file_index], 1040, help_png_y + 8, 0, 160, 88, 0xFFFFFF00 | 0xFF);
			}
			break;

		case cat_cheats_png_index:
			DrawTexture(&menu_textures[help_png_index], off + MENU_ICON_OFF, help_png_y, 0, (SCREEN_WIDTH - 75) - off - MENU_ICON_OFF, help_png_h, 0xFFFFFF00 | 0xFF);
			break;

		case cat_about_png_index:
			break;

		default:
			break;
	}
}

void DrawHeader_Ani(int icon, const char * hdrTitle, const char * headerSubTitle, u32 rgba, u32 bgrgba, int ani, int div)
{
	u8 icon_a = (u8)(((ani * 2) > 0xFF) ? 0xFF : (ani * 2));
	char headerTitle[44];
	snprintf(headerTitle, sizeof(headerTitle), "%.40s%s", hdrTitle, (strlen(hdrTitle) > 40 ? "..." : ""));

	//------------ Backgrounds
	
	//Background
	DrawBackgroundTexture(0, (u8)bgrgba);

	_drawListBackground(0, icon);
	//------------- Menu Bar

	int cnt, cntMax = ((ani * div) > (SCREEN_WIDTH - 75)) ? (SCREEN_WIDTH - 75) : (ani * div);
	for (cnt = MENU_ICON_OFF; cnt < cntMax; cnt++)
		DrawTexture(&menu_textures[header_line_png_index], cnt, 40, 0, menu_textures[header_line_png_index].width, menu_textures[header_line_png_index].height / 2, 0xffffffff);

	DrawTexture(&menu_textures[header_dot_png_index], cnt - 4, 40, 0, menu_textures[header_dot_png_index].width / 2, menu_textures[header_dot_png_index].height / 2, 0xffffff00 | icon_a);

	//header mini icon
	DrawTextureCenteredX(&menu_textures[icon], MENU_ICON_OFF - 20, 32, 0, 48, 48, 0xffffff00 | icon_a);

	//header title string
	SetFontColor(rgba | icon_a, 0);
	SetFontSize(APP_FONT_SIZE_TITLE);
	DrawString(MENU_ICON_OFF + 10, 31, headerTitle);

	//header sub title string
	if (headerSubTitle)
	{
		int width = (SCREEN_WIDTH - 75) - (MENU_ICON_OFF + MENU_TITLE_OFF + WidthFromStr(headerTitle)) - 30;
		SetFontSize(APP_FONT_SIZE_SUBTITLE);
		char * tName = malloc(strlen(headerSubTitle) + 1);
		strcpy(tName, headerSubTitle);
		while (WidthFromStr(tName) > width)
		{
			tName[strlen(tName) - 1] = 0;
		}
		SetFontAlign(FONT_ALIGN_RIGHT);
		DrawString(SCREEN_WIDTH - 75, 35, tName);
		free(tName);
		SetFontAlign(FONT_ALIGN_LEFT);
	}
}

void DrawHeader(int icon, int xOff, const char * hdrTitle, const char * headerSubTitle, u32 rgba, u32 bgrgba, int mode)
{
	char headerTitle[44];
	snprintf(headerTitle, sizeof(headerTitle), "%.40s%s", hdrTitle, (strlen(hdrTitle) > 40 ? "..." : ""));

	//Background
	DrawBackgroundTexture(xOff, (u8)bgrgba);

	_drawListBackground(xOff, icon);
	//------------ Menu Bar
	int cnt = 0;
	for (cnt = xOff + MENU_ICON_OFF; cnt < (SCREEN_WIDTH - 75); cnt++)
		DrawTexture(&menu_textures[header_line_png_index], cnt, 40, 0, menu_textures[header_line_png_index].width, menu_textures[header_line_png_index].height / 2, 0xffffffff);

	DrawTexture(&menu_textures[header_dot_png_index], cnt - 4, 40, 0, menu_textures[header_dot_png_index].width / 2, menu_textures[header_dot_png_index].height / 2, 0xffffffff);

	//header mini icon
	//header title string
	SetFontColor(rgba, 0);
	if (mode)
	{
		DrawTextureCenteredX(&menu_textures[icon], xOff + MENU_ICON_OFF - 12, 40, 0, 32, 32, 0xffffffff);
		SetFontSize(APP_FONT_SIZE_SUBTITLE);
		DrawString(xOff + MENU_ICON_OFF + 10, 35, headerTitle);
	}
	else
	{
		DrawTextureCenteredX(&menu_textures[icon], xOff + MENU_ICON_OFF - 20, 32, 0, 48, 48, 0xffffffff);
		SetFontSize(APP_FONT_SIZE_TITLE);
		DrawString(xOff + MENU_ICON_OFF + 10, 31, headerTitle);
	}

	//header sub title string
	if (headerSubTitle)
	{
		int width = (SCREEN_WIDTH - 75) - (MENU_ICON_OFF + MENU_TITLE_OFF + WidthFromStr(headerTitle)) - 30;
		SetFontSize(APP_FONT_SIZE_SUBTITLE);
		char * tName = malloc(strlen(headerSubTitle) + 1);
		strcpy(tName, headerSubTitle);
		while (WidthFromStr(tName) > width)
		{
			tName[strlen(tName) - 1] = 0;
		}
		SetFontAlign(FONT_ALIGN_RIGHT);
		DrawString(SCREEN_WIDTH - 75, 35, tName);
		free(tName);
		SetFontAlign(FONT_ALIGN_LEFT);
	}
}

void DrawBackgroundTexture(int x, u8 alpha)
{
	if (x == 0)
		DrawTexture(&menu_textures[bgimg_jpg_index], x - apollo_config.marginH, -apollo_config.marginV, 0, SCREEN_WIDTH - x + (apollo_config.marginH * 2), SCREEN_HEIGHT + (apollo_config.marginV * 2), 0xFFFFFF00 | alpha);
	else
		DrawTexture(&menu_textures[bgimg_jpg_index], x, -apollo_config.marginV, 0, SCREEN_WIDTH - x + apollo_config.marginH, SCREEN_HEIGHT + (apollo_config.marginV * 2), 0xFFFFFF00 | alpha);
}

void DrawTexture(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba)
{
	SDL_Rect dest = {
		.x = x,
		.y = y,
		.w = w,
		.h = h,
	};

	SDL_SetTextureAlphaMod(tex->texture, RGBA_A(rgba));	
	SDL_RenderCopy(renderer, tex->texture, NULL, &dest);
}

void DrawTextureCentered(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba)
{
	x -= w / 2;
	y -= h / 2;

	DrawTexture(tex, x, y, z, w, h, rgba);
}

void DrawTextureCenteredX(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba)
{
	x -= w / 2;

	DrawTexture(tex, x, y, z, w, h, rgba);
}

void DrawTextureCenteredY(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba)
{
	y -= h / 2;

	DrawTexture(tex, x, y, z, w, h, rgba);
}

void DrawTextureRotated(png_texture* tex, int x, int y, int z, int w, int h, u32 rgba, float angle)
{
	SDL_Rect dest = {
		.x = x - (w / 2),
		.y = y - (h / 2),
		.w = w,
		.h = h,
	};

	SDL_RenderCopyEx(renderer, tex->texture, NULL, &dest, angle, NULL, SDL_FLIP_NONE);
}

/*
static int please_wait;

void loading_screen_thread(void* user_data)
{
    float angle = 0;

    while (please_wait == 1)
    {
        angle += 0.1f;
    	tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
    	tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);
    	tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
    		TINY3D_BLEND_FUNC_SRC_ALPHA_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_RGB_ONE_MINUS_SRC_ALPHA,
    		TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);

    	tiny3d_Project2D();

		DrawBackgroundTexture(0, 0xFF);

        //Loading animation
        DrawTextureCentered(&menu_textures[logo_png_index], 424, 256, 0, 76, 75, 0xFFFFFFFF);
        DrawTextureCentered(&menu_textures[circle_loading_bg_png_index], 424, 256, 0, 89, 89, 0xFFFFFFFF);
        DrawTextureRotated(&menu_textures[circle_loading_seek_png_index], 424, 256, 0, 89, 89, 0xFFFFFFFF, angle);

		DrawStringMono(0, 336, (char*) user_data);		

    	tiny3d_Flip();
	}

    please_wait = -1;
    sysThreadExit (0);
}

int init_loading_screen(const char* message)
{
    sys_ppu_thread_t tid;
    please_wait = 1;

	SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
	SetFontSize(APP_FONT_SIZE_DESCRIPTION);
	SetFontColor(APP_FONT_MENU_COLOR | 0xFF, 0);

    int ret = sysThreadCreate (&tid, loading_screen_thread, (void*) message, 1000, 16*1024, THREAD_JOINABLE, "please_wait");

    return ret;
}

void stop_loading_screen()
{
    if (please_wait != 1)
        return;

    please_wait = 0;

    while (please_wait != -1)
        usleep(1000);
}
*/

void drawJar(uint8_t idx, int pos_x, int pos_y, const char* text, uint8_t alpha)
{
	uint8_t active = (menu_sel + jar_trophy_png_index == idx);
	DrawTexture(&menu_textures[idx], pos_x, apollo_config.marginV + pos_y, 0, menu_textures[idx].width * SCREEN_W_ADJ, menu_textures[idx].height * SCREEN_H_ADJ, 0xffffff00 | alpha);

	//Selected
	if (active)
		DrawTexture(&menu_textures[idx + JAR_COLUMNS], pos_x, apollo_config.marginV + pos_y, 0, menu_textures[idx + JAR_COLUMNS].width * SCREEN_W_ADJ, menu_textures[idx + JAR_COLUMNS].height * SCREEN_H_ADJ, 0xffffff00 | alpha);

	SetFontColor(APP_FONT_MENU_COLOR | (alpha == 0xFF ? (active ? 0xFF : 0x20) : alpha), 0);
	DrawStringMono(pos_x + (menu_textures[idx].width * SCREEN_W_ADJ / 2), apollo_config.marginV + pos_y - 25, text);
}

void _drawColumn(uint8_t idx, int pos_x, int pos_y, uint8_t alpha)
{
	DrawTexture(&menu_textures[idx], pos_x, apollo_config.marginV + pos_y, 0, menu_textures[idx].width * SCREEN_W_ADJ, menu_textures[idx].height * SCREEN_H_ADJ, 0xffffff00 | alpha);
}

void drawColumns(uint8_t alpha)
{
//	DrawTexture(&menu_textures[bg_water_png_index], bg_water_png_x - apollo_config.marginH, apollo_config.marginV + bg_water_png_y, 0, bg_water_png_w + (apollo_config.marginH * 2), bg_water_png_h, 0xffffff00 | 0xFF);

	//Columns
	_drawColumn(column_1_png_index, column_1_png_x, column_1_png_y, alpha);
	_drawColumn(column_2_png_index, column_2_png_x, column_2_png_y, alpha);
	_drawColumn(column_3_png_index, column_3_png_x, column_3_png_y, alpha);
	_drawColumn(column_4_png_index, column_4_png_x, column_4_png_y, alpha);
	_drawColumn(column_5_png_index, column_5_png_x, column_5_png_y, alpha);
	_drawColumn(column_6_png_index, column_6_png_x, column_6_png_y, alpha);
	_drawColumn(column_7_png_index, column_7_png_x, column_7_png_y, alpha);
}

void drawJars(uint8_t alpha)
{
	SetFontAlign(FONT_ALIGN_CENTER);
	SetFontSize(APP_FONT_SIZE_MENU);
	SetCurrentFont(font_adonais_regular);

	//Trophies
	drawJar(jar_trophy_png_index, jar_empty_png_x, jar_empty_png_y, (alpha == 0xFF ? "Trophies" : ""), alpha);

	//USB save
	drawJar(jar_usb_png_index, jar_usb_png_x, jar_usb_png_y, (alpha == 0xFF ? "USB Saves" : ""), alpha);
	
	//HDD save
	drawJar(jar_hdd_png_index, jar_hdd_png_x, jar_hdd_png_y, (alpha == 0xFF ? "HDD Saves" : ""), alpha);

	//Online cheats
	drawJar(jar_db_png_index, jar_db_png_x, jar_db_png_y, (alpha == 0xFF ? "Online DB" : ""), alpha);
	
	//User Backup
	drawJar(jar_bup_png_index, jar_bup_png_x, jar_bup_png_y, (alpha == 0xFF ? "User Backup" : ""), alpha);

	//Options
	drawJar(jar_opt_png_index, jar_opt_png_x, jar_opt_png_y, (alpha == 0xFF ? "Settings" : ""), alpha);
	
	//About
	drawJar(jar_about_png_index, jar_about_png_x, jar_about_png_y, (alpha == 0xFF ? "About" : ""), alpha);

	SetFontAlign(FONT_ALIGN_LEFT);
}

void drawSplashLogo(int mode)
{
	int ani, max;

	if (mode > 0)
	{
		ani = 0;
		max = MENU_ANI_MAX;
	}
	else
	{
		ani = MENU_ANI_MAX;
		max = 0;
	}

	for (; ani != max; ani += mode)
	{
		// clear the current display buffer
		SDL_RenderClear(renderer);

		DrawBackground2D(0x000000FF);
		
		//------------ Backgrounds
		int logo_a_t = ((ani < 0x20) ? 0 : ((ani - 0x20)*3));
		if (logo_a_t > 0xFF)
			logo_a_t = 0xFF;
		u8 logo_a = (u8)logo_a_t;

		SDL_SetTextureAlphaMod(menu_textures[buk_scr_png_index].texture, logo_a);

		//App description
		DrawTextureCentered(&menu_textures[buk_scr_png_index], SCREEN_WIDTH/2, SCREEN_HEIGHT /2, 0, 484, 363, 0xFFFFFF00 | logo_a);

		//flush and flip
		SDL_UpdateWindowSurface(window);
	}
}

void Draw_MainMenu_Ani()
{
	/*
	int max = MENU_ANI_MAX, ani = 0;
	for (ani = 0; ani < max; ani++)
	{
		SDL_RenderClear(renderer);
		DrawBackground2D(0xFFFFFFFF);
		
		//------------ Backgrounds
		u8 bg_a = (u8)(ani * 2);
		if (bg_a < 0x20)
			bg_a = 0x20;
		int logo_a_t = ((ani < 0x30) ? 0 : ((ani - 0x20)*3));
		if (logo_a_t > 0xFF)
			logo_a_t = 0xFF;
		u8 logo_a = (u8)logo_a_t;
		
		//Background
		DrawBackgroundTexture(0, bg_a);
		
		//App logo
		DrawTexture(&menu_textures[logo_png_index], logo_png_x, logo_png_y, 0, logo_png_w, logo_png_h, 0xFFFFFF00 | logo_a);
		
		//App description
		DrawTextureCenteredX(&menu_textures[logo_text_png_index], SCREEN_WIDTH/2, 320, 0, 459, 75, 0xFFFFFF00 | logo_a);

		SDL_UpdateWindowSurface(window);
	}
	
	max = MENU_ANI_MAX / 2;
	int rate = (0x100 / max);
	for (ani = 0; ani < max; ani++)
	{
		SDL_RenderClear(renderer);
		DrawBackground2D(0xFFFFFFFF);
		
		u8 icon_a = (u8)(((ani * rate) > 0xFF) ? 0xFF : (ani * rate));
		
		//------------ Backgrounds
		
		//Background
		DrawBackgroundTexture(0, 0xFF);
		
		//App logo
		DrawTexture(&menu_textures[logo_png_index], logo_png_x, logo_png_y, 0, logo_png_w, logo_png_h, 0xFFFFFFFF);

		//App description
		DrawTextureCenteredX(&menu_textures[logo_text_png_index], SCREEN_WIDTH/2, 320, 0, 459, 75, 0xFFFFFF00 | 0xFF);

		drawColumns(icon_a);

		//------------ Icons
		drawJars(icon_a);
		
		SDL_UpdateWindowSurface(window);

		if (icon_a == 32)
			break;
	}
	*/
}

void Draw_MainMenu()
{
	//------------ Backgrounds

	//Background
	DrawBackgroundTexture(0, 0xff);
	
	//App logo
	DrawTexture(&menu_textures[logo_png_index], logo_png_x, logo_png_y, 0, logo_png_w, logo_png_h, 0xffffffff);
	
	//App description
	DrawTextureCenteredX(&menu_textures[logo_text_png_index], SCREEN_WIDTH/2, 320, 0, 459, 75, 0xFFFFFF00 | 0xFF);

	drawColumns(0xFF);

	//------------ Icons
	drawJars(0xFF);

}

void drawDialogBackground()
{
	/*
	tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
	tiny3d_AlphaTest(1, 0x10, TINY3D_ALPHA_FUNC_GEQUAL);
	tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
		TINY3D_BLEND_FUNC_SRC_ALPHA_ONE_MINUS_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_RGB_ONE_MINUS_SRC_ALPHA,
		TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);

	tiny3d_Project2D();

	DrawBackgroundTexture(0, 0xFF);
	DrawTexture(&menu_textures[help_png_index], help_png_x, help_png_y + 50, 0, help_png_w, help_png_h - 100, 0xFFFFFF00 | 0xFF);

	tiny3d_Flip();
	*/
}
