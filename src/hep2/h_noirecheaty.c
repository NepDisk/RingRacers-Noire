// Haya's Expansion Pack II for Dr. Robotnik's Ring Racers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by hayaUnderscore/haya

// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  hep2/h_noirecheaty.c
/// \brief HEP2 password shortcut menu

#include "h_cvars.h"
#include "../d_main.h"
#include "../d_netcmd.h"
#include "../v_video.h"
#include "../noire/n_menu.h"
#include "../m_cond.h"
#include "../m_misc.h"
#include "../k_menu.h"

menuitem_t OPTIONS_Cheaty[] =
{
	{IT_HEADER, "Cosmetic...", NULL,
		NULL, {NULL}, 0, 0},
		
	{IT_STRING | IT_CVAR, "Shitty Signs", "It's better because it's worse. Makes signs use alternate designs.",
		NULL, {.cvar = &cv_shittysigns}, 0, 0},
		
	{IT_STRING | IT_CVAR, "Big Head", "Works out at the library. Renders you and most objects 2x tall.",
		NULL, {.cvar = &cv_bighead}, 0, 0},
		
	{IT_STRING | IT_CVAR, "Tasteless Taunts", "Universally hated in dev. Similar to the \"Meme\" voices option in Kart.",
		NULL, {.cvar = &cv_tastelesstaunts}, 0, 0},
		
	{IT_SPACE | IT_DYBIGSPACE, NULL,  NULL,
		NULL, {NULL}, 0, 0},
	
	{IT_STRING | IT_LINKTEXT | IT_CALL, "Skidrow...", "Unlock EVERYTHING. Spoilers, and WILL affect your save\x85 PERMANENTLY.",
		NULL, {.routine = M_Skidrow}, 0, 0},
};

menu_t OPTIONS_CheatyDef = {
	sizeof (OPTIONS_Cheaty) / sizeof (menuitem_t),
	&OPTIONS_NoireDef,
	0,
	OPTIONS_Cheaty,
	48, 80,
	SKINCOLOR_BLACK, 0,
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