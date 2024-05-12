// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "n_func.h"
#include "n_cvar.h"
#include "../z_zone.h"

// Old update Player angle taken from old kart commits
void N_UpdatePlayerAngle(player_t* player)
{
	angle_t angleChange = N_GetKartTurnValue(player, player->cmd.turning) << TICCMD_REDUCE;
	player->steering = player->cmd.turning;
	UINT8 i;

	P_SetPlayerAngle(player, player->angleturn + angleChange);
	player->mo->angle = player->angleturn;

	if (!cv_allowmlook.value || player->spectator == false)
	{
		player->aiming = 0;
	}
	else
	{
		player->aiming += (player->cmd.aiming << TICCMD_REDUCE);
		player->aiming = G_ClipAimingPitch((INT32*) &player->aiming);
	}

	for (i = 0; i <= r_splitscreen; i++)
	{
		if (player == &players[displayplayers[i]])
		{
			localaiming[i] = player->aiming;
			break;
		}
	}
}

// countersteer is how strong the controls are telling us we are turning
// turndir is the direction the controls are telling us to turn, -1 if turning right and 1 if turning left
INT16 N_GetKartDriftValue(const player_t* player, fixed_t countersteer)
{
	INT16 basedrift, driftadjust;
	fixed_t driftweight = player->kartweight * 14; // 12

	if (player->drift == 0 || !P_IsObjectOnGround(player->mo))
	{
		// If they aren't drifting or on the ground, this doesn't apply
		return 0;
	}

	if (player->pflags & PF_DRIFTEND)
	{
		// Drift has ended and we are tweaking their angle back a bit
		return -266 * player->drift;
	}

	basedrift = (83 * player->drift) - (((driftweight - 14) * player->drift) / 5); // 415 - 303
	driftadjust = abs((252 - driftweight) * player->drift / 5);

	if (player->tiregrease > 0) // Buff drift-steering while in greasemode
	{
		basedrift += (basedrift / greasetics) * player->tiregrease;
	}

	if (player->mo->eflags & (MFE_UNDERWATER|MFE_TOUCHWATER) && cv_ng_underwaterhandling.value)
	{
		countersteer = FixedMul(countersteer, 3 * FRACUNIT / 2);
	}

	return basedrift + (FixedMul(driftadjust * FRACUNIT, countersteer) / FRACUNIT);
}

INT16 N_GetKartTurnValue(player_t* player, INT16 turnvalue)
{
	fixed_t p_maxspeed;
	fixed_t p_speed;
	fixed_t weightadjust;
	fixed_t turnfixed = turnvalue * FRACUNIT;
	fixed_t currentSpeed = 0;

	if (player->mo == NULL || P_MobjWasRemoved(player->mo))
	{
		return 0;
	}

	if (player->spectator || objectplacing)
	{
		return turnvalue;
	}

	if (leveltime < introtime)
	{
		return 0;
	}

	if (player->respawn.state == RESPAWNST_MOVE)
	{
		// No turning during respawn
		return 0;
	}

	// Staff ghosts - direction-only trickpanel behavior
	if (G_CompatLevel(0x000A) || K_PlayerUsesBotMovement(player))
	{
		if (player->trickpanel == TRICKSTATE_READY || player->trickpanel == TRICKSTATE_FORWARD)
		{
			// Forward trick or rising from trickpanel
			return 0;
		}
	}

	if (player->justDI > 0)
	{
		// No turning until you let go after DI-ing.
		return 0;
	}

	if (Obj_PlayerRingShooterFreeze(player) == true)
	{
		// No turning while using Ring Shooter
		return 0;
	}


	currentSpeed = R_PointToDist2(0, 0, player->mo->momx, player->mo->momy);

	if ((currentSpeed <= 0)											// Not moving
		&& ((player->cmd.buttons & BT_EBRAKEMASK) != BT_EBRAKEMASK) // not e-braking
		&& (player->respawn.state == RESPAWNST_NONE))				// Not respawning
	{
		return 0;
	}

	p_maxspeed = K_GetKartSpeed(player, false, false);
	p_speed = min(FixedHypot(player->mo->momx, player->mo->momy), (p_maxspeed * 2));
	weightadjust = FixedDiv((p_maxspeed * 3) - p_speed, (p_maxspeed * 3) + (player->kartweight * FRACUNIT));

	if (K_PlayerUsesBotMovement(player))
	{
		turnfixed = FixedMul(turnfixed, 5 * FRACUNIT / 4); // Base increase to turning
		turnfixed = FixedMul(turnfixed, K_BotRubberband(player));
	}

	if (player->drift != 0 && P_IsObjectOnGround(player->mo))
	{
		if (player->pflags & PF_DRIFTEND)
		{
			// Sal: K_GetKartDriftValue is short-circuited to give a weird additive magic number,
			// instead of an entirely replaced turn value. This gaslit me years ago when I was doing a
			// code readability pass, where I missed that fact because it also returned early.
			turnfixed += N_GetKartDriftValue(player, FRACUNIT) * FRACUNIT;
			return (turnfixed / FRACUNIT);

		}
		else
		{
			// If we're drifting we have a completely different turning value
			fixed_t countersteer = FixedDiv(turnfixed, KART_FULLTURN * FRACUNIT);
			return N_GetKartDriftValue(player, countersteer);

		}

	}

	if (player->handleboost > 0)
	{
		turnfixed = FixedMul(turnfixed, FRACUNIT + player->handleboost);
	}


	if (player->mo->eflags & (MFE_UNDERWATER|MFE_TOUCHWATER) && cv_ng_underwaterhandling.value)
	{
		turnfixed = FixedMul(turnfixed, 3 * FRACUNIT / 2);
	}

	// Weight has a small effect on turning
	turnfixed = FixedMul(turnfixed, weightadjust);

	// Side trick
	if (player->trickpanel == TRICKSTATE_LEFT || player->trickpanel == TRICKSTATE_RIGHT)
	{
		turnfixed /= 2;
	}

	// 2.2 - Presteering allowed in trickpanels
	if (!G_CompatLevel(0x000A) && !K_PlayerUsesBotMovement(player))
	{
		if (player->trickpanel == TRICKSTATE_READY || player->trickpanel == TRICKSTATE_FORWARD)
		{
			// Forward trick or rising from trickpanel
			turnfixed /= 2;
			if (player->tricklock)
				turnfixed /= (player->tricklock/2 + 1);
		}
	}


	return (turnfixed / FRACUNIT);
}

//
// N_DoPogoSpring
//
// Copy-pasted version of K_DoPogoSpring from Kart as-is with bare minimum modifications to work with RR
void N_DoPogoSpring(mobj_t* mo, fixed_t vertispeed, UINT8 sound)
{
	const fixed_t vscale = mapobjectscale + (mo->scale - mapobjectscale);

	if (mo->player && mo->player->spectator)
		return;

	if (mo->eflags & MFE_SPRUNG)
		return;

	mo->standingslope = NULL;
	mo->terrain = NULL; //Clear terrain

	mo->eflags |= MFE_SPRUNG;

	if (mo->eflags & MFE_VERTICALFLIP)
		vertispeed *= -1;

	if (vertispeed == 0)
	{
		fixed_t thrust;

		if (mo->player)
		{
			thrust = 3 * mo->player->speed / 2;
			if (thrust < 48 << FRACBITS)
				thrust = 48 << FRACBITS;
			if (thrust > 72 << FRACBITS)
				thrust = 72 << FRACBITS;
			if (mo->player->pogoSpringJumped && mo->player->pogoMaxSpeed == 0) // If its not speedcapped
			{
				if (mo->player->sneakertimer //|| mo->player->kartstuff[k_paneltimer]
				)
					thrust = FixedMul(thrust, 5 * FRACUNIT / 4);
				else if (mo->player->invincibilitytimer)
					thrust = FixedMul(thrust, 9 * FRACUNIT / 8);
			}
		}
		else
		{
			thrust = FixedDiv(3 * P_AproxDistance(mo->momx, mo->momy) / 2, 5 * FRACUNIT / 2);
			if (thrust < 16 << FRACBITS)
				thrust = 16 << FRACBITS;
			if (thrust > 32 << FRACBITS)
				thrust = 32 << FRACBITS;
		}

		mo->momz = P_MobjFlip(mo) * FixedMul(FINESINE(ANGLE_22h >> ANGLETOFINESHIFT), FixedMul(thrust, vscale));
	}
	else
		mo->momz = FixedMul(vertispeed, vscale);

	if (mo->eflags & MFE_UNDERWATER)
		mo->momz = (117 * mo->momz) / 200;

	if (sound)
		S_StartSound(mo, (sound == 1 ? sfx_kc2f : sfx_kpogos));
}

//
// N_DoPogoSpring
//
// Copy-pasted version of K_DoPogoSpring from Kart with more modifications to match RR closer.
/*
void N_DoPogoSpring(mobj_t* mo, fixed_t vertispeed, UINT8 sound)
{
	fixed_t thrust = 0; // Define thrust up here...
	boolean applyMomz = true;

	if (mo->player && mo->player->spectator)
		return;

	if (mo->eflags & MFE_SPRUNG)
		return;

	mo->standingslope = NULL;
	mo->terrain = NULL; // Clear terrain

	mo->eflags |= MFE_SPRUNG;

	// if (mo->eflags & MFE_VERTICALFLIP) //We do not need this as we are doing P_MobjFlip below
	//	vertispeed *= -1;

	//Handle vertispeed depending on its value...
	if (vertispeed == 0)
	{

	}
	else if (vertispeed < 0)
	{
		vertispeed = -vertispeed; //Make it positive
		applyMomz = false;
	}
	else
	{
		vertispeed = FixedMul(vertispeed, mapobjectscale);
	}

	thrust = vertispeed * P_MobjFlip(mo); // Flip it for ceiling gravity

	if (mo->player)
	{
		thrust = 3 * mo->player->speed / 2;
		if (thrust < 48 << FRACBITS)
			thrust = 48 << FRACBITS;
		if (thrust > 72 << FRACBITS)
			thrust = 72 << FRACBITS;
		if (mo->player->pogoSpringJumped) // If its not speedcapped
		{
			if (mo->player->sneakertimer)
				thrust = FixedMul(thrust, 5 * FRACUNIT / 4);
			else if (mo->player->invincibilitytimer)
				thrust = FixedMul(thrust, 9 * FRACUNIT / 8);
		}
	}
	else
	{
		thrust = FixedDiv(3 * P_AproxDistance(mo->momx, mo->momy) / 2, 5 * FRACUNIT / 2);
		if (thrust < 16 << FRACBITS)
			thrust = 16 << FRACBITS;
		if (thrust > 32 << FRACBITS)
			thrust = 32 << FRACBITS;
	}

	mo->momz = FixedMul(FINESINE(ANGLE_22h >> ANGLETOFINESHIFT), FixedMul(thrust, mapobjectscale));

	if (applyMomz)
	{
		mo->momz = FixedMul(thrust, mapobjectscale);

		if (mo->eflags & MFE_UNDERWATER)
			mo->momz = (117 * mo->momz) / 200;
	}

	if (sound)
		S_StartSound(mo, (sound == 1 ? sfx_kc2f : sfx_kpogos));
}*/

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
				for (mo = waypointcap; mo != NULL; mo = mo->tracer)
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

// Function to add an element to an array dynamically
void add_element(void **array, UINT8 *capacity, UINT8 typeSize, void *new_elem) {
    // If the array is not initialized
    if (*array == NULL) {
        *array = Z_Malloc(typeSize, PU_STATIC, NULL);
        if (*array == NULL) {
            // Handle memory allocation failure
            return; // Or any appropriate error handling
        }
        *capacity = 1;
        memcpy(*array, new_elem, typeSize); // Copy the new element
        return;
    }

    // Increase the size of the array
    *capacity++;

    // Reallocate memory for the array
    void *temp = Z_Realloc(*array, (*capacity) * typeSize, PU_STATIC, NULL);
    if (temp == NULL) {
        // Handle reallocation failure
        // Free the old memory block to avoid memory leaks
        Z_Free(*array);
        return; // Or any appropriate error handling
    }
    
    *array = temp; // Update array to point to the newly reallocated memory

    // Copy the new element to the end of the array
    void *dest = (char *)(*array) + ((*capacity - 1) * typeSize);
    memcpy(dest, new_elem, typeSize);
}