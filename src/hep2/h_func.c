// HAYA'S EXPANSION PACK 2
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by haya3218.

// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  hep2/h_func.c
/// \brief Helper functions

#include "h_general.h"
#include "../z_zone.h"

#include "../d_main.h"
#include "../d_netcmd.h"
#include "../m_cond.h"
#include "../m_misc.h"
#include "../k_menu.h"
#include "../s_sound.h"
#include "../m_random.h"
#include "../music.h"
#include "../g_game.h"
#include "../r_fps.h"
#include "../r_main.h"

// Function to add an element to an array dynamically
void add_element(void **array, UINT8 *capacity, UINT8 typeSize, void *new_elem) {
    // If the array is not initialized
    if (*array == NULL) {
        *array = Z_Malloc(typeSize, PU_STATIC, NULL);
        if (*array == NULL) {
            // Handle memory allocation failure
            return; // Or any appropriate error handling
        }
        *capacity = 1;
        memcpy(*array, new_elem, typeSize); // Copy the new element
        return;
    }

    // Increase the size of the array
    *capacity++;

    // Reallocate memory for the array
    void *temp = Z_Realloc(*array, (*capacity) * typeSize, PU_STATIC, NULL);
    if (temp == NULL) {
        // Handle reallocation failure
        // Free the old memory block to avoid memory leaks
        Z_Free(*array);
        return; // Or any appropriate error handling
    }
    
    *array = temp; // Update array to point to the newly reallocated memory

    // Copy the new element to the end of the array
    void *dest = (char *)(*array) + ((*capacity - 1) * typeSize);
    memcpy(dest, new_elem, typeSize);
}

// Menu stuff

static void f_devmode()
{
	INT32 i;

	if (modifiedgame)
		return;

	// Just unlock all the things.
	for (i = 0; i < MAXUNLOCKABLES; i++)
	{
		if (!unlockables[i].conditionset)
			continue;
		gamedata->unlocked[i] = true;
	}

	// Unlock all hidden levels.
	for (i = 0; i < nummapheaders; i++)
	{
		mapheaderinfo[i]->records.mapvisited = MV_MAX;
	}

	gamedata->gonerlevel = GDGONER_DONE;
	gamedata->sealedswapalerted = true;

	// You have been warned!
	G_SaveGameData();
}

static void M_SkidrowAccept(INT32 choice)
{
	if (choice != MA_YES)
		return;

	skidrow.ticker = 0;
	skidrow.curline = 0;
	skidrow.linetime = 0;
	skidrow.waitline = 0;
	skidrow.bottomtext_line = 0;
	bottomtext_x = 320*FRACUNIT;
	skidrow_tick_frac = 0;
	checkerpatches[0] = (patch_t*)(W_CachePatchName("~027", PU_CACHE));
	checkerpatches[1] = (patch_t*)(W_CachePatchName("~020", PU_CACHE));
	checkerpatches[2] = (patch_t*)(W_CachePatchName("~000", PU_CACHE));
	checkerpatches[3] = (patch_t*)(W_CachePatchName("~022", PU_CACHE));
	
	// set current menu if we are not in goner
	if (currentMenu != &MAIN_GonerDef && (currentMenu != &MISC_DevmodeDef || currentMenu != &MISC_SkidrowDef))
	{
		MISC_DevmodeDef.prevMenu = currentMenu;
		MISC_SkidrowDef.prevMenu = currentMenu;
	}

	switch (M_RandomKey(2))
	{
		case 0: // Lemmings Final style
			M_SetupNextMenu(&MISC_SkidrowDef, false);
			Music_Remap("menu_nocred", "4MAT");
			break;
		default: // TODO: Original style
			M_SetupNextMenu(&MISC_DevmodeDef, false);
			Music_Remap("menu_nocred", "KYGN");
			break;
	}

	// Done here to avoid immediate music credit
	Music_Play("menu_nocred");
	
	f_devmode();
}

void M_Skidrow(INT32 choice)
{
	(void)choice; // SHUT UP
	
	M_StartMessage("AAAAACK!", M_GetText("This option will unlock\x85 EVERYTHING.\n\nAll unlockables are permanently saved to your save file. If you still want to unlock stuff, PLEASE do not proceed.\n\nYou have been warned."), M_SkidrowAccept, MM_YESNO, "Start cracktro", "Nevermind");
}