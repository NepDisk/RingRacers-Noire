// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2020 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_CONTROL__
#define __N_CONTROL__

#include "../d_player.h"
#include "../doomdef.h"
#include "../g_game.h"
#include "../k_kart.h"
#include "../k_respawn.h"
#include "../p_local.h"
#include "../r_main.h"
#include "../s_sound.h" //S_StartSound
#include "../k_objects.h"
#include "n_cvar.h"

#ifdef __cplusplus
extern "C" {
#endif

void N_UpdatePlayerAngle(player_t *player);
void KV1_UpdatePlayerAngle(player_t *player);
INT16 N_GetKartTurnValue(player_t *player, INT16 turnvalue);
INT16 N_GetKartDriftValue(const player_t *player, fixed_t countersteer);
void N_DoPogoSpring(mobj_t* mo, fixed_t vertispeed, UINT8 sound);
void N_PogoSidemove(player_t *player);
void N_LegacyStart(player_t *player);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
