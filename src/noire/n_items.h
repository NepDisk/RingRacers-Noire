// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2020 by KartKrew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_ITEMS__
#define __N_ITEMS__

#include "../d_player.h"
#include "../doomdef.h"
#include "../doomtype.h"
#include "../g_game.h"
#include "../k_kart.h"
#include "../k_respawn.h"
#include "../p_local.h"
#include "../r_main.h"
#include "../m_easing.h"
#include "../s_sound.h" //S_StartSound
#include "n_cvar.h"

#ifdef __cplusplus
extern "C" {
#endif

void N_DoHyudoroSteal(player_t *player);
void N_DoShrink(player_t *user);

extern UINT8 N_NoireItemOddsRace[NUMKARTRESULTS-1][8];

#ifdef __cplusplus
} // extern "C"
#endif

#endif
