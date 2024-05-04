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

#ifdef __cplusplus
extern "C" {
#endif

void N_UpdatePlayerAngle(player_t *player);
INT16 N_GetKartTurnValue(player_t *player, INT16 turnvalue);
INT16 N_GetKartDriftValue(player_t *player, fixed_t countersteer);

#ifdef __cplusplus
} // extern "C"
#endif

#endif