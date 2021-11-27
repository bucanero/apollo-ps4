#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "saves.h"
#include "menu.h"
#include "menu_cheats.h"
#include "libfont.h"

void DrawScrollBar(int selIndex, int max, int y_inc, int xOff, u8 alpha)
{
    int game_y = 120;
    int maxPerPage = (SCREEN_HEIGHT - (game_y * 2)) / y_inc;
    float pLoc = (float)selIndex / (float)max;
    int yTotal = SCREEN_HEIGHT - (game_y * 2);
    
    if (max < maxPerPage)
        return;
    
    if (idle_time > 80)
    {
        int dec = (idle_time - 80) * 4;
        if (dec > alpha)
            dec = alpha;
        alpha -= dec;
    }
    
    SetFontAlign(FONT_ALIGN_LEFT);
    
    //Draw box
	DrawTextureCenteredX(&menu_textures[scroll_bg_png_index], xOff, game_y, 0, 6, yTotal, 0xffffff00 | alpha);
    
    //Draw cursor
	DrawTextureCenteredX(&menu_textures[scroll_lock_png_index], xOff, (int)((float)yTotal * pLoc) + game_y, 0, 4, maxPerPage * 2, 0xffffff00 | alpha);
}

u8 CalculateAlphaList(int curIndex, int selIndex, int max)
{
    int mult = ((float)0xFF / (float)max * 7) / 4;
    int dif = (selIndex - curIndex);
    if (dif < 0)
        dif *= -1;
    int alpha = (0xFF - (dif * mult));
    return (u8)(alpha < 0 ? 0 : alpha);
}


/*
 * Cheats Code Options Menu
 */
void DrawOptions(option_entry_t* option, u8 alpha, int y_inc, int selIndex)
{
    if (!option->name || !option->value)
        return;
    
    int c = 0, yOff = 80, cIndex = 0;
    
    int maxPerPage = (SCREEN_HEIGHT - (yOff * 2)) / y_inc;
    int startDrawX = selIndex - (maxPerPage / 2);
    int max = maxPerPage + startDrawX;
    
    SetFontSize(y_inc-6, y_inc-4);

    for (c = startDrawX; c < max; c++)
    {
        if (c >= 0 && c < option->size)
        {
            SetFontColor(APP_FONT_COLOR | ((alpha * CalculateAlphaList(c, selIndex, maxPerPage)) / 0xFF), 0);
            
            if (option->name[c])
                DrawString(MENU_SPLIT_OFF + MENU_ICON_OFF + 20, yOff, option->name[c]);
            
            //Selector
            if (c == selIndex)
            {
                int i = 0;
				for (i = MENU_SPLIT_OFF; i < SCREEN_WIDTH; i++)
					DrawTexture(&menu_textures[mark_line_png_index], i, yOff, 0, menu_textures[mark_line_png_index].width, menu_textures[mark_line_png_index].height, 0xFFFFFF00 | alpha);
            }
            
            cIndex++;
        }
        yOff += y_inc;
    }
}

void Draw_CheatsMenu_Options_Ani_Exit(void)
{
    /*
	int div = 12, ani = 0, left = MENU_SPLIT_OFF;
	for (ani = MENU_ANI_MAX - 1; ani >= 0; ani--)
	{
		tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
		tiny3d_AlphaTest(1, 0x0, TINY3D_ALPHA_FUNC_GEQUAL);
		tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
			0x00000303 | 0x00000000,
			TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);

		tiny3d_Project2D();

		u8 icon_a = (u8)((int)(((SCREEN_WIDTH - left) / SCREEN_WIDTH.0) * 255.0));
		left = MENU_SPLIT_OFF + ((MENU_ANI_MAX - ani) * div * 3);
		if (left > SCREEN_WIDTH)
			left = SCREEN_WIDTH;

		u8 rgbVal = 0xFF;
		rgbVal -= (u8)((SCREEN_WIDTH - left) / div);
		if (rgbVal < 0xD0)
			rgbVal = 0xD0;
		Draw_CheatsMenu_Selection(menu_old_sel[5], (rgbVal << 24) | (rgbVal << 16) | (rgbVal << 8) | 0xFF);

		DrawTexture(&menu_textures[edit_shadow_png_index], left - (menu_textures[edit_shadow_png_index].texture.width * 1) + 1, -apollo_config.marginV, 0, menu_textures[edit_shadow_png_index].texture.width, SCREEN_HEIGHT + (apollo_config.marginV * 2), icon_a);
		DrawHeader(cat_cheats_png_index, left, selected_centry->name, "Options", APP_FONT_TITLE_COLOR | icon_a, 0xffffffff, 1);

		//DrawOptions(selected_centry->options[option_index], game_a, 18, menu_old_sel[7]);
		//DrawScrollBar2(menu_old_sel[7], selected_centry->options[option_index].size, 18, 700, game_a);

		tiny3d_Flip();

		if (left == SCREEN_WIDTH)
			return;
	}
    */
}

void Draw_CheatsMenu_Options_Ani(void)
{
    /*
	int div = 12, ani = 0, left = SCREEN_WIDTH;
    for (ani = 0; ani < MENU_ANI_MAX; ani++)
    {
        tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
        tiny3d_AlphaTest(1, 0x0, TINY3D_ALPHA_FUNC_GEQUAL);
        tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
            0x00000303 | 0x00000000,
            TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
        
        tiny3d_Project2D();
        
		u8 icon_a = (u8)(((ani * 4 + 0x40) > 0xFF) ? 0xFF : (ani * 4 + 0x40));
		left = SCREEN_WIDTH - (ani * div * 3);
		if (left < MENU_SPLIT_OFF)
			left = MENU_SPLIT_OFF;
		
        
		u8 rgbVal = 0xFF;
		rgbVal -= (u8)((SCREEN_WIDTH - left) / div);
		if (rgbVal < 0xD0)
			rgbVal = 0xD0;
		Draw_CheatsMenu_Selection(menu_sel, (rgbVal << 24) | (rgbVal << 16) | (rgbVal << 8) | 0xFF);

		DrawTexture(&menu_textures[edit_shadow_png_index], left - (menu_textures[edit_shadow_png_index].texture.width * 1) + 1, -apollo_config.marginV, 0, menu_textures[edit_shadow_png_index].texture.width, SCREEN_HEIGHT + (apollo_config.marginV * 2), icon_a);
		DrawHeader(cat_cheats_png_index, left, selected_centry->name, "Options", APP_FONT_TITLE_COLOR | icon_a, 0xffffffff, 1);
        
		u8 game_a = (u8)(icon_a < 0x8F ? 0 : icon_a);
		DrawOptions(&selected_centry->options[option_index], game_a, 20, menu_old_sel[7]);
        DrawScrollBar(menu_old_sel[7], selected_centry->options[option_index].size, 20, SCREEN_WIDTH - 75, game_a);
        
        tiny3d_Flip();
        
		if ((SCREEN_WIDTH - (ani * div * 3)) < (MENU_SPLIT_OFF / 2))
            return;
    }
    */
}

void Draw_CheatsMenu_Options(void)
{
	//------------ Backgrounds

	Draw_CheatsMenu_Selection(menu_old_sel[5], 0xD0D0D0FF);

	DrawTexture(&menu_textures[edit_shadow_png_index], MENU_SPLIT_OFF - (menu_textures[edit_shadow_png_index].width * 1) + 1, -apollo_config.marginV, 0, menu_textures[edit_shadow_png_index].width, SCREEN_HEIGHT + (apollo_config.marginV * 2), 0x000000FF);
	DrawHeader(cat_cheats_png_index, MENU_SPLIT_OFF, selected_centry->name, "Options", APP_FONT_TITLE_COLOR | 0xFF, 0xffffffff, 1);

	DrawOptions(&selected_centry->options[option_index], 0xFF, 20, menu_sel);
	DrawScrollBar(menu_sel, selected_centry->options[option_index].size, 20, SCREEN_WIDTH - 75, 0xFF);
}

int DrawCodes(code_entry_t* code, u8 alpha, int y_inc, int xOff, int selIndex)
{
    if (!code->name || !code->codes)
        return 0;
    
    int numOfLines = 0, c = 0, yOff = 80, cIndex = 0;
    int maxPerPage = (SCREEN_HEIGHT - (yOff * 2)) / y_inc;
    int startDrawX = selIndex - (maxPerPage / 2);
    int max = maxPerPage + startDrawX;
    int len = strlen(code->codes);

    for (c = 0; c < len; c++) { if (code->codes[c] == '\n') { numOfLines++; } }

    if (!len || !numOfLines)
        return 0;

    //Splits the codes by line into an array
    char * splitCodes = (char *)malloc(len + 1);
    memcpy(splitCodes, code->codes, len);
    splitCodes[len] = 0;
    char * * lines = (char **)malloc(sizeof(char *) * numOfLines);
    memset(lines, 0, sizeof(char *) * numOfLines);
    lines[0] = (char*)(&splitCodes[0]);

    for (c = 1; c < numOfLines; c++)
    {
        while (splitCodes[cIndex] != '\n' && cIndex < len)
            cIndex++;
        
        if (cIndex >= len)
            break;
        
        splitCodes[cIndex] = 0;
        lines[c] = (char*)(&splitCodes[cIndex + 1]);
    }
    
    SetFontSize(y_inc-6, y_inc-4);
    //SetCurrentFont(font_comfortaa_regular);
    //SetExtraSpace(0);

    if (code->file)
        DrawFormatString(xOff + MENU_ICON_OFF + 20, 434, "Target File: %s", code->file);

    for (c = startDrawX; c < max; c++)
    {
        if (c >= 0 && c < numOfLines)
        {
            SetFontColor(APP_FONT_COLOR | ((alpha * CalculateAlphaList(c, selIndex, maxPerPage)) / 0xFF), 0);
            
            //Draw line
            DrawString(xOff + MENU_ICON_OFF + 20, yOff, lines[c]);
            
            //Selector
            if (c == selIndex)
                for (int i = 0; i < SCREEN_WIDTH; i++)
                    DrawTexture(&menu_textures[mark_line_png_index], xOff + i, yOff, 0, menu_textures[mark_line_png_index].width, menu_textures[mark_line_png_index].height, 0xFFFFFF00 | alpha);
        }
        yOff += y_inc;
    }
    
    free (lines);
    free (splitCodes);
    
//    SetMonoSpace(0);
    SetCurrentFont(0);
//    SetExtraSpace(5);
    
    return numOfLines;
}

void Draw_CheatsMenu_View_Ani_Exit(void)
{
    /*
	int div = 12, ani = 0, left = MENU_SPLIT_OFF;
	for (ani = MENU_ANI_MAX - 1; ani >= 0; ani--)
	{
		tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
		tiny3d_AlphaTest(1, 0x0, TINY3D_ALPHA_FUNC_GEQUAL);
		tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
			0x00000303 | 0x00000000,
			TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);

		tiny3d_Project2D();

		u8 icon_a = (u8)((int)(((SCREEN_WIDTH - left) / SCREEN_WIDTH.0) * 255.0));
		left = MENU_SPLIT_OFF + ((MENU_ANI_MAX - ani) * div * 3);
		if (left > SCREEN_WIDTH)
			left = SCREEN_WIDTH;

		u8 rgbVal = 0xFF;
		rgbVal -= (u8)((SCREEN_WIDTH - left) / div);
		if (rgbVal < 0xD0)
			rgbVal = 0xD0;
		Draw_CheatsMenu_Selection(menu_old_sel[5], (rgbVal << 24) | (rgbVal << 16) | (rgbVal << 8) | 0xFF);

		DrawTexture(&menu_textures[edit_shadow_png_index], left - (menu_textures[edit_shadow_png_index].texture.width * 1) + 1, -apollo_config.marginV, 0, menu_textures[edit_shadow_png_index].texture.width, SCREEN_HEIGHT + (apollo_config.marginV * 2), icon_a);
		DrawHeader(cat_cheats_png_index, left, "Details", selected_centry->name, APP_FONT_TITLE_COLOR | icon_a, 0xffffffff, 1);

		tiny3d_Flip();

		if (left == SCREEN_WIDTH)
			return;
	}
    */
}

void Draw_CheatsMenu_View_Ani(const char* title)
{
    /*
	int div = 12, ani = 0, left = MENU_SPLIT_OFF;
    for (ani = 0; ani < MENU_ANI_MAX; ani++)
    {
        tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
        tiny3d_AlphaTest(1, 0x0, TINY3D_ALPHA_FUNC_GEQUAL);
        tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
            0x00000303 | 0x00000000,
            TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
        
        tiny3d_Project2D();
        
		u8 icon_a = (u8)(((ani * 4 + 0x40) > 0xFF) ? 0xFF : (ani * 4 + 0x40));
		left = SCREEN_WIDTH - (ani * div * 3);
		if (left < MENU_SPLIT_OFF)
			left = MENU_SPLIT_OFF;

		u8 rgbVal = 0xFF;
		rgbVal -= (u8)((SCREEN_WIDTH - left) / div);
		if (rgbVal < 0xD0)
			rgbVal = 0xD0;
		Draw_CheatsMenu_Selection(menu_sel, (rgbVal << 24) | (rgbVal << 16) | (rgbVal << 8) | 0xFF);

		DrawTexture(&menu_textures[edit_shadow_png_index], left - (menu_textures[edit_shadow_png_index].texture.width * 1) + 1, -apollo_config.marginV, 0, menu_textures[edit_shadow_png_index].texture.width, SCREEN_HEIGHT + (apollo_config.marginV * 2), icon_a);
		DrawHeader(cat_cheats_png_index, left, title, selected_centry->name, APP_FONT_TITLE_COLOR | icon_a, 0xffffffff, 1);

		u8 game_a = (u8)(icon_a < 0x8F ? 0 : icon_a);
		int nlines = DrawCodes(selected_centry, game_a, 20, left, menu_old_sel[6]);
		DrawScrollBar(menu_old_sel[6], nlines, 20, SCREEN_WIDTH - 75, game_a);
		
		tiny3d_Flip();

		if ((SCREEN_WIDTH - (ani * div * 3)) < (MENU_SPLIT_OFF / 2))
			return;
    }
    */
}

void Draw_CheatsMenu_View(const char* title)
{
    //------------ Backgrounds
    
	Draw_CheatsMenu_Selection(menu_old_sel[5], 0xD0D0D0FF);

	DrawTexture(&menu_textures[edit_shadow_png_index], MENU_SPLIT_OFF - (menu_textures[edit_shadow_png_index].width * 1) + 1, -apollo_config.marginV, 0, menu_textures[edit_shadow_png_index].width, SCREEN_HEIGHT + (apollo_config.marginV * 2), 0x000000FF);
	DrawHeader(cat_cheats_png_index, MENU_SPLIT_OFF, title, selected_centry->name, APP_FONT_TITLE_COLOR | 0xFF, 0xffffffff, 1);

    int nlines = DrawCodes(selected_centry, 0xFF, 20, MENU_SPLIT_OFF, menu_sel);
    //DrawScrollBar2(menu_sel, nlines, 18, 700, 0xFF);
    DrawScrollBar(menu_sel, nlines, 20, SCREEN_WIDTH - 75, 0xFF);
}

/*
 * Cheats Codes Selection Menu
 */
void DrawGameList(int selIndex, list_t * games, u8 alpha)
{
    SetFontSize(APP_FONT_SIZE_SELECTION);
    
    list_node_t *node;
    save_entry_t *item;
    char tmp[4] = "   ";
    int game_y = 160, y_inc = 20;
    int maxPerPage = (SCREEN_HEIGHT - (game_y * 2)) / y_inc;
    
    int x = selIndex - (maxPerPage / 2);
    int max = maxPerPage + selIndex;
    
    if (max > list_count(games))
        max = list_count(games);
    
    node = list_head(games);
    for (int i = 0; i < x; i++)
        node = list_next(node);
    
    for (; x < max; x++)
    {
        if (x >= 0 && node)
        {
			item = list_get(node);
			u8 a = ((alpha * CalculateAlphaList(x, selIndex, maxPerPage)) / 0xFF);

            if (!a)
                goto skip_draw;

            SetFontColor(APP_FONT_COLOR | a, 0);
			if (item->name)
			{
				char * nBuffer = strdup(item->name);
				int game_name_width = 0;
				while ((game_name_width = WidthFromStr(nBuffer)) > 0 && (MENU_ICON_OFF + (MENU_TITLE_OFF * 1) + game_name_width) > ((SCREEN_WIDTH - 40) - (MENU_ICON_OFF * 3)))
					nBuffer[strlen(nBuffer) - 1] = '\0';
				DrawString(MENU_ICON_OFF + (MENU_TITLE_OFF * 1), game_y, nBuffer);
				free(nBuffer);
			}
			if (item->title_id)
				DrawString((SCREEN_WIDTH - 40) - (MENU_ICON_OFF * 3), game_y, item->title_id);

			tmp[0] = ' ';
			if (item->flags & SAVE_FLAG_PS1) tmp[0] = CHAR_TAG_PS1;
			if (item->flags & SAVE_FLAG_PS2) tmp[0] = CHAR_TAG_PS2;
			if (item->flags & SAVE_FLAG_PSP) tmp[0] = CHAR_TAG_PSP;
			if (item->flags & SAVE_FLAG_PS3) tmp[0] = CHAR_TAG_PS3;
			tmp[1] = (item->flags & SAVE_FLAG_OWNER) ? CHAR_TAG_OWNER : ' ';
			tmp[2] = (item->flags & SAVE_FLAG_LOCKED) ? CHAR_TAG_LOCKED : ' ';
			if (item->flags & SAVE_FLAG_PSV) tmp[1] = CHAR_TAG_PSV;

			DrawString(SCREEN_WIDTH - (MENU_ICON_OFF * 2), game_y, tmp);
skip_draw:
			node = list_next(node);
        }
        
        if (x == selIndex)
        {
            int c;
            for (c = 0; c < SCREEN_WIDTH; c++)
				DrawTexture(&menu_textures[mark_line_png_index], c, game_y, 0, menu_textures[mark_line_png_index].width, menu_textures[mark_line_png_index].height, 0xFFFFFF00 | alpha);
        
			DrawTextureCenteredX(&menu_textures[mark_arrow_png_index], MENU_ICON_OFF - 20, game_y, 0, (2 * y_inc) / 3, y_inc + 2, 0xFFFFFF00 | alpha);
		}
        
        game_y += y_inc;
    }
    
    DrawScrollBar(selIndex, list_count(games), y_inc, SCREEN_WIDTH - 75, alpha);
}

void DrawCheatsList(int selIndex, save_entry_t* game, u8 alpha)
{
    SetFontSize(APP_FONT_SIZE_SELECTION);
    
    list_node_t *node;
    code_entry_t *code;
    int game_y = 80, y_inc = 20;
    int maxPerPage = (SCREEN_HEIGHT - (game_y * 2)) / y_inc;
    
    int x = selIndex - (maxPerPage / 2);
    int max = maxPerPage + selIndex;
    
    if (max > list_count(game->codes))
        max = list_count(game->codes);
    
    node = list_head(game->codes);
    for (int i = 0; i < x; i++)
        node = list_next(node);

    for (; x < max; x++)
    {
        int xo = 0; //(((selIndex - x) < 0) ? -(selIndex - x) : (selIndex - x));
        
        if (x >= 0 && node)
        {
			code = list_get(node);
            //u32 color = game.codes[x].activated ? 0x4040C000 : 0x00000000;
			u8 a = (u8)((alpha * CalculateAlphaList(x, selIndex, maxPerPage)) / 0xFF);
            SetFontColor(APP_FONT_COLOR | a, 0);
            //printf ("Drawing code name %d\n", x);
            float dx = DrawString(MENU_ICON_OFF + (MENU_TITLE_OFF * 3) - xo, game_y, code->name);
            
            if (code->activated)
            {
				DrawTexture(&menu_textures[cheat_png_index], MENU_ICON_OFF, game_y, 0, (MENU_TITLE_OFF * 3) - 15, y_inc + 2, 0xFFFFFF00 | a);

				SetFontSize((int)(y_inc * 0.6), (int)(y_inc * 0.6));
                SetFontAlign(FONT_ALIGN_CENTER);
				SetFontColor(APP_FONT_TAG_COLOR | a, 0);
				DrawString(MENU_ICON_OFF + ((MENU_TITLE_OFF * 3) - 15) / 2, game_y + 2, "select");
                SetFontAlign(FONT_ALIGN_LEFT);
                SetFontSize(APP_FONT_SIZE_SELECTION);
                
                if (code->options_count > 0 && code->options)
                {
                    int od = 0;
                    for (od = 0; od < code->options_count; od++)
                    {
                        if (code->options[od].sel >= 0 && code->options[od].name && code->options[od].name[code->options[od].sel])
                        {
                            //Allocate option
                            char * option = calloc(1, strlen(code->options[od].name[code->options[od].sel]) + 4);

                            //If first print "(NAME", else add to list of names ", NAME"
                            sprintf(option, (od == 0) ? " (%s" : ", %s", code->options[od].name[code->options[od].sel]);
                            
                            //If it's the last one then end the list
                            if (od == (code->options_count - 1))
                                option[strlen(option)] = ')';
                            
							SetFontColor(APP_FONT_COLOR | a, 0);
                            dx = DrawString(dx, game_y, option);
                            
                            free (option);
                        }
                    }
                }
            }
            node = list_next(node);
        }
        
        if (x == selIndex)
        {
            //Draw selection bar
            int c = 0;
            for (c = 0; c < SCREEN_WIDTH; c++)
				DrawTexture(&menu_textures[mark_line_png_index], c, game_y, 0, menu_textures[mark_line_png_index].width, menu_textures[mark_line_png_index].height, 0xFFFFFF00 | alpha);
        
			DrawTextureCenteredX(&menu_textures[mark_arrow_png_index], MENU_ICON_OFF - 20, game_y, 0, (2 * y_inc) / 3, y_inc + 2, 0xFFFFFF00 | alpha);
		}
        
        game_y += y_inc;
    }
    
    DrawScrollBar(selIndex, list_count(game->codes), y_inc, SCREEN_WIDTH - 75, alpha);
}

void Draw_CheatsMenu_Selection_Ani()
{
    /*
    int ani = 0;
    for (ani = 0; ani < MENU_ANI_MAX; ani++)
    {
        tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
        tiny3d_AlphaTest(1, 0x0, TINY3D_ALPHA_FUNC_GEQUAL);
        tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
            0x00000303 | 0x00000000,
            TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
        
        tiny3d_Project2D();
        
        u8 icon_a = (u8)(((ani * 2) > 0xFF) ? 0xFF : (ani * 2));
        
		DrawHeader_Ani(cat_sav_png_index, selected_entry->name, selected_entry->title_id, APP_FONT_TITLE_COLOR, 0xffffffff, ani, 12);

        int _game_a = (int)(icon_a - (MENU_ANI_MAX / 2)) * 2;
        if (_game_a > 0xFF)
            _game_a = 0xFF;
        u8 game_a = (u8)(_game_a < 0 ? 0 : _game_a);
        DrawCheatsList(menu_old_sel[5], selected_entry, game_a);
        
        tiny3d_Flip();
        
        if (_game_a == 0xFF)
            return;
    }
    */
}

void Draw_CheatsMenu_Selection(int menuSel, u32 rgba)
{
	DrawHeader(cat_sav_png_index, 0, selected_entry->name, selected_entry->title_id, APP_FONT_TITLE_COLOR | 0xFF, rgba, 0);
    DrawCheatsList(menuSel, selected_entry, (u8)rgba);
}


/*
 * User Cheats Game Selection Menu
 */
void Draw_UserCheatsMenu_Ani(save_list_t * list)
{
    /*
    int ani = 0;
    for (ani = 0; ani < MENU_ANI_MAX; ani++)
    {
        tiny3d_Clear(0xff000000, TINY3D_CLEAR_ALL);
        tiny3d_AlphaTest(1, 0x0, TINY3D_ALPHA_FUNC_GEQUAL);
        tiny3d_BlendFunc(1, TINY3D_BLEND_FUNC_SRC_RGB_SRC_ALPHA | TINY3D_BLEND_FUNC_SRC_ALPHA_SRC_ALPHA,
            0x00000303 | 0x00000000,
            TINY3D_BLEND_RGB_FUNC_ADD | TINY3D_BLEND_ALPHA_FUNC_ADD);
        
        tiny3d_Project2D();
        
        u8 icon_a = (u8)(((ani * 2) > 0xFF) ? 0xFF : (ani * 2));
        
		DrawHeader_Ani(list->icon_id, list->title, "Save Game List", APP_FONT_TITLE_COLOR, 0xffffffff, ani, 12);
        
        int _game_a = (int)(icon_a - (MENU_ANI_MAX / 2)) * 2;
        if (_game_a > 0xFF)
            _game_a = 0xFF;
        u8 game_a = (u8)(_game_a < 0 ? 0 : _game_a);
        DrawGameList(menu_old_sel[1], list->list, game_a);
        
        tiny3d_Flip();
        
        if (_game_a == 0xFF)
            return;
    }
*/
}

void Draw_UserCheatsMenu(save_list_t * list, int menuSel, u8 alpha)
{
	DrawHeader(list->icon_id, 0, list->title, "Save Game List", APP_FONT_TITLE_COLOR | 0xFF, 0xffffff00 | alpha, 0);
	DrawGameList(menuSel, list->list, alpha);
}
