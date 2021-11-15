#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#define MDIALOG_OK		0
#define MDIALOG_YESNO	1

void drawDialogBackground();

static float bar1_countparts;
volatile int msg_dialog_action = 0;
/*
void msg_dialog_event(msgButton button, void *userdata)
{
    switch(button) {

        case MSG_DIALOG_BTN_YES:
            msg_dialog_action = 1;
            break;
        case MSG_DIALOG_BTN_NO:
        case MSG_DIALOG_BTN_ESCAPE:
        case MSG_DIALOG_BTN_NONE:
            msg_dialog_action = 2;
            break;
        default:
		    break;
    }
}
*/
int show_dialog(int tdialog, const char * format, ...)
{
/*
    msg_dialog_action = 0;
    char str[0x800];
    va_list	opt;

    va_start(opt, format);
    vsprintf((void*) str, format, opt);
    va_end(opt);

    msgType mtype = MSG_DIALOG_BKG_INVISIBLE | MSG_DIALOG_NORMAL;
    mtype |=  (tdialog ? (MSG_DIALOG_BTN_TYPE_YESNO  | MSG_DIALOG_DEFAULT_CURSOR_NO) : MSG_DIALOG_BTN_TYPE_OK);

    msgDialogOpen2(mtype, str, msg_dialog_event, NULL, NULL);

    while(!msg_dialog_action)
    {
        drawDialogBackground();
    }
    msgDialogAbort();
    usleep(100 *1000);

    return (msg_dialog_action == 1);
*/
    return 0;
}

/*
void init_progress_bar(const char* progress_bar_title, const char* msg)
{
	bar1_countparts = 0.0f;

    msgDialogOpen2(MSG_DIALOG_BKG_INVISIBLE | MSG_DIALOG_SINGLE_PROGRESSBAR | MSG_DIALOG_MUTE_ON, progress_bar_title, NULL, NULL, NULL);
    msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0, msg);
    msgDialogProgressBarReset(MSG_PROGRESSBAR_INDEX0);

    drawDialogBackground();
}

void end_progress_bar(void)
{
    msgDialogAbort();
}

void update_progress_bar(uint64_t* progress, const uint64_t total_size, const char* msg)
{
	if(*progress > 0) {
		bar1_countparts += (100.0f * ((double) *progress)) / ((double) total_size);
        *progress = 0;
	}

	if(bar1_countparts >= 1.0f) {
        msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0, msg);
        msgDialogProgressBarInc(MSG_PROGRESSBAR_INDEX0, (u32) bar1_countparts);
            
       	bar1_countparts -= (float) ((u32) bar1_countparts);
	}

	drawDialogBackground();
}

/ *
#define TOTAL_OPT 3

void Xmsg_dialog_event(msgButton button, void *userdata)
{
    uint32_t* bits = (uint32_t*) userdata;

    switch(button) {

        case MSG_DIALOG_BTN_YES:
            *bits |= (1 << 31);
            *bits ^= (1 << msg_dialog_action);

            if (msg_dialog_action == TOTAL_OPT)
                msg_dialog_action = -msg_dialog_action;
            break;

        case MSG_DIALOG_BTN_NO:
        case MSG_DIALOG_BTN_ESCAPE:
        case MSG_DIALOG_BTN_NONE:
            *bits |= (1 << 31);
            msg_dialog_action++;

            if (msg_dialog_action > TOTAL_OPT)
                msg_dialog_action = 1;
            break;

        default:
		    break;
    }
}

#include <string.h>
//#include <stdio.h>

int Xshow_dialog()
{
    uint32_t enabled = 0;
    char str[1024];
    const char text_options[4][32] = {
        "--- MENU ---",
        "Option 1",
        "Option 2",
        "Exit",
    };

    msg_dialog_action = 1;
    msgType mtype = MSG_DIALOG_BKG_INVISIBLE | MSG_DIALOG_NORMAL | MSG_DIALOG_BTN_TYPE_OK;

    while(msg_dialog_action >= 0)
    {
        strcpy(str, text_options[0]);
        strcat(str, "\n\n");

        for (int i = 1; i <= TOTAL_OPT; i++)
        {
            strcat(str, (i == msg_dialog_action) ? "> " : "  ");
            strcat(str, text_options[i]);

            if (i < TOTAL_OPT)
                strcat(str, (enabled & (1 << i)) ? ": ON" : ": OFF");

            if (i == msg_dialog_action)
                strcat(str, " <");

            strcat(str, "\n");
        }

        msgDialogOpen2(mtype, str, Xmsg_dialog_event, &enabled, NULL);

        while (!(enabled & (1 << 31)))
        {
            drawDialogBackground();
        }

        enabled ^= (1 << 31);
    }
    msgDialogAbort();
    usleep(100 *1000);

    return (msg_dialog_action == 1);
}
*/