#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>

#include <orbis/libkernel.h>
#include <orbis/CommonDialog.h>
#include <orbis/MsgDialog.h>


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

void init_progress_bar(const char* msg)
{
    OrbisMsgDialogParam param;
    OrbisMsgDialogProgressBarParam userBarParam;

    sceMsgDialogInitialize();
    orbisMsgDialogParamInitialize(&param);
    param.mode = ORBIS_MSG_DIALOG_MODE_PROGRESS_BAR;

    memset(&userBarParam, 0, sizeof(userBarParam));
    userBarParam.msg = msg;
    userBarParam.barType = ORBIS_MSG_DIALOG_PROGRESSBAR_TYPE_PERCENTAGE;
    param.progBarParam = &userBarParam;

    if (sceMsgDialogOpen(&param) < 0)
        return;
}

void end_progress_bar(void)
{
    sceMsgDialogClose();
    sceMsgDialogTerminate();
}

void update_progress_bar(uint64_t progress, const uint64_t total_size, const char* msg)
{
    float bar_value = (100.0f * ((double) progress)) / ((double) total_size);

    if (sceMsgDialogUpdateStatus() == ORBIS_COMMON_DIALOG_STATUS_RUNNING)
    {
        sceMsgDialogProgressBarSetMsg(0, msg);
        sceMsgDialogProgressBarSetValue(0, (uint32_t) bar_value);
    }
}

int init_loading_screen(const char* message)
{
	init_progress_bar(message);
    return 1;
}

void stop_loading_screen()
{
	update_progress_bar(1, 1, "");
	end_progress_bar();
}

void notifi(const char *p_Uri, const char *p_Format, ...)
{
    OrbisNotificationRequest s_Request;
    memset(&s_Request, '\0', sizeof(s_Request));

    s_Request.reqId = NotificationRequest;
    s_Request.unk3 = 0;
    s_Request.useIconImageUri = 0;
    s_Request.targetId = -1;

    // Maximum size to move is destination size - 1 to allow for null terminator
    if (p_Uri != NULL) {
        s_Request.useIconImageUri = 1;
        strlcpy(s_Request.iconUri, p_Uri, sizeof(s_Request.iconUri));
    }

    va_list p_Args;
    va_start(p_Args, p_Format);
    // p_Format is controlled externally, some compiler/linter options will mark this as a security issue
    vsnprintf(s_Request.message, sizeof(s_Request.message), p_Format, p_Args);
    va_end(p_Args);

    sceKernelSendNotificationRequest(NotificationRequest, &s_Request, sizeof(s_Request), 0);
}
