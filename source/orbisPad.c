/*
#  ____   ____    ____         ___ ____   ____ _     _
# |    |  ____>   ____>  |    |        | <____  \   /
# |____| |    \   ____>  | ___|    ____| <____   \_/	ORBISDEV Open Source Project.
#------------------------------------------------------------------------------------
# Copyright 2010-2020, orbisdev - http://orbisdev.github.io
# Licensed under MIT license
# Review README & LICENSE files for further details.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/time.h>
#include <dbglogger.h>
#include <orbis/SystemService.h>
#include "orbisPad.h"

#define LOG dbglogger_log

static OrbisPadConfig orbisPadConf;
static int orbispad_initialized = 0;
static uint64_t g_time;


static uint64_t timeInMilliseconds(void)
{
    struct timeval tv;

    gettimeofday(&tv,NULL);
    return (((uint64_t)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

void orbisPadFinish()
{
	int ret;

	if(orbispad_initialized && (ret=scePadClose(orbisPadConf.padHandle)) < 0)
	{
		LOG("scePadClose return 0x%8X", ret);
	}
	orbispad_initialized=0;

	LOG("ORBISPAD finished");
}

OrbisPadConfig *orbisPadGetConf()
{
	if(orbispad_initialized)
	{
		return (&orbisPadConf);
	}
	
	return NULL; 
}

static int orbisPadInitConf()
{	
	if(orbispad_initialized)
	{
		return orbispad_initialized;
	}

	memset(&orbisPadConf, 0, sizeof(OrbisPadConfig));
	orbisPadConf.padHandle=-1;
	
	return 0;
}

unsigned int orbisPadGetCurrentButtonsPressed()
{
	return orbisPadConf.buttonsPressed;
}

void orbisPadSetCurrentButtonsPressed(unsigned int buttons)
{
	orbisPadConf.buttonsPressed=buttons;
}

unsigned int orbisPadGetCurrentButtonsReleased()
{
	return orbisPadConf.buttonsReleased;
}

void orbisPadSetCurrentButtonsReleased(unsigned int buttons)
{
	orbisPadConf.buttonsReleased=buttons;
}

bool orbisPadGetButtonHold(unsigned int filter)
{
	uint64_t time = timeInMilliseconds();
	uint64_t delta = time - g_time;

	if((orbisPadConf.buttonsHold&filter)==filter && delta > 150)
	{
		g_time = time;
		return 1;
	}

	return 0;
}

bool orbisPadGetButtonPressed(unsigned int filter)
{
	if (!orbisPadConf.crossButtonOK && (filter & (ORBIS_PAD_BUTTON_CROSS|ORBIS_PAD_BUTTON_CIRCLE)))
		filter ^= (ORBIS_PAD_BUTTON_CROSS|ORBIS_PAD_BUTTON_CIRCLE);

	if((orbisPadConf.buttonsPressed&filter)==filter)
	{
		orbisPadConf.buttonsPressed ^= filter;
		return 1;
	}

	return 0;
}

bool orbisPadGetButtonReleased(unsigned int filter)
{
 	if((orbisPadConf.buttonsReleased&filter)==filter)
	{
		if(~(orbisPadConf.padDataLast.buttons)&filter)
		{
			return 0;
		}
		return 1;
	}

	return 0;
}

int orbisPadUpdate()
{
	int ret;
	unsigned int actualButtons=0;
	unsigned int lastButtons=0;

	memcpy(&orbisPadConf.padDataLast, &orbisPadConf.padDataCurrent, sizeof(OrbisPadData));	
	ret=scePadReadState(orbisPadConf.padHandle, &orbisPadConf.padDataCurrent);

	if(ret==0 && orbisPadConf.padDataCurrent.connected==1)
	{
		if (orbisPadConf.padDataCurrent.leftStick.y < ANALOG_MIN)
			orbisPadConf.padDataCurrent.buttons |= ORBIS_PAD_BUTTON_UP;

		if (orbisPadConf.padDataCurrent.leftStick.y > ANALOG_MAX)
			orbisPadConf.padDataCurrent.buttons |= ORBIS_PAD_BUTTON_DOWN;

		if (orbisPadConf.padDataCurrent.leftStick.x < ANALOG_MIN)
			orbisPadConf.padDataCurrent.buttons |= ORBIS_PAD_BUTTON_LEFT;

		if (orbisPadConf.padDataCurrent.leftStick.x > ANALOG_MAX)
			orbisPadConf.padDataCurrent.buttons |= ORBIS_PAD_BUTTON_RIGHT;

		actualButtons=orbisPadConf.padDataCurrent.buttons;
		lastButtons=orbisPadConf.padDataLast.buttons;
		orbisPadConf.buttonsPressed=(actualButtons)&(~lastButtons);
		if(actualButtons!=lastButtons)
		{
			orbisPadConf.buttonsReleased=(~actualButtons)&(lastButtons);
			orbisPadConf.idle=0;
		}
		else
		{
			orbisPadConf.buttonsReleased=0;
			if (actualButtons == 0)
			{
				orbisPadConf.idle++;
			}
		}
		orbisPadConf.buttonsHold=actualButtons&lastButtons;

		return 0;
	}

	return -1;
}

int orbisPadInit(void)
{
	int ret;
	OrbisUserServiceInitializeParams param;
	param.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;

	if(orbisPadInitConf()==1)
	{
		LOG("ORBISPAD already initialized!");
		return orbispad_initialized;
	}

	ret=sceUserServiceInitialize(&param);
	if((ret<0) && (ret != ORBIS_USER_SERVICE_ERROR_ALREADY_INITIALIZED))
	{
		LOG("sceUserServiceInitialize Error 0x%8X", ret);
		return -1;
	}

	ret=scePadInit();
	if(ret<0)
	{
		LOG("scePadInit Error 0x%8X", ret);
		return -1;
	}

	ret=sceUserServiceGetInitialUser(&orbisPadConf.userId);
	if(ret<0)
	{
		LOG("sceUserServiceGetInitialUser Error 0x%8X", ret);
		return -1;
//		orbisPadConf.userId=0x10000000;
	}
	
	orbisPadConf.padHandle=scePadOpen(orbisPadConf.userId, 0, 0, NULL);
	if(orbisPadConf.padHandle<0)
	{
		LOG("scePadOpen Error 0x%8X", orbisPadConf.padHandle);
		return -1;
	}

	ret = sceSystemServiceParamGetInt(ORBIS_SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN, &orbisPadConf.crossButtonOK);
	if (ret < 0)
	{
		LOG("sceSystemServiceParamGetInt error 0x%08X", ret);
		LOG("Failed to obtain ORBIS_SYSTEM_SERVICE_PARAM_ID_ENTER_BUTTON_ASSIGN info!, assigning X as main button.");
		orbisPadConf.crossButtonOK = 1;
	}

	orbispad_initialized=1;
	g_time = timeInMilliseconds();
	LOG("ORBISPAD initialized: scePadOpen return handle 0x%X", orbisPadConf.padHandle);

	return orbispad_initialized;
}
