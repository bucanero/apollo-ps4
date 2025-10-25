#include <unistd.h>
#include <string.h>
#include <math.h>
#include <mini18n.h>

#include "saves.h"
#include "menu.h"
#include "menu_gui.h"
#include "libfont.h"

#define FONT_W  64
#define FONT_H  62
#define STEP_X  -4         // horizontal displacement

static int sx = SCREEN_WIDTH;

static char user_id_str[9] = "00000000";
static char psid_str[] = "0000000000000000 0000000000000000";
static char account_id_str[] = "0000000000000000";

const char * menu_about_strings[] = { "Bucanero", "Developer",
									"", "",
									"PS3", "credits",
									"Berion", "GUI design",
									"Dnawrkshp", "Artemis code",
									"aldostools", "Bruteforce Save Data",
									NULL };

const char * menu_about_strings_project[] = { "User ID", user_id_str,
											"Account ID", account_id_str,
											"Console PSID", psid_str,
											NULL };

/***********************************************************************
* Draw a string of chars, amplifing y by sin(x)
***********************************************************************/
static void draw_sinetext(int y, const char* string)
{
    int x = sx;       // every call resets the initial x
    int sl = strlen(string);
    char tmp[2] = {0, 0};
    float amp;

    SetFontSize(FONT_W, FONT_H);
    for(int i = 0; i < sl; i++)
    {
        amp = sinf(x      // testing sinf() from math.h
                 * 0.01)  // it turns out in num of bends
                 * 10;    // +/- vertical bounds over y

        if(x > 0 && x < SCREEN_WIDTH - FONT_W)
        {
            tmp[0] = string[i];
            DrawStringMono(x, y + amp, tmp);
        }

        x += FONT_W;
    }

    //* Move string by defined step
    sx += STEP_X;

    if(sx + (sl * FONT_W) < 0)           // horizontal bound, then loop
        sx = SCREEN_WIDTH + FONT_W;
}

static void _setIdValues()
{
	// set to display the PSID on the About menu
	snprintf(psid_str, sizeof(psid_str), "%016lX %016lX", apollo_config.psid[0], apollo_config.psid[1]);
	snprintf(user_id_str, sizeof(user_id_str), "%08x", apollo_config.user_id);
	snprintf(account_id_str, sizeof(account_id_str), "%016lx", apollo_config.account_id);
}

static void _draw_AboutMenu(u8 alpha)
{
	int cnt = 0;
	u8 alp2 = ((alpha*2) > 0xFF) ? 0xFF : (alpha * 2); 
    
    //------------- About Menu Contents
	DrawTextureCenteredX(&menu_textures[logo_text_png_index], SCREEN_WIDTH/2, 110, 0, menu_textures[logo_text_png_index].width * 3/2, menu_textures[logo_text_png_index].height * 3/2, 0xFFFFFF00 | alp2);

    SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
	SetCurrentFont(font_adonais_regular);
	SetFontColor(APP_FONT_MENU_COLOR | alpha, 0);
	SetFontSize(APP_FONT_SIZE_JARS);
	DrawStringMono(0, 220, "PlayStation 4 version");
    
    for (cnt = 0; menu_about_strings[cnt] != NULL; cnt += 2)
    {
        SetFontAlign(FONT_ALIGN_RIGHT);
		DrawStringMono((SCREEN_WIDTH / 2) - 20, 280 + (cnt * 20), menu_about_strings[cnt]);
        
		SetFontAlign(FONT_ALIGN_LEFT);
		DrawStringMono((SCREEN_WIDTH / 2) + 20, 280 + (cnt * 20), menu_about_strings[cnt + 1]);
    }

	DrawTexture(&menu_textures[help_png_index], help_png_x, 300 + (cnt * 22), 0, help_png_w, 220, 0xFFFFFF00 | alp2);

	SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
	SetFontColor(APP_FONT_COLOR | alpha, 0);
	SetFontSize(APP_FONT_SIZE_DESCRIPTION);
	DrawString(0, 250 + ((cnt + 3) * 22), _("Console details:"));
	SetFontSize(APP_FONT_SIZE_SELECTION);

	int off = cnt + 5;
	for (cnt = 0; menu_about_strings_project[cnt] != NULL; cnt += 2)
	{
		SetFontAlign(FONT_ALIGN_RIGHT);
		DrawString((SCREEN_WIDTH / 2) - 10, 250 + ((cnt + off) * 22), menu_about_strings_project[cnt]);

		SetFontAlign(FONT_ALIGN_LEFT);
		DrawString((SCREEN_WIDTH / 2) + 10, 250 + ((off + cnt) * 22), menu_about_strings_project[cnt + 1]);
	}

	SetFontAlign(FONT_ALIGN_SCREEN_CENTER);
	SetCurrentFont(font_adonais_regular);
	SetFontColor(APP_FONT_MENU_COLOR | alp2, 0);
	SetFontSize(APP_FONT_SIZE_JARS);
	DrawStringMono(0, 890, "in memory of Leon & Luna");
	SetFontAlign(FONT_ALIGN_LEFT);
}

static void _draw_LeonLuna(void)
{
	DrawTextureCenteredY(&menu_textures[leon_luna_jpg_index], 0, SCREEN_HEIGHT/2, 0, menu_textures[leon_luna_jpg_index].width, menu_textures[leon_luna_jpg_index].height, 0xFFFFFF00 | 0xFF);
	DrawTexture(&menu_textures[help_png_index], 0, 840, 0, SCREEN_WIDTH + 20, 100, 0xFFFFFF00 | 0xFF);

	SetFontColor(APP_FONT_MENU_COLOR | 0xFF, 0);
	draw_sinetext(860, "... in memory of Leon & Luna - may your days be filled with eternal joy ...");
}

void Draw_AboutMenu_Ani(void)
{
	_setIdValues();

	for (int ani = 0; ani < MENU_ANI_MAX; ani++)
	{
		SDL_RenderClear(renderer);
		DrawBackground2D(0xFFFFFFFF);

		DrawHeader_Ani(cat_about_png_index, _("About"), "v" APOLLO_VERSION, APP_FONT_TITLE_COLOR, 0xffffffff, ani, 12);

		//------------- About Menu Contents

		int rate = (0x100 / (MENU_ANI_MAX - 0x60));
		u8 about_a = (u8)((((ani - 0x60) * rate) > 0xFF) ? 0xFF : ((ani - 0x60) * rate));
		if (ani < 0x60)
			about_a = 0;

		_draw_AboutMenu(about_a);

		SDL_RenderPresent(renderer);

		if (about_a == 0xFF)
			return;
	}
}

void Draw_AboutMenu(int ll)
{
	if (ll)
		return(_draw_LeonLuna());

	_setIdValues();
	DrawHeader(cat_about_png_index, 0, _("About"), "v" APOLLO_VERSION, APP_FONT_TITLE_COLOR | 0xFF, 0xffffffff, 0);
	_draw_AboutMenu(0xFF);
}
