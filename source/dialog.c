#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdbool.h>
#include "menu.h"

#include <orbis/libkernel.h>
#include <orbis/CommonDialog.h>
#include <orbis/MsgDialog.h>
#include <orbis/ImeDialog.h>

#define SCE_IME_DIALOG_MAX_TEXT_LENGTH 512

static int ime_dialog_running = 0;
static uint16_t inputTextBuffer[SCE_IME_DIALOG_MAX_TEXT_LENGTH+1];
static uint16_t input_ime_title[SCE_IME_DIALOG_MAX_TEXT_LENGTH];


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
	OrbisMsgDialogButtonsParam buttonsParam;
	OrbisMsgDialogUserMessageParam messageParam;
	OrbisMsgDialogParam dialogParam;
	memset(&buttonsParam, 0, sizeof(buttonsParam));
	memset(&dialogParam, 0, sizeof(dialogParam));
	memset(&messageParam, 0, sizeof(messageParam));

	sceMsgDialogInitialize();
	orbisMsgDialogParamInitialize(&dialogParam);
	dialogParam.userMsgParam = &messageParam;
	dialogParam.mode = ORBIS_MSG_DIALOG_MODE_USER_MSG;
	messageParam.buttonType = ORBIS_MSG_DIALOG_BUTTON_TYPE_WAIT;
	messageParam.msg = message;

	return (sceMsgDialogOpen(&dialogParam) >= 0);
}

void stop_loading_screen(void)
{
	end_progress_bar();
}

void notify_popup(const char *p_Uri, const char *p_Format, ...)
{
    OrbisNotificationRequest s_Request;
    memset(&s_Request, '\0', sizeof(s_Request));

    s_Request.reqId = NotificationRequest;
    s_Request.unk3 = 0;
    s_Request.useIconImageUri = 0;
    s_Request.targetId = -1;

    // Maximum size to move is destination size - 1 to allow for null terminator
    if (p_Uri)
    {
        s_Request.useIconImageUri = 1;
        strlcpy(s_Request.iconUri, p_Uri, sizeof(s_Request.iconUri));
    }

    va_list p_Args;
    va_start(p_Args, p_Format);
    vsnprintf(s_Request.message, sizeof(s_Request.message), p_Format, p_Args);
    va_end(p_Args);

    sceKernelSendNotificationRequest(NotificationRequest, &s_Request, sizeof(s_Request), 0);
}

static int convert_to_utf16(const char* utf8, uint16_t* utf16, uint32_t available)
{
    int count = 0;
    while (*utf8)
    {
        uint8_t ch = (uint8_t)*utf8++;
        uint32_t code;
        uint32_t extra;

        if (ch < 0x80)
        {
            code = ch;
            extra = 0;
        }
        else if ((ch & 0xe0) == 0xc0)
        {
            code = ch & 31;
            extra = 1;
        }
        else if ((ch & 0xf0) == 0xe0)
        {
            code = ch & 15;
            extra = 2;
        }
        else
        {
            // TODO: this assumes there won't be invalid utf8 codepoints
            code = ch & 7;
            extra = 3;
        }

        for (uint32_t i=0; i<extra; i++)
        {
            uint8_t next = (uint8_t)*utf8++;
            if (next == 0 || (next & 0xc0) != 0x80)
            {
                goto utf16_end;
            }
            code = (code << 6) | (next & 0x3f);
        }

        if (code < 0xd800 || code >= 0xe000)
        {
            if (available < 1) goto utf16_end;
            utf16[count++] = (uint16_t)code;
            available--;
        }
        else // surrogate pair
        {
            if (available < 2) goto utf16_end;
            code -= 0x10000;
            utf16[count++] = 0xd800 | (code >> 10);
            utf16[count++] = 0xdc00 | (code & 0x3ff);
            available -= 2;
        }
    }

utf16_end:
    utf16[count]=0;
    return count;
}

static int convert_from_utf16(const uint16_t* utf16, char* utf8, uint32_t size)
{
    int count = 0;
    while (*utf16)
    {
        uint32_t code;
        uint16_t ch = *utf16++;
        if (ch < 0xd800 || ch >= 0xe000)
        {
            code = ch;
        }
        else // surrogate pair
        {
            uint16_t ch2 = *utf16++;
            if (ch < 0xdc00 || ch > 0xe000 || ch2 < 0xd800 || ch2 > 0xdc00)
            {
                goto utf8_end;
            }
            code = 0x10000 + ((ch & 0x03FF) << 10) + (ch2 & 0x03FF);
        }

        if (code < 0x80)
        {
            if (size < 1) goto utf8_end;
            utf8[count++] = (char)code;
            size--;
        }
        else if (code < 0x800)
        {
            if (size < 2) goto utf8_end;
            utf8[count++] = (char)(0xc0 | (code >> 6));
            utf8[count++] = (char)(0x80 | (code & 0x3f));
            size -= 2;
        }
        else if (code < 0x10000)
        {
            if (size < 3) goto utf8_end;
            utf8[count++] = (char)(0xe0 | (code >> 12));
            utf8[count++] = (char)(0x80 | ((code >> 6) & 0x3f));
            utf8[count++] = (char)(0x80 | (code & 0x3f));
            size -= 3;
        }
        else
        {
            if (size < 4) goto utf8_end;
            utf8[count++] = (char)(0xf0 | (code >> 18));
            utf8[count++] = (char)(0x80 | ((code >> 12) & 0x3f));
            utf8[count++] = (char)(0x80 | ((code >> 6) & 0x3f));
            utf8[count++] = (char)(0x80 | (code & 0x3f));
            size -= 4;
        }
    }

utf8_end:
    utf8[count]=0;
    return count;
}

static int initImeDialog(const char *usrTitle, const char *initialText, int maxTextLen, int type)
{
    OrbisImeDialogSetting param;

    if (ime_dialog_running)
        return 0;

    if ((strlen(initialText) > countof(inputTextBuffer)) || (strlen(usrTitle) > countof(input_ime_title)))
    {
        ime_dialog_running = 0;
        return 0;
    }

    memset(&param, 0, sizeof(OrbisImeDialogSetting));
    memset(inputTextBuffer, 0, sizeof(inputTextBuffer));
    memset(input_ime_title, 0, sizeof(input_ime_title));

    // converts the multibyte string src to a wide-character string starting at dest.
    convert_to_utf16(initialText, inputTextBuffer, countof(inputTextBuffer));
    convert_to_utf16(usrTitle, input_ime_title, countof(input_ime_title));

    param.userId = apollo_config.user_id;
    param.supportedLanguages = 0;
    param.maxTextLength = maxTextLen;
    param.inputTextBuffer = (wchar_t*) inputTextBuffer;
    param.title = (wchar_t*) input_ime_title;
    param.type = type ? ORBIS_TYPE_TYPE_URL : ORBIS_TYPE_DEFAULT;
    param.posx = 1920 / 2;
    param.posy = 1080 / 2;
    param.horizontalAlignment = ORBIS_H_CENTER;
    param.verticalAlignment = ORBIS_V_CENTER;
    param.enterLabel = ORBIS_BUTTON_LABEL_DEFAULT;

    if (sceImeDialogInit(&param, NULL) < 0)
        return 0;

    ime_dialog_running = 1;
    return 1;
}

static int updateImeDialog(void)
{
    OrbisDialogStatus status;
    OrbisDialogResult result;

    if (!ime_dialog_running)
        return 0;

    status = sceImeDialogGetStatus();
    if (status == ORBIS_DIALOG_STATUS_STOPPED)
    {
        memset(&result, 0, sizeof(OrbisDialogResult));
        sceImeDialogGetResult(&result);

        sceImeDialogTerm();
        ime_dialog_running = 0;

        if (result.endstatus == ORBIS_DIALOG_OK)
            return 1;

        return (-1);
    }

    return 0;
}

int osk_dialog_get_text(const char* title, char* text, uint32_t size)
{
    size = (size > SCE_IME_DIALOG_MAX_TEXT_LENGTH) ? SCE_IME_DIALOG_MAX_TEXT_LENGTH : (size-1);

    if (!initImeDialog(title, text, size, 1))
        return 0;

    while (ime_dialog_running)
    {
        if (updateImeDialog() < 0)
            return 0;
    }

    return (convert_from_utf16(inputTextBuffer, text, size));
}
