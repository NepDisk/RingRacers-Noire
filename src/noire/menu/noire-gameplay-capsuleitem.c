// RING RACERS NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2024 by Vivian "toastergrl" Grannell.
// Copyright (C) 2024 by Kart Krew.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../n_menu.h"
#include "../n_cvar.h"
#include "../../s_sound.h"
#include "../../z_zone.h"
#include "../../k_kart.h"
#include "../../k_hud.h"
#include "../../m_easing.h"

menuitem_t OPTIONS_NoireGameplayCapsuleItems[] =
{
	// Mostly handled by the drawing function.
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Super Ring",			NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_SUPERRING, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Self-Propelled Bomb",	NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_SPB, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Toggle All ", NULL, {.routine = M_HandleCapsuleItemToggles}, 0, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Sneaker",				NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_SNEAKER, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Rocket Sneaker",		NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_ROCKETSNEAKER, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Banana",				NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_BANANA, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Eggmark",				NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_EGGMAN, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Gachabom",				NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_GACHABOM, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Orbinaut",				NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_ORBINAUT, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Jawz",					NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_JAWZ, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Proximity Mine",		NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_MINE, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Ballhog",				NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_BALLHOG, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Hyudoro",				NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_HYUDORO, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Drop Target",			NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_DROPTARGET, sfx_s258},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Land Mine",				NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_LANDMINE, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Pogo Spring",		 	NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_POGOSPRING, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Invincibility",			NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_INVINCIBILITY, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Grow",					NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_GROW, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Shrink",				NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_SHRINK, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Gardentop",		 		NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_GARDENTOP, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Lightning Shield",		NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_LIGHTNINGSHIELD, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Bubble Shield",			NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_BUBBLESHIELD, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Flame Shield",			NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_FLAMESHIELD, 0},
	{IT_KEYHANDLER | IT_NOTHING, NULL, "Kitchen Sink",			NULL, {.routine = M_HandleCapsuleItemToggles}, KITEM_KITCHENSINK, 0}
};

static void init_routine(void)
{
	// Since this menu can be accessed from different
	// locations. (currentMenu has not changed yet.)
	OPTIONS_GameplayItemsDef.prevMenu = currentMenu;
}

static void M_DrawCapsuleItemToggles(void)
{
	static UINT8 black[256];
	memset(black, 16, 256);

	const INT32 edges = 8;
	const INT32 height = 4;
	const INT32 spacing = 35;
	const INT32 column = itemOn/height;
	//const INT32 row = itemOn%height;
	INT32 leftdraw, rightdraw, totaldraw;
	INT32 x = currentMenu->x + M_EaseWithTransition(Easing_Linear, 5 * 64), y = currentMenu->y;
	INT32 onx = 0, ony = 0;
	consvar_t *cv;
	INT32 i, drawnum;
	patch_t *pat;

	M_DrawMenuTooltips();
	M_DrawOptionsMovingButton();

	// Find the available space around column
	leftdraw = rightdraw = column;
	totaldraw = 0;
	for (i = 0; (totaldraw < edges*2 && i < edges*4); i++)
	{
		if (rightdraw+1 < (currentMenu->numitems/height)+1)
		{
			rightdraw++;
			totaldraw++;
		}
		if (leftdraw-1 >= 0)
		{
			leftdraw--;
			totaldraw++;
		}
	}

	patch_t *isbg = W_CachePatchName("K_ISBG", PU_CACHE);
	patch_t *isbgd = W_CachePatchName("K_ISBGD", PU_CACHE);
	patch_t *ismul = W_CachePatchName("K_ISMUL", PU_CACHE);
	patch_t *isstrk = W_CachePatchName("K_ISSTRK", PU_CACHE);

	for (i = leftdraw; i <= rightdraw; i++)
	{
		INT32 j;

		for (j = 0; j < height; j++)
		{
			const INT32 thisitem = (i*height)+j;

			if (thisitem >= currentMenu->numitems)
				break;

			if (thisitem == itemOn)
			{
				onx = x;
				ony = y;
				y += spacing;
				continue;
			}

			if (currentMenu->menuitems[thisitem].mvar1 == 0)
			{
				V_DrawScaledPatch(x, y, 0, isbg);
				V_DrawScaledPatch(x, y, 0, W_CachePatchName("K_ISTOGL", PU_CACHE));
				y += spacing;
				continue;
			}

			if (currentMenu->menuitems[thisitem].mvar1 == 255)
			{
				V_DrawScaledPatch(x, y, 0, isbgd);
				y += spacing;
				continue;
			}

			cv = &cv_capsuleitems[currentMenu->menuitems[thisitem].mvar1-1];

			drawnum = K_ItemResultToAmount(currentMenu->menuitems[thisitem].mvar1);

			V_DrawScaledPatch(x, y, 0, cv->value ? isbg : isbgd);

			if (drawnum > 1)
				V_DrawScaledPatch(x, y, 0, ismul);

			pat = W_CachePatchName(K_GetItemPatch(currentMenu->menuitems[thisitem].mvar1, true), PU_CACHE);

			V_DrawScaledPatch(x, y, 0, pat);

			if (!cv->value)
				V_DrawMappedPatch(x, y, V_MODULATE, pat, black);

			if (drawnum > 1)
			{
				V_DrawString(x+24, y+31, 0, va("x%d", drawnum));

				if (!cv->value)
					DrawMappedString(x+24, y+31, V_MODULATE, HU_FONT, va("x%d", drawnum), black);
			}

			if (!cv->value)
				V_DrawScaledPatch(x, y, 0, isstrk);

			y += spacing;
		}

		x += spacing;
		y = currentMenu->y;
	}

	{
		if (currentMenu->menuitems[itemOn].mvar1 == 0)
		{
			V_DrawScaledPatch(onx-1, ony-2, 0, W_CachePatchName("K_ITBG", PU_CACHE));
			V_DrawScaledPatch(onx-1, ony-2, 0, W_CachePatchName("K_ITTOGL", PU_CACHE));
		}
		else if (currentMenu->menuitems[itemOn].mvar1 == 255)
		{
			V_DrawScaledPatch(onx-1, ony-2, 0, W_CachePatchName("K_ITBGD", PU_CACHE));
			if (shitsfree)
			{
				INT32 trans = V_TRANSLUCENT;
				if (shitsfree-1 > TICRATE-5)
					trans = ((10-TICRATE)+shitsfree-1)<<V_ALPHASHIFT;
				else if (shitsfree < 5)
					trans = (10-shitsfree)<<V_ALPHASHIFT;
				V_DrawScaledPatch(onx-1, ony-2, trans, W_CachePatchName("K_ITFREE", PU_CACHE));
			}
		}
		else
		{
			cv = &cv_capsuleitems[currentMenu->menuitems[itemOn].mvar1-1];

			drawnum = K_ItemResultToAmount(currentMenu->menuitems[itemOn].mvar1);

			if (cv->value)
				V_DrawScaledPatch(onx-1, ony-2, 0, W_CachePatchName("K_ITBG", PU_CACHE));
			else
				V_DrawScaledPatch(onx-1, ony-2, 0, W_CachePatchName("K_ITBGD", PU_CACHE));

			if (drawnum > 1)
				V_DrawScaledPatch(onx-1, ony-2, 0, W_CachePatchName("K_ITMUL", PU_CACHE));

			pat = W_CachePatchName(K_GetItemPatch(currentMenu->menuitems[itemOn].mvar1, false), PU_CACHE);

			V_DrawScaledPatch(onx-1, ony-2, 0, pat);

			if (!cv->value)
				V_DrawMappedPatch(onx-1, ony-2, V_MODULATE, pat, black);

			if (drawnum > 1)
			{
				V_DrawScaledPatch(onx+27, ony+39, 0, W_CachePatchName("K_ITX", PU_CACHE));
				V_DrawTimerString(onx+37, ony+34, 0, va("%d", drawnum));

				if (!cv->value)
				{
					V_DrawMappedPatch(onx+27, ony+39, V_MODULATE, W_CachePatchName("K_ITX", PU_CACHE), black);
					DrawMappedString(onx+37, ony+34, V_MODULATE, TIMER_FONT, va("%d", drawnum), black);
				}
			}

			if (!cv->value)
			{
				V_DrawScaledPatch(onx-1, ony-2, 0, W_CachePatchName("K_ITSTRK", PU_CACHE));
			}
		}
	}
}

menu_t OPTIONS_NoireGameplayCapsuleItemsDef = {
	sizeof (OPTIONS_NoireGameplayCapsuleItems) / sizeof (menuitem_t),
	&OPTIONS_NoireGameplayItemsDef,
	0,
	OPTIONS_NoireGameplayCapsuleItems,
	50, 40,
	SKINCOLOR_BLACK, 0,
	MBF_DRAWBGWHILEPLAYING,
	NULL,
	2, 5,
	M_DrawCapsuleItemToggles,
	M_DrawOptionsCogs,
	M_OptionsTick,
	init_routine,
	NULL,
	NULL,
};

static boolean M_AnyCapsuleItemsEnabled(void)
{
	INT32 i;
	for (i = 0; i < NUMKARTITEMS-1; i++)
	{
		if (cv_capsuleitems[i].value)
			return true;
	}
	return false;
}

static void M_ToggleCapsules(void)
{
	CV_SetValue(&cv_ng_capsules, M_AnyCapsuleItemsEnabled());
}

void M_HandleCapsuleItemToggles(INT32 choice)
{
	const INT32 width = 8, height = 4;
	INT32 column = itemOn/height, row = itemOn%height;
	INT16 next;
	UINT8 i;
	boolean exitmenu = false;
	const UINT8 pid = 0;

	(void) choice;

	if (menucmd[pid].dpad_lr > 0)
	{
		S_StartSound(NULL, sfx_s3k5b);
		column++;
		if (((column*height)+row) >= currentMenu->numitems)
			column = 0;
		next = min(((column*height)+row), currentMenu->numitems-1);
		itemOn = next;

		M_SetMenuDelay(pid);
	}

	else if (menucmd[pid].dpad_lr < 0)
	{
		S_StartSound(NULL, sfx_s3k5b);
		column--;
		if (column < 0)
			column = width-1;
		if (((column*height)+row) >= currentMenu->numitems)
			column--;
		next = max(((column*height)+row), 0);
		if (next >= currentMenu->numitems)
			next = currentMenu->numitems-1;
		itemOn = next;

		M_SetMenuDelay(pid);
	}

	else if (menucmd[pid].dpad_ud > 0)
	{
		S_StartSound(NULL, sfx_s3k5b);
		row = (row+1) % height;
		if (((column*height)+row) >= currentMenu->numitems)
			row = 0;
		next = min(((column*height)+row), currentMenu->numitems-1);
		itemOn = next;

		M_SetMenuDelay(pid);
	}

	else if (menucmd[pid].dpad_ud < 0)
	{
		S_StartSound(NULL, sfx_s3k5b);
		row = (row-1) % height;
		if (row < 0)
			row = height-1;
		if (((column*height)+row) >= currentMenu->numitems)
			row--;
		next = max(((column*height)+row), 0);
		if (next >= currentMenu->numitems)
			next = currentMenu->numitems-1;
		itemOn = next;

		M_SetMenuDelay(pid);
	}

	else if (M_MenuConfirmPressed(pid))
	{
		M_SetMenuDelay(pid);
		if (currentMenu->menuitems[itemOn].mvar1 == 255)
		{
			//S_StartSound(NULL, sfx_s26d);
			if (!shitsfree)
			{
				shitsfree = TICRATE;
				S_StartSound(NULL, sfx_itfree);
			}
		}
		else
		if (currentMenu->menuitems[itemOn].mvar1 == 0)
		{
			INT32 v = !M_AnyCapsuleItemsEnabled();
			S_StartSound(NULL, sfx_s1b4);
			for (i = 0; i < NUMKARTITEMS-1; i++)
			{
				CV_SetValue(&cv_capsuleitems[i], v);
			}
			M_ToggleCapsules();
		}
		else
		{
			if (currentMenu->menuitems[itemOn].mvar2)
			{
				S_StartSound(NULL, currentMenu->menuitems[itemOn].mvar2);
			}
			else
			{
				S_StartSound(NULL, sfx_s1ba);
			}
			CV_AddValue(&cv_capsuleitems[currentMenu->menuitems[itemOn].mvar1-1], 1);
			M_ToggleCapsules();
		}
	}

	else if (M_MenuBackPressed(pid))
	{
		M_SetMenuDelay(pid);
		exitmenu = true;
	}

	if (exitmenu)
	{
		if (currentMenu->prevMenu)
			M_SetupNextMenu(currentMenu->prevMenu, false);
		else
			M_ClearMenus(true);
	}
}
