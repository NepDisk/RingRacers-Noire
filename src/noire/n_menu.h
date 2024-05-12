// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_MENU__
#define __N_MENU__

#include "../k_menu.h"
#include "../m_cond.h"
#include "../command.h"
#include "../console.h"
#include "../g_state.h" //For the tripwire toggle

// Noire
#include "n_cvar.h"

#ifdef __cplusplus
extern "C" {
#endif

extern menuitem_t OPTIONS_NoireGameplay[];
extern menuitem_t OPTIONS_NoireGameplayRings[];
extern menuitem_t OPTIONS_NoireGameplayItems[];
extern menuitem_t OPTIONS_NoireGameplayMechanics[];
extern menuitem_t OPTIONS_NoireGameplayInstawhip[];
extern menuitem_t OPTIONS_NoireGameplaySpindash[];
extern menuitem_t OPTIONS_NoireGameplayDriving[];
extern menuitem_t OPTIONS_NoireGameplayBots[];
extern menuitem_t OPTIONS_NoireGameplayRivals[];
extern menu_t OPTIONS_NoireGameplayDef;
extern menu_t OPTIONS_NoireGameplayRingsDef;
extern menu_t OPTIONS_NoireGameplayItemsDef;
extern menu_t OPTIONS_NoireGameplayMechanicsDef;
extern menu_t OPTIONS_NoireGameplayInstawhipDef;
extern menu_t OPTIONS_NoireGameplaySpindashDef;
extern menu_t OPTIONS_NoireGameplayDrivingDef;
extern menu_t OPTIONS_NoireGameplayBotsDef;
extern menu_t OPTIONS_NoireGameplayRivalsDef;

extern menuitem_t OPTIONS_Noire[];
extern menu_t OPTIONS_NoireDef;

//Character Menu stuff
//Def and functions
extern menu_t PLAY_CharSelect1PDef;
void M_Character1PSelect(INT32);
void M_DrawCharacter1PSelect(void);
void M_Character1PSelectTick(void);
void M_Character1PSelectInit(void);
boolean M_Character1PSelectQuit(void);
boolean M_Character1PSelectHandler(INT32);

UINT8 M_GetSkinIndexGivenPos(setup_player_t* p);

struct setup_nestedchar {
    UINT8 parentSkinId; //Skin ID of this (the parent).
    UINT8 childNum; // Amount of items in childrenSkinIds
    UINT8 *childrenSkinIds; //Skin IDs that will be hold in this instance. Needs to be dynamically allocated!
};

extern struct setup_flatchargrid_s {
    UINT8 sortingMode; 					// How we are currently sorting
	UINT8 numSkins; 					// Length of skinList, as skinList is dynamically allocated.
	boolean isExtended; 				// Is SkinList expanded right now or not, showing children as individual items outside of their parents.
    struct setup_nestedchar *skinList; // Skins that we'll have
} setup_flatchargrid;

//NOTES:
//Is allocating skinList dynamically worth it? we WILl eventually have the same amount of data as numskins which is 255 or whatever
//The only reason would be saving up on struct data since children will only get its ids saved, instead of childNum and parentSkinId as well..
//But then we need to do stupid search functions to find skins as we don't know how or when or in which order they got loaded into the array. is that worth it?
//Make an union? Something? make a setup_nestedchar only have parentSkinId if its a child? no idea.

#ifdef __cplusplus
} // extern "C"
#endif

#endif
