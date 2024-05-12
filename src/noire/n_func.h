// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2020 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_FUNC__
#define __N_FUNC__

#include "../byteptr.h"
#include "../d_player.h"
#include "../doomdef.h"
#include "../g_game.h"
#include "../k_kart.h"
#include "../k_respawn.h"
#include "../p_local.h"
#include "../r_main.h"
#include "../s_sound.h" //S_StartSound
#include "../k_objects.h"

#ifdef __cplusplus
extern "C" {
#endif

void N_UpdatePlayerAngle(player_t *player);
INT16 N_GetKartTurnValue(player_t *player, INT16 turnvalue);
INT16 N_GetKartDriftValue(const player_t *player, fixed_t countersteer);
void N_DoPogoSpring(mobj_t* mo, fixed_t vertispeed, UINT8 sound);


void K_KartLegacyUpdatePosition(player_t *player);
mobj_t *P_GetObjectTypeInSectorNum(mobjtype_t type, size_t s);

void add_element(void **array, size_t *size, size_t elem_size, void *new_elem);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
