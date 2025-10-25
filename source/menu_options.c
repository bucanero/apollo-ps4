#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <mini18n.h>

#include "saves.h"
#include "menu.h"
#include "menu_gui.h"
#include "libfont.h"
#include "orbisPad.h"

static void _draw_OptionsMenu(u8 alpha)
{
	int c = 0;

    SetFontSize(APP_FONT_SIZE_SELECTION);
    for (int ind = 0, y_off = 200; menu_options[ind].name; ind++, y_off += APP_LINE_OFFSET)
    {
        if (menu_options[ind].spacer)
            y_off += APP_LINE_OFFSET;

        SetFontColor(APP_FONT_COLOR | alpha, 0);
        DrawString(MENU_ICON_OFF + MENU_TITLE_OFF + 50, y_off, menu_options[ind].name);

		switch (menu_options[ind].type)
		{
			case APP_OPTION_BOOL:
				c = (*menu_options[ind].value == 1) ? opt_on_png_index : opt_off_png_index;
				DrawTexture(&menu_textures[c], OPTION_ITEM_OFF - 29, y_off, 0, menu_textures[c].width, menu_textures[c].height, 0xFFFFFF00 | alpha);
				break;

			case APP_OPTION_CALL:
				DrawTexture(&menu_textures[orbisPadGetConf()->crossButtonOK ? footer_ico_cross_png_index : footer_ico_circle_png_index], OPTION_ITEM_OFF - 29, y_off+2, 0, menu_textures[footer_ico_cross_png_index].width, menu_textures[footer_ico_cross_png_index].height, 0xFFFFFF00 | alpha);
				break;

			case APP_OPTION_LIST:
				SetFontAlign(FONT_ALIGN_CENTER);
				DrawFormatString(OPTION_ITEM_OFF - 18, y_off, "< %s >", menu_options[ind].options[*menu_options[ind].value]);
				SetFontAlign(FONT_ALIGN_LEFT);
				break;

			case APP_OPTION_INC:
				SetFontAlign(FONT_ALIGN_CENTER);
				DrawFormatString(OPTION_ITEM_OFF - 18, y_off, "- %d +", *menu_options[ind].value);
				SetFontAlign(FONT_ALIGN_LEFT);
				break;

			default:
				break;
		}
        
        if (menu_sel == ind)
        {
            DrawTexture(&menu_textures[mark_line_png_index], 0, y_off, 0, SCREEN_WIDTH, menu_textures[mark_line_png_index].height * 2, 0xFFFFFF00 | alpha);
            DrawTextureCenteredX(&menu_textures[mark_arrow_png_index], MENU_ICON_OFF + MENU_TITLE_OFF, y_off, 0, (2 * APP_LINE_OFFSET) / 3, APP_LINE_OFFSET + 2, 0xFFFFFF00 | alpha);
        }
    }
}

void Draw_OptionsMenu_Ani(void)
{
    int ani = 0;
    for (ani = 0; ani < MENU_ANI_MAX; ani++)
    {
        SDL_RenderClear(renderer);
        DrawHeader_Ani(cat_opt_png_index, _("Settings"), NULL, APP_FONT_TITLE_COLOR, 0xffffffff, ani, 12);
        
		u8 icon_a = (u8)(((ani * 2) > 0xFF) ? 0xFF : (ani * 2));
        int _game_a = (int)(icon_a - (MENU_ANI_MAX / 2)) * 2;
        if (_game_a > 0xFF)
            _game_a = 0xFF;
        u8 game_a = (u8)(_game_a < 0 ? 0 : _game_a);
        
        if (game_a > 0)
        	_draw_OptionsMenu(game_a);
        
        SDL_RenderPresent(renderer);
        
        if (game_a == 0xFF)
            return;
    }
}

void Draw_OptionsMenu(void)
{
    DrawHeader(cat_opt_png_index, 0, _("Settings"), NULL, APP_FONT_TITLE_COLOR | 0xFF, 0xffffffff, 0);
    _draw_OptionsMenu(0xFF);
}
