// RadioRacers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by $HOME
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file radioracers/netgame/rr_votesnitch.c
/// \brief Vote Snitching - print out vote statuses in chat

#include "../rr_cvar.h"
#include "../rr_votesnitch.h"

#include "../../k_hud.h"
#include "../../g_game.h"
#include "../../hu_stuff.h"
#include "../../k_zvote.h"

boolean RR_UseVoteSnitch(void)
{
    return (cv_votesnitch.value);
}

// Who did it? Huh? Who's the filthy CUNT who voted redo for the fourth time in the row...
void RR_VoteSnitchNewVote(midVoteType_e type, player_t *victim, player_t *caller)
{
    const char *voteReason;
	const char *levelTitle = mapheaderinfo[gamemap-1]->lvlttl;
	switch (type){
		case MVT_KICK: // Kick
			voteReason = va("\x82KICK \x89%s \x86", player_names[victim - players]);
			break;
		case MVT_RTV: // Skip
			if (*levelTitle != '\0') {
				voteReason = va("\x82SKIP \x89%s", levelTitle);
			} else {
				voteReason = "\x82SKIP \x86the current level";
			}
			break;
		case MVT_RUNITBACK: // Redo
			if (*levelTitle != '\0') {
				voteReason = va("\x82REDO \x89%s", levelTitle);
			} else {
				voteReason = "\x82REDO \x86the current level";
			}
			break;
		default:
			return;
	}

	char *newVoteReason = strdup(voteReason);

    // e.g. [VOTE]: Player voted to REDO Death Egg.
    // e.g. [VOTE]: Player voted to SKIP Death Egg.
    // e.g. [VOTE]: Player voted to KICK Someone.
	HU_AddChatText(
		va("\x86[\x89VOTE\x86]: \x82%s\x86 voted to %s\x86.", player_names[caller - players], newVoteReason), 
		true
	);
}

// .. and WHO are the fuckers who keep voting yes?
void RR_VoteSnitchYesVotes(INT32 playernum)
{
    // e.g. [VOTE]: SlamDunk voted yes.
    if (playeringame[playernum]) {		
		HU_AddChatText(
			va("\x86[\x89VOTE\x86]: \x82%s\x86 voted \x82yes\x86.", player_names[playernum]), 
			true
		);
	}
}