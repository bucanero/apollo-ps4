/*
#  ____   ____    ____         ___ ____   ____ _     _
# |    |  ____>   ____>  |    |        | <____  \   /
# |____| |    \   ____>  | ___|    ____| <____   \_/	ORBISDEV Open Source Project.
#------------------------------------------------------------------------------------
# Copyright 2010-2020, orbisdev - http://orbisdev.github.io
# Licenced under MIT license
# Review README & LICENSE files for further details.
*/

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <orbis/UserService.h>
#include <orbis/Pad.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ANALOG_CENTER       0x78
#define ANALOG_THRESHOLD    0x68
#define ANALOG_MIN          (ANALOG_CENTER - ANALOG_THRESHOLD)
#define ANALOG_MAX          (ANALOG_CENTER + ANALOG_THRESHOLD)


typedef struct OrbisPadConfig
{
	OrbisUserServiceUserId userId;
	OrbisPadData padDataCurrent;
	OrbisPadData padDataLast;
	unsigned int buttonsPressed;
	unsigned int buttonsReleased;
	unsigned int buttonsHold;
	unsigned int idle;
	int padHandle;
	int crossButtonOK;
}OrbisPadConfig;

int orbisPadInit(void);
void orbisPadFinish(void);
OrbisPadConfig *orbisPadGetConf(void);
bool orbisPadGetButtonHold(unsigned int filter);
bool orbisPadGetButtonPressed(unsigned int filter);
bool orbisPadGetButtonReleased(unsigned int filter);
unsigned int orbisPadGetCurrentButtonsPressed(void);
unsigned int orbisPadGetCurrentButtonsReleased(void);
void orbisPadSetCurrentButtonsPressed(unsigned int buttons);
void orbisPadSetCurrentButtonsReleased(unsigned int buttons);
int orbisPadUpdate(void);

#ifdef __cplusplus
}
#endif
