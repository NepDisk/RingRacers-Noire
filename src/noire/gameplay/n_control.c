// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../n_control.h"
#include "../n_soc.h"

// v2 almost broke sliptiding when it fixed turning bugs!
// This value is fine-tuned to feel like v1 again without reverting any of those changes.
#define SLIPTIDEHANDLING 7*FRACUNIT/8

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

void KV1_UpdatePlayerAngle(player_t *player)
{
	INT16 angle_diff, max_left_turn, max_right_turn;
	boolean add_delta = true;
	ticcmd_t *cmd = &player->cmd;
	angle_t anglechange = player->angleturn;
	int i;
	UINT8 p = UINT8_MAX;

	for (i = 0; i <= splitscreen; i++)
	{
		if (player == &players[g_localplayers[i]])
		{
			p = i;
			break;
		}
	}

	player->steering = cmd->turning; // Set this so functions that rely on steering still work.

	if (N_GetKartTurnValue(player, KART_FULLTURN) != 0)
	{
		player->lturn_max[leveltime%MAXPREDICTTICS] = N_GetKartTurnValue(player, KART_FULLTURN)+1;
		player->rturn_max[leveltime%MAXPREDICTTICS] = N_GetKartTurnValue(player, -KART_FULLTURN)-1;
	}
	else
	{
		player->rturn_max[leveltime%MAXPREDICTTICS] = player->lturn_max[leveltime%MAXPREDICTTICS] = 0;
	}

	// KART: Don't directly apply angleturn! It may have been either A) forged by a malicious client, or B) not be a smooth turn due to a player dropping frames.
	// Instead, turn the player only up to the amount they're supposed to turn.
	angle_diff = cmd->angle - (player->mo->angle>>16);

	max_left_turn = player->lturn_max[(leveltime + MAXPREDICTTICS - cmd->latency) % MAXPREDICTTICS];
	max_right_turn = player->rturn_max[(leveltime + MAXPREDICTTICS - cmd->latency) % MAXPREDICTTICS];

	//CONS_Printf("----------------\nangle diff: %d - turning options: %d to %d - ", angle_diff, max_left_turn, max_right_turn);

	if (angle_diff > max_left_turn)
		angle_diff = max_left_turn;
	else if (angle_diff < max_right_turn)
		angle_diff = max_right_turn;
	else
	{
		// Try to keep normal turning as accurate to 1.0.1 as possible to reduce replay desyncs.
		anglechange = cmd->angle<<TICCMD_REDUCE;
		add_delta = false;
	}
	//CONS_Printf("applied turn: %d\n", angle_diff);

	if (add_delta) {
		anglechange += angle_diff<<TICCMD_REDUCE;
		anglechange &= ~0xFFFF; // Try to keep the turning somewhat similar to how it was before?
		//CONS_Printf("leftover turn (%s): %5d or %4d%%\n",
						//player_names[player-players],
						//(INT16) (cmd->angle - (player->mo->angle>>TICCMD_REDUCE)),
						//(INT16) (cmd->angle - (player->mo->angle>>TICCMD_REDUCE)) * 100 / (angle_diff ? angle_diff : 1));
	}


	// CONS_Printf("Playerid:%d\n",p);

	if (p == UINT8_MAX)
	{
		// When F12ing players, set local angle directly.
		P_SetPlayerAngle(player, anglechange + (N_GetKartTurnValue(player, cmd->turning) << TICCMD_REDUCE));
		player->angleturn = anglechange;
		player->mo->angle = player->angleturn;
	}
	else
	{
		player->angleturn = anglechange;
		player->mo->angle = player->angleturn;
	}

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

	// Compat level for Noire 1.0 Replays
	if (G_CompatLevel(0x1000))
	{
		return basedrift + (FixedMul(driftadjust * FRACUNIT, countersteer) / FRACUNIT);
	}
	else
	{
		return basedrift + FixedMul(driftadjust, countersteer);
	}
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

	if (N_UseLegacyStart() && (leveltime <= starttime))
	{
		// No turning during Legacy Start
		return 0;
	}

	// Staff ghosts - direction-only trickpanel behavior
	if (G_CompatLevel(0x000A) || K_PlayerUsesBotMovement(player) || (player->nflags & NF_OLDTRICKS))
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

	if ((currentSpeed <= 0) // Not moving
	&& (K_PressingEBrake(player) == false) // Not e-braking
	&& (player->respawn.state == RESPAWNST_NONE) // Not respawning
	&& (player->curshield != KSHIELD_TOP) // Not riding a Top
	&& (P_IsObjectOnGround(player->mo) == true)) // On the ground
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

	fixed_t finalhandleboost = player->handleboost;

	// If you're sliptiding, don't interact with handling boosts.
	// You need turning power proportional to your speed, no matter what!
	fixed_t topspeed = K_GetKartSpeed(player, false, false);
	if (K_Sliptiding(player))
	{
		fixed_t sliptide_handle;

		if (G_CompatLevel(0x000A))
		{
			// Compat level for 2.0 staff ghosts
			sliptide_handle = 5 * SLIPTIDEHANDLING / 4;
		}
		else
		{
			sliptide_handle = 3 * SLIPTIDEHANDLING / 4;
		}

		finalhandleboost = FixedMul(sliptide_handle, FixedDiv(player->speed, topspeed));
	}

	if (finalhandleboost > 0 && player->respawn.state == RESPAWNST_NONE)
	{
		turnfixed = FixedMul(turnfixed, FRACUNIT + finalhandleboost);
	}


	if (player->curshield == KSHIELD_TOP || !cv_ng_underwaterhandling.value) //NOIRE: Take into consideration the underwater handling cvar
		;
	else if (player->mo->eflags & (MFE_UNDERWATER))
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
	if (!G_CompatLevel(0x000A) && !K_PlayerUsesBotMovement(player) && !(player->nflags & NF_OLDTRICKS))
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

	mo->eflags |= MFE_SPRUNG;

	if (mo->eflags & MFE_VERTICALFLIP)
		vertispeed *= -1;

	if (vertispeed == 0)
	{
		fixed_t thrust;

		if (mo->player)
		{
			thrust = 3*mo->player->speed/2;
			if (thrust < 48<<FRACBITS)
				thrust = 48<<FRACBITS;
			if (thrust > 72<<FRACBITS)
				thrust = 72<<FRACBITS;
			if (mo->player->pogospring != 2)
			{
				if (mo->player->sneakertimer)
					thrust = FixedMul(thrust, 5*FRACUNIT/4);
				else if (mo->player->invincibilitytimer)
					thrust = FixedMul(thrust, 9*FRACUNIT/8);
			}
		}
		else
		{
			thrust = FixedDiv(3*P_AproxDistance(mo->momx, mo->momy)/2, 5*FRACUNIT/2);
			if (thrust < 16<<FRACBITS)
				thrust = 16<<FRACBITS;
			if (thrust > 32<<FRACBITS)
				thrust = 32<<FRACBITS;
		}

		mo->momz = P_MobjFlip(mo)*FixedMul(FINESINE(ANGLE_22h>>ANGLETOFINESHIFT), FixedMul(thrust, vscale));
	}
	else
		mo->momz = FixedMul(vertispeed, vscale);

	if (mo->eflags & MFE_UNDERWATER)
		mo->momz = (117 * mo->momz) / 200;

	if (sound)
		S_StartSound(mo, (sound == 1 ? sfx_kc2f : sfx_kpogos));
}

void N_LegacyStart(player_t *player)
{

	if (leveltime <= starttime)
		player->nocontrol = 1;

	// Start charging once you're given the opportunity.
	if (leveltime >= starttime-(2*TICRATE) && leveltime <= starttime)
	{
		if (player->cmd.buttons & BT_ACCELERATE)
		{
			if (player->boostcharge == 0)
				player->boostcharge = player->cmd.latency;

			player->boostcharge++;
		}
		else
			player->boostcharge = 0;
	}

	// Increase your size while charging your engine.
	if (leveltime < starttime+10)
	{
		player->mo->scalespeed = mapobjectscale/12;
		player->mo->destscale = mapobjectscale + (FixedMul(mapobjectscale, player->boostcharge*131));
		if ((player->pflags & PF_SHRINKME) && !modeattacking && !player->bot)
			player->mo->destscale = (6*player->mo->destscale)/8;
	}

	// Determine the outcome of your charge.
	if (leveltime > starttime && player->boostcharge)
	{
		// Not even trying?
		if (player->boostcharge < 35)
		{
			if (player->boostcharge > 17)
				S_StartSound(player->mo, sfx_cdfm00); // chosen instead of a conventional skid because it's more engine-like
		}
		// Get an instant boost!
		else if (player->boostcharge <= 50)
		{
			player->dropdashboost = (50-player->boostcharge)+20;

			if (player->boostcharge <= 36)
			{
				player->startboost = 0;
				K_DoSneaker(player, 0);
				player->sneakertimer = 70; // PERFECT BOOST!!

				if (!player->floorboost || player->floorboost == 3) // Let everyone hear this one
					S_StartSound(player->mo, sfx_s25f);
			}
			else
			{
				K_SpawnDashDustRelease(player); // already handled for perfect boosts by K_DoSneaker
				if ((!player->floorboost || player->floorboost == 3) && P_IsPartyPlayer(player))
				{
					if (player->boostcharge <= 40)
						S_StartSound(player->mo, sfx_cdfm01); // You were almost there!
					else
						S_StartSound(player->mo, sfx_s23c); // Nope, better luck next time.
				}
			}
		}
		// You overcharged your engine? Those things are expensive!!!
		else if (player->boostcharge > 50)
		{
			player->spinouttimer = 40;
			player->nocontrol = 40;
			//S_StartSound(player->mo, sfx_kc34);
			S_StartSound(player->mo, sfx_s3k83);
			//player->pflags |= PF_SKIDDOWN; // cheeky pflag reuse
		}

		player->boostcharge = 0;
	}

}

