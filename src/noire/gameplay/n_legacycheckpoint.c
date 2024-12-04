// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2020 by KartKrew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../n_legacycheckpoint.h"

//
// K_KartUpdatePosition
//
void K_KartLegacyUpdatePosition(player_t *player)
{
	fixed_t position = 1;
	fixed_t oldposition = player->position;
	fixed_t i, ppcd, pncd, ipcd, incd;
	fixed_t pmo, imo;
	mobj_t *mo;

	if (player->spectator || !player->mo)
		return;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator || !players[i].mo)
			continue;

		//if (G_RaceGametype())
		{
			if ((((players[i].cheatchecknum) + (numcheatchecks + 1) * players[i].laps) >
				((player->cheatchecknum) + (numcheatchecks + 1) * player->laps)))
				position++;
			else if (((players[i].cheatchecknum) + (numcheatchecks+1)*players[i].laps) ==
				((player->cheatchecknum) + (numcheatchecks+1)*player->laps))
			{
				ppcd = pncd = ipcd = incd = 0;

				player->prevcheck = players[i].prevcheck = 0;
				player->nextcheck = players[i].nextcheck = 0;

				// This checks every thing on the map, and looks for MT_BOSS3WAYPOINT (the thing we're using for checkpoint wp's, for now)
				for (mo = boss3cap; mo != NULL; mo = mo->tracer)
				{
					pmo = P_AproxDistance(P_AproxDistance(	mo->x - player->mo->x,
															mo->y - player->mo->y),
															mo->z - player->mo->z) / FRACUNIT;
					imo = P_AproxDistance(P_AproxDistance(	mo->x - players[i].mo->x,
															mo->y - players[i].mo->y),
															mo->z - players[i].mo->z) / FRACUNIT;

					if (mo->health == player->cheatchecknum && (!mo->movecount || mo->movecount == player->laps+1))
					{
						player->prevcheck += pmo;
						ppcd++;
					}
					if (mo->health == (player->cheatchecknum + 1) && (!mo->movecount || mo->movecount == player->laps+1))
					{
						player->nextcheck += pmo;
						pncd++;
					}
					if (mo->health == players[i].cheatchecknum && (!mo->movecount || mo->movecount == players[i].laps+1))
					{
						players[i].prevcheck += imo;
						ipcd++;
					}
					if (mo->health == (players[i].cheatchecknum + 1) && (!mo->movecount || mo->movecount == players[i].laps+1))
					{
						players[i].nextcheck += imo;
						incd++;
					}
				}

				if (ppcd > 1) player->prevcheck /= ppcd;
				if (pncd > 1) player->nextcheck /= pncd;
				if (ipcd > 1) players[i].prevcheck /= ipcd;
				if (incd > 1) players[i].nextcheck /= incd;

				if ((players[i].nextcheck > 0 || player->nextcheck > 0) && !player->exiting)
				{
					if ((players[i].nextcheck - players[i].prevcheck) <
						(player->nextcheck - player->prevcheck))
						position++;
				}
				else if (!player->exiting)
				{
					if (players[i].prevcheck > player->prevcheck)
						position++;
				}
				else
				{
					if (players[i].cheatchecktime < player->cheatchecktime)
						position++;
				}
			}
		}
		/*else if (G_BattleGametype())
		{
			if (player->exiting) // Ends of match standings
			{
				if (players[i].marescore > player->marescore) // Only score matters
					position++;
			}
			else
			{
				if (players[i].kartstuff[k_bumper] == player->kartstuff[k_bumper] && players[i].marescore > player->marescore)
					position++;
				else if (players[i].kartstuff[k_bumper] > player->kartstuff[k_bumper])
					position++;
			}
		}*/
	}

	if (leveltime < starttime || oldposition == 0)
		oldposition = position;

	if (oldposition != position) // Changed places?
		player->positiondelay = 10; // Position number growth

	player->position = position;
}

mobj_t *P_GetObjectTypeInSectorNum(mobjtype_t type, size_t s)
{
	sector_t *sec = sectors + s;
	mobj_t *thing = sec->thinglist;

	while (thing)
	{
		if (thing->type == type)
			return thing;
		thing = thing->snext;
	}
	return NULL;
}
