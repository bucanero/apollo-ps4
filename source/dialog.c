#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

#include <orbis/CommonDialog.h>
#include <orbis/MsgDialog.h>

#define MDIALOG_OK		0
#define MDIALOG_YESNO	1

void drawDialogBackground();

static float bar1_countparts;

static inline void _orbisCommonDialogSetMagicNumber(uint32_t* magic, const OrbisCommonDialogBaseParam* param)
{
    *magic = (uint32_t)(ORBIS_COMMON_DIALOG_MAGIC_NUMBER + (uint64_t)param);
}

static inline void _orbisCommonDialogBaseParamInit(OrbisCommonDialogBaseParam *param)
{
    memset(param, 0x0, sizeof(OrbisCommonDialogBaseParam));
    param->size = (uint32_t)sizeof(OrbisCommonDialogBaseParam);
    _orbisCommonDialogSetMagicNumber(&(param->magic), param);
}

static inline void orbisMsgDialogParamInitialize(OrbisMsgDialogParam *param)
{
    memset(param, 0x0, sizeof(OrbisMsgDialogParam));
    _orbisCommonDialogBaseParamInit(&param->baseParam);
    param->size = sizeof(OrbisMsgDialogParam);
}

int show_dialog(int tdialog, const char * format, ...)
{
    OrbisMsgDialogParam param;
    OrbisMsgDialogUserMessageParam userMsgParam;
    OrbisMsgDialogResult result;
    char str[0x800];
    va_list	opt;

    memset(str, 0, sizeof(str));
    va_start(opt, format);
    vsprintf((void*) str, format, opt);
    va_end(opt);

    sceMsgDialogInitialize();
    orbisMsgDialogParamInitialize(&param);
    param.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;

    memset(&userMsgParam, 0, sizeof(userMsgParam));
    userMsgParam.msg = str;
    userMsgParam.buttonType = (tdialog ? ORBIS_MSG_DIALOG_BUTTON_TYPE_YESNO_FOCUS_NO : ORBIS_MSG_DIALOG_BUTTON_TYPE_OK);
    param.userMsgParam = &userMsgParam;

    if (sceMsgDialogOpen(&param) < 0)
        return 0;

    do { } while (sceMsgDialogUpdateStatus() != ORBIS_COMMON_DIALOG_STATUS_FINISHED);
    sceMsgDialogClose();

    memset(&result, 0, sizeof(result));
    sceMsgDialogGetResult(&result);
    sceMsgDialogTerminate();

    return (result.buttonId == ORBIS_MSG_DIALOG_BUTTON_ID_YES);
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

*/