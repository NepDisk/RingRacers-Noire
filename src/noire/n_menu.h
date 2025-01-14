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
extern menuitem_t OPTIONS_NoireGameplayPresets[];
extern menuitem_t OPTIONS_NoireGameplayRings[];
extern menuitem_t OPTIONS_NoireGameplayItems[];
extern menuitem_t OPTIONS_NoireGameplayItemFunctionality[];
extern menuitem_t OPTIONS_NoireGameplayMechanics[];
extern menuitem_t OPTIONS_NoireGameplayInstawhip[];
extern menuitem_t OPTIONS_NoireGameplaySpindash[];
extern menuitem_t OPTIONS_NoireGameplayLives[];
extern menuitem_t OPTIONS_NoireGameplayDriving[];
extern menuitem_t OPTIONS_NoireGameplayBots[];
extern menuitem_t OPTIONS_NoireGameplayRivals[];
extern menuitem_t OPTIONS_NoireGameplayAttacks[];
extern menuitem_t OPTIONS_NoireGameplayMaps[];
extern menuitem_t OPTIONS_NoireGameplayCapsuleItems[];
extern menu_t OPTIONS_NoireGameplayDef;
extern menu_t OPTIONS_NoireGameplayPresetsDef;
extern menu_t OPTIONS_NoireGameplayRingsDef;
extern menu_t OPTIONS_NoireGameplayItemsDef;
extern menu_t OPTIONS_NoireGameplayItemFunctionalityDef;
extern menu_t OPTIONS_NoireGameplayMechanicsDef;
extern menu_t OPTIONS_NoireGameplayInstawhipDef;
extern menu_t OPTIONS_NoireGameplayLivesDef;
extern menu_t OPTIONS_NoireGameplaySpindashDef;
extern menu_t OPTIONS_NoireGameplayDrivingDef;
extern menu_t OPTIONS_NoireGameplayBotsDef;
extern menu_t OPTIONS_NoireGameplayRivalsDef;
extern menu_t OPTIONS_NoireGameplayAttackDef;
extern menu_t OPTIONS_NoireGameplayMapsDef;
extern menu_t OPTIONS_NoireGameplayCapsuleItemsDef;

extern menuitem_t OPTIONS_Noire[];
extern menu_t OPTIONS_NoireDef;

extern menuitem_t OPTIONS_NoireProfileAccessibility[];
extern menu_t OPTIONS_NoireProfileAccessibilityDef;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
