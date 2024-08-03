// RadioRacers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by $HOME
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file radioracers/rr_cvar.h
/// \brief RadioRacers CVARs

// Separating custom functionality into new header and source code files is so much cleaner.    

#ifndef __RR_CVAR__
#define __RR_CVAR__

// consvar_t
#include "../command.h"

#ifdef __cplusplus
extern "C" {
#endif

// Player (Clientside)
extern consvar_t cv_votesnitch;         // Vote Snitch
extern consvar_t cv_morerumbleevents;   // Extra gameplay events considered for controller rumble
extern consvar_t cv_ringsonplayer;      // Rings drawn on player
extern consvar_t cv_rouletteonplayer;   // Item/Ring Roulette drawn on player

#ifdef __cplusplus
} // extern "C"
#endif

#endif