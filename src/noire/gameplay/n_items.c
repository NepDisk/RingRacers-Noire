
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
#include "../n_cvar.h"
#include "../../g_demo.h"
#include "../../k_grandprix.h"

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

void N_DoShrink(player_t *user)
{
	INT32 i;

	S_StartSound(user->mo, sfx_kc46); // Sound the BANG!
	K_SetItemCooldown(KITEM_SHRINK, TICRATE*30);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator || !players[i].mo)
			continue;
		if (&players[i] == user)
			continue;
		if (players[i].position < user->position)
		{

			// Grow should get taken away.
			if (players[i].growshrinktimer > 0)
				K_RemoveGrowShrink(&players[i]);
			// Don't hit while invulnerable!
			else if (!players[i].invincibilitytimer
				&& players[i].growshrinktimer <= 0
				&& !players[i].hyudorotimer)
			{
				// Start shrinking!
				if (cv_ng_oldshrinkitemdrop.value)
					K_DropItems(&players[i]);
				players[i].growshrinktimer = -(20*TICRATE);

				if (players[i].mo && !P_MobjWasRemoved(players[i].mo))
				{
					if (!G_CompatLevel(0x1000))
					{
						players[i].mo->scalespeed = mapobjectscale/TICRATE;
						players[i].mo->destscale = FixedMul(mapobjectscale, SHRINK_SCALE);

						if (K_PlayerShrinkCheat(players[i].mo->player) == true)
						{
							players[i].mo->destscale = FixedMul(players[i].mo->destscale, SHRINK_SCALE);
						}
					}
					else
					{
						players[i].mo->scalespeed = mapobjectscale/TICRATE;
						players[i].mo->destscale = (6*mapobjectscale)/8;
						if (K_PlayerShrinkCheat(&players[i]) == true)
						{
							players[i].mo->destscale = (6*players[i].mo->destscale)/8;
						}

					}
				}
			}
		}
	}
}


UINT8 N_NoireItemOddsRace[NUMKARTRESULTS-1][8] =
{
	{ 0, 0, 2, 3, 4, 0, 0, 0 }, // Sneaker
	{ 0, 0, 0, 0, 0, 3, 5, 6 }, // Rocket Sneaker
	{ 0, 0, 0, 0, 0, 4, 5, 7 }, // Invincibility
	{ 5, 3, 1, 0, 0, 0, 0, 0 }, // Banana
	{ 1, 2, 0, 0, 0, 0, 0, 0 }, // Eggman Monitor
	{ 6, 4, 2, 2, 0, 0, 0, 0 }, // Orbinaut
	{ 0, 4, 2, 1, 0, 0, 0, 0 }, // Jawz
	{ 0, 2, 3, 1, 0, 0, 0, 0 }, // Mine
	{ 3, 0, 0, 0, 0, 0, 0, 0 }, // Land Mine
	{ 0, 0, 2, 1, 0, 0, 0, 0 }, // Ballhog
	{ 0, 1, 2, 3, 4, 2, 2, 0 }, // Self-Propelled Bomb
	{ 0, 0, 0, 0, 2, 3, 1, 0 }, // Grow
	{ 0, 0, 0, 0, 0, 0, 2, 0 }, // Shrink
	{ 1, 2, 0, 0, 0, 0, 0, 0 }, // Lightning Shield
	{ 0, 1, 2, 1, 0, 0, 0, 0 }, // Bubble Shield
	{ 0, 0, 0, 0, 0, 1, 3, 5 }, // Flame Shield
	{ 0, 0, 0, 1, 1, 0, 0, 0 }, // Hyudoro
	{ 0, 0, 0, 0, 0, 0, 0, 0 }, // Pogo Spring
	{ 2, 1, 1, 1, 0, 0, 0, 0 }, // Super Ring
	{ 0, 0, 0, 0, 0, 0, 0, 0 }, // Kitchen Sink
	{ 3, 0, 0, 0, 0, 0, 0, 0 }, // Drop Target
	{ 0, 0, 0, 0, 1, 2, 2, 0 }, // Garden Top
	{ 0, 0, 0, 0, 0, 0, 0, 0 }, // Gachabom
	{ 0, 0, 1, 2, 2, 1, 0, 0 }, // Sneaker x2
	{ 0, 0, 0, 1, 4, 4, 5, 0 }, // Sneaker x3
	{ 0, 1, 1, 0, 0, 0, 0, 0 }, // Banana x3
	{ 0, 0, 1, 0, 0, 0, 0, 0 }, // Orbinaut x3
	{ 0, 0, 0, 1, 1, 0, 0, 0 }, // Orbinaut x4
	{ 0, 0, 1, 2, 1, 0, 0, 0 }, // Jawz x2
	{ 0, 0, 0, 0, 0, 0, 0, 0 }  // Gachabom x3
};

boolean N_CapsuleItemEnabled(kartitems_t item)
{
	if (item < 1 || item >= NUMKARTITEMS)
	{
		// Not a real item.
		return cv_capsuleitems[KITEM_SUPERRING - 1].value;
	}

	// Allow the user preference.
	return cv_capsuleitems[item - 1].value;
}
