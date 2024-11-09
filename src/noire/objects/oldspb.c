// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2018-2024 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../n_object.h"
#include "../../m_random.h"
#include "../../k_kart.h"
#include "../../p_local.h"
#include "../../k_respawn.h"
#include "../../k_specialstage.h"

// These are not the OG functions, these were ported from v1
void Obj_SPBOldThink(mobj_t *mobj)
{
	K_SetItemCooldown(KITEM_SPB,20*TICRATE);

	mobj_t *ghost = P_SpawnGhostMobj(mobj);
    ghost->fuse = 3;
    if (mobj->target && !P_MobjWasRemoved(mobj->target) && mobj->target->player)
    {
        ghost->color = mobj->target->player->skincolor;
        ghost->colorized = true;
    }
    if (mobj->threshold > 0)
        mobj->threshold--;
}

void Obj_SPBOldTouch(mobj_t *spb, mobj_t *toucher)
{

	if ((spb->target == toucher || spb->target == toucher->target) && (spb->threshold > 0))
		return;

	if (spb->health <= 0 || spb->health <= 0)
		return;

	if (toucher->player && toucher->player->spectator)
		return;

	if (toucher->player && toucher->player->bubbleblowup > 0)
	{
		// Stun the SPB, and remove the shield.
		K_PopPlayerShield(toucher->player);
		K_DropHnextList(toucher->player);
		spb->extravalue1 = 3;
		spb->extravalue2 = 55; // Slightly over the respawn timer length
		return;
	}

	if (spb->tracer && !P_MobjWasRemoved(spb->tracer) && toucher == spb->tracer)
	{
		mobj_t *spbexplode;

		if (toucher->player->invincibilitytimer > 0 || toucher->player->growshrinktimer > 0 || toucher->player->hyudorotimer > 0)
		{
			//player->powers[pw_flashing] = 0;
			K_DropHnextList(toucher->player);
			K_StripItems(toucher->player);
		}

		S_StopSound(spb); // Don't continue playing the gurgle or the siren

		spbexplode = P_SpawnMobj(toucher->x, toucher->y, toucher->z, MT_SPBEXPLOSION);
		spbexplode->extravalue1 = 1; // Tell K_ExplodePlayer to use extra knockback
		if (spb->target && !P_MobjWasRemoved(spb->target))
			P_SetTarget(&spbexplode->target, spb->target);

		P_RemoveMobj(spb);
	}
	else
		P_DamageMobj(toucher, spb->target, 0, 1, DMG_NORMAL);
}

void Obj_SPBChase(mobj_t *spb)
{
	mobj_t *bestMobj = NULL;
	UINT8 i;
	UINT8 bestrank = UINT8_MAX;
	fixed_t dist;
	angle_t hang, vang;
	fixed_t wspeed, xyspeed, zspeed;

	// Default speed
	wspeed = spb->movefactor;

	if (spb->threshold) // Just fired, go straight.
	{
		spb->lastlook = -1;
		spbplace = -1;
		P_InstaThrust(spb, spb->angle, wspeed);
		spb->flags &=  ~MF_NOCLIPTHING;	// just in case.
		return;
	}

	// Find the player with the best rank
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator || players[i].exiting)
			continue; // not in-game

		/*if (!players[i].mo)
			continue; // no mobj

		if (players[i].mo->health <= 0)
			continue; // dead

		if (players[i].kartstuff[k_respawn])
			continue;*/ // respawning

		if (players[i].position < bestrank)
		{
			bestrank = players[i].position;
			bestMobj = players[i].mo;
		}
	}

	if (specialstageinfo.valid == true && specialstageinfo.ufo != NULL)
		bestMobj = specialstageinfo.ufo;

	if (spb->extravalue1 == 1) // MODE: TARGETING
	{
		if (spb->tracer && spb->tracer->health && !(spb->tracer->player && spb->tracer->player->spectator))
		{

			fixed_t defspeed = wspeed;
			fixed_t range = (160*spb->tracer->scale);
			fixed_t cx = 0, cy =0;

			// we're tailing a player, now's a good time to regain our damage properties
			spb->flags &=  ~MF_NOCLIPTHING;

			// Play the intimidating gurgle
			if (!S_SoundPlaying(spb, spb->info->activesound))
				S_StartSound(spb, spb->info->activesound);

			// Maybe we want SPB to target an object later? IDK lol
			if (spb->tracer->player)
			{
				UINT8 fracmax = 32;
				UINT8 spark = ((10-spb->tracer->player->kartspeed) + spb->tracer->player->kartweight) / 2;
				fixed_t easiness = ((spb->tracer->player->kartspeed + (10-spark)) << FRACBITS) / 2;

				fixed_t scaleAdjust = FRACUNIT;
				if (spb->tracer->scale > mapobjectscale)
					scaleAdjust = GROW_PHYSICS_SCALE;
				if (spb->tracer->scale < mapobjectscale)
					scaleAdjust = SHRINK_PHYSICS_SCALE;

				spb->lastlook = spb->tracer->player-players; // Save the player num for death scumming...

				if (!P_IsObjectOnGround(spb->tracer) /*&& !spb->tracer->player->kartstuff[k_pogospring]*/)
				{
					// In the air you have no control; basically don't hit unless you make a near complete stop
					defspeed = (7 * spb->tracer->player->speed) / 8;
				}
				else
				{
					// 7/8ths max speed for Knuckles, 3/4ths max speed for min accel, exactly max speed for max accel
					defspeed = FixedMul(((fracmax+1)<<FRACBITS) - easiness, FixedMul(K_GetKartSpeed(spb->tracer->player, false, false), scaleAdjust)) / fracmax;
				}

				// Be fairer on conveyors
				cx = spb->tracer->player->cmomx;
				cy = spb->tracer->player->cmomy;

				// Switch targets if you're no longer 1st for long enough
				if (spb->tracer->player->position <= bestrank)
					spb->extravalue2 = 7*TICRATE;
				else if (spb->extravalue2-- <= 0)
					spb->extravalue1 = 0; // back to SEEKING

				spbplace = spb->tracer->player->position;
			}
			else
			{
				spbplace = 1;
			}

			dist = P_AproxDistance(P_AproxDistance(spb->x-spb->tracer->x, spb->y-spb->tracer->y), spb->z-spb->tracer->z);

			wspeed = FixedMul(defspeed, FRACUNIT + FixedDiv(dist-range, range));
			if (wspeed < defspeed)
				wspeed = defspeed;
			if (wspeed > (3*defspeed)/2)
				wspeed = (3*defspeed)/2;
			if (wspeed < 20*spb->tracer->scale)
				wspeed = 20*spb->tracer->scale;
			if (spb->tracer->player && spb->tracer->player->carry & CR_SLIDING)
				wspeed = spb->tracer->player->speed/2;
			//  ^^^^ current section: These are annoying, and grand metropolis in particular needs this.

			hang = R_PointToAngle2(spb->x, spb->y, spb->tracer->x, spb->tracer->y);
			vang = R_PointToAngle2(0, spb->z, dist, spb->tracer->z);

			// Modify stored speed
			if (wspeed > spb->cvmem)
				spb->cvmem += (wspeed - spb->cvmem) / TICRATE;
			else
				spb->cvmem = wspeed;

			{
				// Smoothly rotate horz angle
				angle_t input = hang - spb->angle;
				boolean invert = (input > ANGLE_180);
				if (invert)
					input = InvAngle(input);

				// Slow down when turning; it looks better and makes U-turns not unfair
				xyspeed = FixedMul(spb->cvmem, max(0, (((180<<FRACBITS) - AngleFixed(input)) / 90) - FRACUNIT));

				input = FixedAngle(AngleFixed(input)/4);
				if (invert)
					input = InvAngle(input);

				spb->angle += input;

				// Smoothly rotate vert angle
				input = vang - spb->movedir;
				invert = (input > ANGLE_180);
				if (invert)
					input = InvAngle(input);

				// Slow down when turning; might as well do it for momz, since we do it above too
				zspeed = FixedMul(spb->cvmem, max(0, (((180<<FRACBITS) - AngleFixed(input)) / 90) - FRACUNIT));

				input = FixedAngle(AngleFixed(input)/4);
				if (invert)
					input = InvAngle(input);

				spb->movedir += input;
			}

			spb->momx = cx + FixedMul(FixedMul(xyspeed, FINECOSINE(spb->angle>>ANGLETOFINESHIFT)), FINECOSINE(spb->movedir>>ANGLETOFINESHIFT));
			spb->momy = cy + FixedMul(FixedMul(xyspeed, FINESINE(spb->angle>>ANGLETOFINESHIFT)), FINECOSINE(spb->movedir>>ANGLETOFINESHIFT));
			spb->momz = FixedMul(zspeed, FINESINE(spb->movedir>>ANGLETOFINESHIFT));

			// Red speed lines for when it's gaining on its target. A tell for when you're starting to lose too much speed!
			if (R_PointToDist2(0, 0, spb->momx, spb->momy) > (spb->tracer->player ? (16*spb->tracer->player->speed)/15
				: (16*R_PointToDist2(0, 0, spb->tracer->momx, spb->tracer->momy))/15) // Going faster than the target
				&& xyspeed > (spb->tracer->player ? K_GetKartSpeed(spb->tracer->player, false, false) : K_GetKartSpeedFromStat(5))/4) // Don't display speedup lines at pitifully low speeds
			{
				mobj_t *fast = P_SpawnMobj(spb->x + (P_RandomRange(PR_DECORATION,-24,24) * spb->scale),
					spb->y + (P_RandomRange(PR_DECORATION,-24,24) * spb->scale),
					spb->z + (spb->height/2) + (P_RandomRange(PR_DECORATION,-24,24) * spb->scale),
					MT_FASTLINE);
				fast->angle = R_PointToAngle2(0, 0, spb->momx, spb->momy);
				fast->color = SKINCOLOR_RED;
				fast->colorized = true;
				P_SetTarget(&fast->target, spb); // easier lua access
				K_MatchGenericExtraFlags(fast, spb);
			}

			return;
		}
		else // Target's gone, return to SEEKING
		{
			P_SetTarget(&spb->tracer, NULL);
			spb->extravalue1 = 2; // WAIT...
			spb->extravalue2 = 52; // Slightly over the respawn timer length
			return;
		}
	}
	else if (spb->extravalue1 == 2) // MODE: WAIT...
	{
		spb->momx = spb->momy = spb->momz = 0; // Stoooop

		// don't hurt players that have nothing to do with this:
		spb->flags |= MF_NOCLIPTHING;

		if (spb->lastlook != -1
			&& playeringame[spb->lastlook]
			&& !players[spb->lastlook].spectator
			&& !players[spb->lastlook].exiting)
		{
			spbplace = players[spb->lastlook].position;
			if (spb->extravalue2-- <= 0 && players[spb->lastlook].mo)
			{
				P_SetTarget(&spb->tracer, players[spb->lastlook].mo);
				spb->extravalue1 = 1; // TARGET ACQUIRED
				spb->extravalue2 = 7*TICRATE;
				spb->cvmem = wspeed;
			}
		}
		else
		{
			spb->extravalue1 = 0; // SEEKING
			spb->extravalue2 = 0;
			spbplace = -1;
		}
	}
	else // MODE: SEEKING
	{
		spb->lastlook = -1; // Just make sure this is reset

		if (bestMobj == NULL
			|| P_MobjWasRemoved(bestMobj) == true
			|| bestMobj->health <= 0
			|| (bestMobj->player != NULL && bestMobj->player->respawn.state != RESPAWNST_NONE))
		{
			// No one there? Completely STOP.
			spb->momx = spb->momy = spb->momz = 0;

			if (bestMobj == NULL)
			{
				spbplace = -1;
			}

			return;
		}

		// Found someone, now get close enough to initiate the slaughter...

		// don't hurt players that have nothing to do with this:
		spb->flags |= MF_NOCLIPTHING;

		P_SetTarget(&spb->tracer, bestMobj);
		spbplace = bestrank;

		dist = P_AproxDistance(P_AproxDistance(spb->x-spb->tracer->x, spb->y-spb->tracer->y), spb->z-spb->tracer->z);

		hang = R_PointToAngle2(spb->x, spb->y, spb->tracer->x, spb->tracer->y);
		vang = R_PointToAngle2(0, spb->z, dist, spb->tracer->z);

		{
			// Smoothly rotate horz angle
			angle_t input = hang - spb->angle;
			boolean invert = (input > ANGLE_180);
			if (invert)
				input = InvAngle(input);

			// Slow down when turning; it looks better and makes U-turns not unfair
			xyspeed = FixedMul(wspeed, max(0, (((180<<FRACBITS) - AngleFixed(input)) / 90) - FRACUNIT));

			input = FixedAngle(AngleFixed(input)/4);
			if (invert)
				input = InvAngle(input);

			spb->angle += input;

			// Smoothly rotate vert angle
			input = vang - spb->movedir;
			invert = (input > ANGLE_180);
			if (invert)
				input = InvAngle(input);

			// Slow down when turning; might as well do it for momz, since we do it above too
			zspeed = FixedMul(wspeed, max(0, (((180<<FRACBITS) - AngleFixed(input)) / 90) - FRACUNIT));

			input = FixedAngle(AngleFixed(input)/4);
			if (invert)
				input = InvAngle(input);

			spb->movedir += input;
		}

		spb->momx = FixedMul(FixedMul(xyspeed, FINECOSINE(spb->angle>>ANGLETOFINESHIFT)), FINECOSINE(spb->movedir>>ANGLETOFINESHIFT));
		spb->momy = FixedMul(FixedMul(xyspeed, FINESINE(spb->angle>>ANGLETOFINESHIFT)), FINECOSINE(spb->movedir>>ANGLETOFINESHIFT));
		spb->momz = FixedMul(zspeed, FINESINE(spb->movedir>>ANGLETOFINESHIFT));

		if (dist <= (3072*spb->tracer->scale)) // Close enough to target?
		{
			S_StartSound(spb, spb->info->attacksound); // Siren sound; might not need this anymore, but I'm keeping it for now just for debugging.
			spb->extravalue1 = 1; // TARGET ACQUIRED
			spb->extravalue2 = 7*TICRATE;
			spb->cvmem = wspeed;
		}
	}

	return;
}
