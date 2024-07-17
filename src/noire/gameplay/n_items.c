
// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2020 by KartKrew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../n_items.h"
#include "../../m_random.h"

void N_DoHyudoroSteal(player_t *player)
{
	INT32 i, numplayers = 0;
	INT32 playerswappable[MAXPLAYERS];
	INT32 stealplayer = -1; // The player that's getting stolen from
	INT32 prandom = 0;
	boolean sink = P_RandomChance(PR_ITEM_ROULETTE,FRACUNIT/64); // Figure out how to use PR_HYUDORO

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].mo && players[i].mo->health > 0 && players[i].playerstate == PST_LIVE
			&& player != &players[i] && !players[i].exiting && !players[i].spectator // Player in-game

			// Can steal from this player
			&& ((gametyperules & GTR_CIRCUIT) //&& players[i].kartstuff[k_position] < player->kartstuff[k_position])
			|| ((gametyperules & GTR_BUMPERS) && players[i].mo->health > 0))

			// Has an item
			&& players[i].itemtype
			&& players[i].itemamount
			&& (!(players[i].itemflags & IF_ITEMOUT)))
			//&& !players[i].karthud[khud_itemblink]))
		{
			playerswappable[numplayers] = i;
			numplayers++;
		}
	}

	prandom = P_RandomFixed(PR_ITEM_ROULETTE); // Figure out how to use PR_HYUDORO
	S_StartSound(player->mo, sfx_s3k92);

	if (sink && numplayers > 0 && cv_items[KITEM_KITCHENSINK].value) // BEHOLD THE KITCHEN SINK
	{
		player->hyudorotimer = hyudorotime;
		player->stealingtimer = stealtime;

		player->itemtype = KITEM_KITCHENSINK;
		player->itemamount = 1;
		player->itemflags &= ~IF_ITEMOUT;
		return;
	}
	else if (((gametyperules & GTR_CIRCUIT) && player->position == 1) || numplayers == 0) // No-one can be stolen from? Oh well...
	{
		player->hyudorotimer = hyudorotime;
		player->stealingtimer = stealtime;
		return;
	}
	else if (numplayers == 1) // With just 2 players, we just need to set the other player to be the one to steal from
	{
		stealplayer = playerswappable[numplayers-1];
	}
	else if (numplayers > 1) // We need to choose between the available candidates for the 2nd player
	{
		stealplayer = playerswappable[prandom%(numplayers-1)];
	}

	if (stealplayer > -1) // Now here's where we do the stealing, has to be done here because we still know the player we're stealing from
	{
		player->hyudorotimer = hyudorotime;
		player->stealingtimer = stealtime;
		players[stealplayer].stealingtimer = -stealtime;

		player->itemtype = players[stealplayer].itemtype;
		player->itemamount = players[stealplayer].itemamount;
		player->itemflags &= ~IF_ITEMOUT;

		players[stealplayer].itemtype = KITEM_NONE;
		players[stealplayer].itemamount = 0;
		players[stealplayer].itemflags &= ~IF_ITEMOUT;

		if (P_IsPartyPlayer(&players[stealplayer]) && !splitscreen)
			S_StartSound(NULL, sfx_s3k92);
	}
}
