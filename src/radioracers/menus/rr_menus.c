// RadioRacers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by $HOME
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file radioracers/menu/rr_menus_hud.c

#include "../rr_menu.h"
#include "../rr_cvar.h"

#include "../../d_main.h"
#include "../../v_video.h"

// HUD
menuitem_t OPTIONS_RadioRacersHud[] =
{
	{IT_STRING | IT_CVAR, "Ring Counter Position", "Toggle between the Vanilla and Custom HUD positioning.",
		NULL, {.cvar = &cv_ringsonplayer}, 0, 0},

	{IT_SPACE | IT_NOTHING, NULL,  NULL,
		NULL, {NULL}, 0, 0},
	
	{IT_HEADER, "Hide HUD Elements...", NULL,
		NULL, {NULL}, 0, 0},
    
	{IT_STRING | IT_CVAR, "Hide Countdown", "Hide the countdown graphics at the beginning of a race.",
		NULL, {.cvar = &cv_hud_hidecountdown}, 0, 0},
        
	{IT_STRING | IT_CVAR, "Hide POSITION!!!", "Hide the POSITION!!! graphics at the beginning of a race.",
		NULL, {.cvar = &cv_hud_hideposition}, 0, 0},

	{IT_STRING | IT_CVAR, "Hide Lap Emblem", "Hide the Lap Emblem when you begin a new lap.",
		NULL, {.cvar = &cv_hud_hidelapemblem}, 0, 0}
};

menu_t OPTIONS_RadioRacersHudDef = {
	sizeof (OPTIONS_RadioRacersHud) / sizeof (menuitem_t),
	&OPTIONS_HUDDef,
	0,
	OPTIONS_RadioRacersHud,
	48, 80,
	SKINCOLOR_SUNSLAM, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawGenericOptions,
	M_DrawOptionsCogs,
	M_OptionsTick,
	NULL,
	NULL,
	NULL,
};