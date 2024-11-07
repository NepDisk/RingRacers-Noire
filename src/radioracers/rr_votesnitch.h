// RadioRacers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by $HOME
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file radioracers/netgame/rr_votesnitch.h
/// \brief Vote Snitching - Functions

#ifndef __RR_VOTESNITCH__
#define _RR_VOTESNITCH__

#ifdef __cplusplus
extern "C" {
#endif

#include "../k_hud.h"
#include "../g_game.h"
#include "../k_zvote.h"

boolean RR_UseVoteSnitch(void);
void RR_VoteSnitchNewVote(midVoteType_e type, player_t *victim, player_t *caller);
void RR_VoteSnitchYesVotes(INT32 playernum);

#ifdef __cplusplus
} // extern "C"
#endif
#endif