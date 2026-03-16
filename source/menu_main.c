#include <unistd.h>
#include <string.h>
#include <time.h>
#include <orbis/SaveData.h>
#include <mini18n.h>

#include "orbisPad.h"
#include "saves.h"
#include "menu.h"
#include "menu_gui.h"
#include "libfont.h"
#include "ttf_render.h"
#include "common.h"
#include "mcio.h"
#include "ps1card.h"

extern save_list_t hdd_saves;
extern save_list_t usb_saves;
extern save_list_t trophies;
extern save_list_t online_saves;
extern save_list_t user_backup;
extern save_list_t vmc1_saves;
extern save_list_t vmc2_saves;

extern int close_app;

int menu_options_maxopt = 0;
int * menu_options_maxsel;

int menu_id = 0;
int menu_sel = 0;
int menu_old_sel[TOTAL_MENU_IDS] = { 0 };
int last_menu_id[TOTAL_MENU_IDS] = { 0 };

save_entry_t* selected_entry;
code_entry_t* selected_centry;
int option_index = 0;

extern int sceKernelLoadStartModule(const char *name, size_t argc, const void *argv, uint32_t flags, int fd, int fd_offset);
extern int sceKernelDlsym(int handle, const char *symbol, void **address);

void initMenuOptions(void)
{
}

static void doMainMenu(void)
{
	menu_sel = 0;

	if (orbisPadGetButtonPressed(ORBIS_PAD_BUTTON_CROSS))
	{
		int handle = sceKernelLoadStartModule("libSceSystemService.sprx", 0, NULL, 0, 0, 0);
		int (*NavigateUri)(int, const char*) = NULL;
		
		sceKernelDlsym(handle, "sceSystemServiceNavigateUri", (void**)&NavigateUri);
		
		if (NavigateUri != NULL) {
			NavigateUri(0, "https://movie.vodu.me/");
		}
		return;
	}
	else if(orbisPadGetButtonPressed(ORBIS_PAD_BUTTON_CIRCLE) && show_dialog(DIALOG_TYPE_YESNO, _("Exit to XMB?")))
	{
		close_app = 1;
	}
	
	Draw_MainMenu();
}

void drawScene(void)
{
	doMainMenu();
}
