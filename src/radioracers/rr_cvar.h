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
extern consvar_t cv_ringsonplayer;      // Rings drawn on player
extern consvar_t cv_ringsonplayeryoffset; // Y offset for rings drawn on player
extern consvar_t cv_rouletteonplayer;   // Item/Ring Roulette drawn on player

// Extra customization
extern consvar_t cv_ringbox_roulette_player_scale;
extern consvar_t cv_ringbox_roulette_player_position;
extern consvar_t cv_item_roulette_player_scale;
extern consvar_t cv_item_roulette_player_position;
// Controller Rumble Toggles
extern consvar_t cv_morerumbleevents;           // Extra gameplay events considered for controller rumble
extern consvar_t cv_rr_rumble_wall_bump;        // Wall Bump
extern consvar_t cv_rr_rumble_fastfall_bounce;  // Fastfall Bounce
extern consvar_t cv_rr_rumble_drift;            // Drift
extern consvar_t cv_rr_rumble_spindash;         // Spindash
extern consvar_t cv_rr_rumble_tailwhip;         // Tailwhip
extern consvar_t cv_rr_rumble_rings;            // Rings
extern consvar_t cv_rr_rumble_wavedash;         // Wavedash

// HUD
extern consvar_t cv_translucenthud;    // Self-explanatory; controls HUD translucency 
extern consvar_t cv_hud_hidecountdown; // Hide the bigass letters at the start of the race
extern consvar_t cv_hud_hideposition;  // Hide the bigass position bulbs at the start of the race
extern consvar_t cv_hud_hidelapemblem; // Hide the bigass lap emblem when you start a new lap

void RumbleEvents_OnChange(void);
void Roulette_OnChange(void);
  
#ifdef __cplusplus
} // extern "C"
#endif

#endif
