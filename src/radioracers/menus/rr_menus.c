// RadioRacers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by $HOME
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file radioracers/menu/rr_menus.c

#include "../rr_menu.h"
#include "../rr_cvar.h"

#include "../../d_main.h"
#include "../../v_video.h"
#include "../../k_kart.h"

// Main
menuitem_t OPTIONS_RadioRacersMenu[] =
{
	{IT_HEADER, "Netplay", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Vote Snitch", "Show players who initiated and cast votes in the chatbox.",
		NULL, {.cvar = &cv_votesnitch}, 0, 0},

	{IT_HEADER, "Controller Rumble", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Extended Controller Rumbles", "Toggle the extended controller rumble events.",
		NULL, {.cvar = &cv_morerumbleevents}, 0, 0},

	{IT_STRING | IT_CVAR, "Rings", "Toggle controller rumble when you pickup and use rings.",
		NULL, {.cvar = &cv_rr_rumble_rings}, 0, 0},

	{IT_STRING | IT_CVAR, "Drift", "Toggle controller rumble when a new drift spark starts.",
		NULL, {.cvar = &cv_rr_rumble_drift}, 0, 0},
		
	{IT_STRING | IT_CVAR, "Spindash", "Toggle controller rumble when spindashing.",
		NULL, {.cvar = &cv_rr_rumble_spindash}, 0, 0},

	{IT_STRING | IT_CVAR, "Wall Bump", "Toggle controller rumble when you bump into a wall.",
		NULL, {.cvar = &cv_rr_rumble_wall_bump}, 0, 0},

	{IT_STRING | IT_CVAR, "Fastfall Bounce", "Toggle controller rumble when you bounce after a fastfall.",
		NULL, {.cvar = &cv_rr_rumble_fastfall_bounce}, 0, 0},

	{IT_STRING | IT_CVAR, "Tailwhip", "Toggle controller rumble when you charge a tailwhip.",
		NULL, {.cvar = &cv_rr_rumble_tailwhip}, 0, 0},

	{IT_STRING | IT_CVAR, "Wavedash", "Toggle controller rumble when your wavedash charge passes the minimum threshold.",
		NULL, {.cvar = &cv_rr_rumble_wavedash}, 0, 0},
};

// HUD
menuitem_t OPTIONS_RadioRacersHud[] =
{	
	{IT_HEADER, "Custom HUD Options", NULL,
		NULL, {NULL}, 0, 0},	

	{IT_STRING | IT_CVAR, "Ring Counter Position", "Toggle the RING COUNTER's HUD position.",
		NULL, {.cvar = &cv_ringsonplayer}, 0, 0},

	{IT_STRING | IT_CVAR, "Use Higher Resolution Portraits", "Draw higher resolution portraits in the minirankings.",
		NULL, {.cvar = &cv_hud_usehighresportraits}, 0, 0},

	{IT_HEADER, "Roulette Options", NULL,
		NULL, {NULL}, 0, 0},	

	{IT_STRING | IT_CVAR, "Roulette Layout", "Change the HUD layout for drawing the item/ring roluette.",
		NULL, {.cvar = &cv_rouletteonplayer}, 0, 0},

	{IT_STRING | IT_CVAR, "Ring Roulette Scale", "Choose a scale to draw the RING ROULETTE at.",
		NULL, {.cvar = &cv_ringbox_roulette_player_scale}, 0, 0},

	{IT_STRING | IT_CVAR, "Ring Roulette Position", "Choose where the RING ROULETTE should be positioned.",
		NULL, {.cvar = &cv_ringbox_roulette_player_position}, 0, 0},

	{IT_STRING | IT_CVAR, "Item Roulette Scale", "Choose a scale to draw the ITEM ROULETTE at.",
		NULL, {.cvar = &cv_item_roulette_player_scale}, 0, 0},

	{IT_STRING | IT_CVAR, "Item Roulette Position", "Choose where the ITEM ROULETTE should be positioned.",
		NULL, {.cvar = &cv_item_roulette_player_position}, 0, 0},

	{IT_HEADER, "Hide HUD Elements", NULL,
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
	Roulette_OnChange,
	NULL,
	NULL,
};

void RumbleEvents_OnChange(void)
{
	if (con_startup) return;

	UINT16 newstatus = (cv_morerumbleevents.value) ? IT_STRING | IT_CVAR : IT_GRAYEDOUT;

	for (int i = 4; i < 11; i++) {
		OPTIONS_RadioRacersMenu[i].status = newstatus;
	}

	if (!cv_morerumbleevents.value)
	{
		if (localPlayerWavedashClickTimer > 0)
			localPlayerWavedashClickTimer = 0;

		if (localPlayerJustBootyBounced)
			localPlayerJustBootyBounced = false;
	}
}

void Roulette_OnChange(void)
{
	if (con_startup) return;

	UINT16 newstatus = (cv_rouletteonplayer.value) ? IT_STRING | IT_CVAR : IT_GRAYEDOUT;

	for (int i = 5; i < 9; i++) {
		OPTIONS_RadioRacersHud[i].status = newstatus;
	}
}

menu_t OPTIONS_RadioRacersMenuDef = {
	sizeof (OPTIONS_RadioRacersMenu) / sizeof (menuitem_t),
	&OPTIONS_MainDef,
	0,
	OPTIONS_RadioRacersMenu,
	48, 80,
	SKINCOLOR_BANANA, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawGenericOptions,
	M_DrawOptionsCogs,
	M_OptionsTick,
	RumbleEvents_OnChange,
	NULL,
	NULL
};