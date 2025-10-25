#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <mini18n.h>

#include "saves.h"
#include "menu.h"
#include "menu_gui.h"
#include "libfont.h"

#define MAX_LINES   18
#define FNT_HEIGHT  40

static void _draw_HexEditor(const hexedit_data_t* hex, u8 alpha)
{
    char msgout[64];
    char ascii[32];
    int i, y_off = 190;

    SetCurrentFont(font_console_10x20);
    SetFontSize(20, FNT_HEIGHT);
    SetFontColor(0x00000000 | alpha, 0);

    memset(msgout, 32, sizeof(msgout));
    sprintf(msgout + (hex->pos % 16)*3 + hex->low_nibble, "%c", 0xDB);
    DrawFormatStringMono(MENU_ICON_OFF + MENU_TITLE_OFF, y_off - FNT_HEIGHT, "  OFFSET \xB3 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F  \xB3 0123456789ABCDEF");
    DrawFormatStringMono(MENU_ICON_OFF + MENU_TITLE_OFF, y_off + (hex->pos - hex->start)/16*FNT_HEIGHT, "           %s", msgout);
    DrawTexture(&menu_textures[mark_line_png_index], 0, y_off-1 + (hex->pos - hex->start)/16*FNT_HEIGHT, 0, SCREEN_WIDTH, menu_textures[mark_line_png_index].height*2, 0xFFFFFF00 | alpha);

    SetFontColor(APP_FONT_COLOR | alpha, 0);
    for (i = hex->start; i < hex->size && (i - hex->start) < (MAX_LINES*16); i++)
    {
        if (i != hex->start && !(i % 16))
        {
            DrawFormatStringMono(MENU_ICON_OFF + MENU_TITLE_OFF, y_off, "%08X \xB3 %s \xB3 %s", i-0x10, msgout, ascii);
            y_off += FNT_HEIGHT;
        }

        sprintf(msgout + (i % 16)*3, "%02X ", hex->data[i]);
        sprintf(ascii  + (i % 16), "%c", hex->data[i] ? hex->data[i] : '.');
    }
    DrawFormatStringMono(MENU_ICON_OFF + MENU_TITLE_OFF, y_off, "%08X \xB3 %-48s \xB3 %s", (i-1) & ~15, msgout, ascii);

    SetCurrentFont(font_adonais_regular);
}

void Draw_HexEditor_Ani(const hexedit_data_t* hex)
{
    for (int ani = 0; ani < MENU_ANI_MAX; ani++)
    {
        SDL_RenderClear(renderer);
        DrawHeader_Ani(cat_opt_png_index, _("Hex Editor"), strrchr(hex->filepath, '/')+1, APP_FONT_TITLE_COLOR, 0xffffffff, ani, 12);

        u8 icon_a = (u8)(((ani * 2) > 0xFF) ? 0xFF : (ani * 2));
        int _game_a = (int)(icon_a - (MENU_ANI_MAX / 2)) * 2;
        if (_game_a > 0xFF)
            _game_a = 0xFF;
        u8 game_a = (u8)(_game_a < 0 ? 0 : _game_a);

        if (game_a > 0)
        	_draw_HexEditor(hex, game_a);

        SDL_RenderPresent(renderer);

        if (game_a == 0xFF)
            return;
    }
}

void Draw_HexEditor(const hexedit_data_t* hex)
{
    DrawHeader(cat_opt_png_index, 0, _("Hex Editor"), strrchr(hex->filepath, '/')+1, APP_FONT_TITLE_COLOR | 0xFF, 0xffffffff, 0);
    _draw_HexEditor(hex, 0xFF);
}
