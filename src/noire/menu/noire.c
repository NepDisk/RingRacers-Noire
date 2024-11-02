// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

// Noire
#include "../n_menu.h"
#include "../n_cvar.h"
#include "../../hep2/h_cvars.h"

#include "../../hep2/h_general.h"
#include "../../d_main.h"
#include "../../v_video.h"

static void confirm_warning(INT32 choice)
{
	if (choice == MA_YES)
	{
		CV_StealthSet(&cv_cheatywarning, "On");
		M_SetupNextMenu(&OPTIONS_CheatyDef, false);
	}
}

static void M_CheatyWarning(INT32 choice)
{
	(void)choice;
	
	if (!cv_cheatywarning.value)
		M_StartMessage("Forbidden territory!", M_GetText("Password spoilers ahead!\n\nIf you want to figure out the passwords yourself, please refrain from using this menu. Otherwise, if you're sure, go on right ahead.\n\n\x85This warning will not appear again when accepted,\n\x85so choose wisely!"), confirm_warning, MM_YESNO, "I accept", "I do not");
	else
		M_SetupNextMenu(&OPTIONS_CheatyDef, false);
}

menuitem_t OPTIONS_Noire[] =
{
	{IT_HEADER, "Colorized HUD...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Colorized Hud", "HUD will match player color.",
		NULL, {.cvar = &cv_colorizedhud}, 0, 0},

	{IT_STRING | IT_CVAR, "Colorized Itembox", "Itembox becomes colored as well.",
		NULL, {.cvar = &cv_colorizeditembox}, 0, 0},

	{IT_STRING | IT_CVAR, "Colorized Hud Color", "Use a different color for the colorized hud.",
		NULL, {.cvar = &cv_colorizedhudcolor}, 0, 0},

	{IT_HEADER, "Miscellaneous HUD...", NULL,
		NULL, {NULL}, 0, 0},
		
	{IT_STRING | IT_CVAR, "HUD Translucency", "Determines how transparent the hud is.",
		NULL, {.cvar = &cv_translucenthud}, 0, 0},

	{IT_STRING | IT_CVAR, "Hold Scoreboard Button", "Reverts the scoreboard button behavior to how it was in SRB2Kart.",
		NULL, {.cvar = &cv_holdscorebutt}, 0, 0},

	{IT_STRING | IT_CVAR, "Hi-res HUD Scale", "Determines the scale of the hud at higher resolutions.",
		NULL, {.cvar = &cv_highreshudscale}, 0, 0},

	{IT_STRING | IT_CVAR, "Kart Input Display", "Use the input display from SRB2Kart.",
		NULL, {.cvar = &cv_oldinputdisplay}, 0, 0},

	{IT_STRING | IT_CVAR, "Kart Position Display", "Use the position display from SRB2Kart.",
		NULL, {.cvar = &cv_oldpositiondisplay}, 0, 0},

	{IT_STRING | IT_CVAR, "Extended Speedometer", "When not using percentage, show it alongside your current.",
		NULL, {.cvar = &cv_extendedspeedometer}, 0, 0},

	{IT_STRING | IT_CVAR, "Hires Portrait", "Enable the use of the higher resolution character icons in some places",
		NULL, {.cvar = &cv_highresportrait}, 0, 0},
		
	{IT_HEADER, "Sound/Music...", NULL,
		NULL, {NULL}, 0, 0},
		
	{IT_STRING | IT_CVAR, "Power Music", "Toggles the power music during gameplay.",
		NULL, {.cvar = &cv_specialmusic}, 0, 0},
		
	{IT_STRING | IT_CVAR | IT_CV_STRING, "Title Screen Music", "Music lump to replace Fluvial Beat Deposits on the title screen.",
		NULL, {.cvar = &cv_menumusic}, 0, 0},

	{IT_STRING | IT_CVAR, "Spectator Music", "Toggles the ability to use separate music while spectating.",
		NULL, {.cvar = &cv_postracemusic}, 0, 0},

	{IT_STRING | IT_CVAR | IT_CV_STRING, "Spectator Music", "Music lump to use in spectator. Nothing or invalid lump means no music",
		NULL, {.cvar = &cv_menumusic}, 0, 0},
		
	{IT_STRING | IT_CVAR, "Final Rank Intermissions", "Toggles the ability to use separate intermission music lumps based on your final rank.",
		NULL, {.cvar = &cv_postracemusic}, 0, 0},
		
	{IT_STRING | IT_CVAR, "Finish Line Voices", "Plays the win/lose voiceline on race finish!",
		NULL, {.cvar = &cv_postracevoices}, 0, 0},
		
	{IT_HEADER, "Visuals...", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Visual Hopping", "Do a funny hop!",
		NULL, {.cvar = &cv_saltyhop}, 0, 0},
		
	{IT_SPACE | IT_DYBIGSPACE, NULL,  NULL,
		NULL, {NULL}, 0, 0},
		
	{IT_STRING | IT_LINKTEXT | IT_CALL, "Cheaty...", "Options locked by passwords. Spoilers!",
		NULL, {.routine = M_CheatyWarning}, 0, 0},
};

void ColorHUD_OnChange(void)
{
	if (con_startup) return;

	if (cv_colorizedhud.value && clr_hud)
	{
		for (int i = 2; i < 4; i++)
			OPTIONS_Noire[i].status = IT_STRING | IT_CVAR;
	}
	else
	{
		for (int i = 2; i < 4; i++)
			OPTIONS_Noire[i].status = IT_GRAYEDOUT;
	}
}

menu_t OPTIONS_NoireDef = {
	sizeof (OPTIONS_Noire) / sizeof (menuitem_t),
	&OPTIONS_MainDef,
	0,
	OPTIONS_Noire,
	48, 80,
	SKINCOLOR_BLACK, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawGenericOptions,
	M_DrawOptionsCogs,
	M_OptionsTick,
	ColorHUD_OnChange,
	NULL,
	NULL,
};
