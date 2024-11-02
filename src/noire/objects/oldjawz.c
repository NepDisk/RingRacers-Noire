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
#include "../n_control.h"

#define jawz_speed(o) ((o)->movefactor)
#define jawz_selfdelay(o) ((o)->threshold)
#define jawz_dropped(o) ((o)->flags2 & MF2_AMBUSH)
#define jawz_droptime(o) ((o)->movecount)

#define jawz_retcolor(o) ((o)->cvmem)
#define jawz_stillturn(o) ((o)->cusval)

#define jawz_owner(o) ((o)->target)
#define jawz_chase(o) ((o)->tracer)

static player_t *K_FindOldJawzTarget(mobj_t *actor, player_t *source)
{
	fixed_t best = -1;
	player_t *wtarg = NULL;
	INT32 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		angle_t thisang;
		player_t *player;

		if (!playeringame[i])
			continue;

		player = &players[i];

		if (player->spectator)
			continue; // spectator

		if (!player->mo)
			continue;

		if (player->mo->health <= 0)
			continue; // dead

		// Don't target yourself, stupid.
		if (player == source)
			continue;

		// Don't home in on teammates.
		if (G_GametypeHasTeams() && source->ctfteam == player->ctfteam)
			continue;

		// Invisible, don't bother
		if (player->hyudorotimer)
			continue;

		// Find the angle, see who's got the best.
		thisang = actor->angle - R_PointToAngle2(actor->x, actor->y, player->mo->x, player->mo->y);
		if (thisang > ANGLE_180)
			thisang = InvAngle(thisang);

		// Jawz only go after the person directly ahead of you in race... sort of literally now!
		if ((gametyperules & GTR_CIRCUIT))
		{
			// Don't go for people who are behind you
			if (thisang > ANGLE_67h)
				continue;
			// Don't pay attention to people who aren't above your position
			if (player->position >= source->position)
				continue;
			if ((best == -1) || (player->position > best))
			{
				wtarg = player;
				best = player->position;
			}
		}
		else
		{
			fixed_t thisdist;
			fixed_t thisavg;

			// Don't go for people who are behind you
			if (thisang > ANGLE_45)
				continue;

			// Don't pay attention to dead players
			//if (player->health <= 0)
			//	continue;

			// Z pos too high/low
			if (abs(player->mo->z - (actor->z + actor->momz)) > RING_DIST/8)
				continue;

			thisdist = P_AproxDistance(player->mo->x - (actor->x + actor->momx), player->mo->y - (actor->y + actor->momy));

			if (thisdist > 2*RING_DIST) // Don't go for people who are too far away
				continue;

			thisavg = (AngleFixed(thisang) + thisdist) / 2;

			//CONS_Printf("got avg %d from player # %d\n", thisavg>>FRACBITS, i);

			if ((best == -1) || (thisavg < best))
			{
				wtarg = player;
				best = thisavg;
			}
		}
	}

	return wtarg;
}


static void OBJ_JawzOldChase(mobj_t *actor)
{
	player_t *player;

	if (actor->tracer)
	{

		if (actor->tracer->health)
		{
			mobj_t *ret;

			ret = P_SpawnMobj(actor->tracer->x, actor->tracer->y, actor->tracer->z, MT_PLAYERRETICULE);
			P_SetTarget(&ret->target, actor->tracer);
			ret->old_x = actor->tracer->old_x;
			ret->old_y = actor->tracer->old_y;
			ret->old_z = actor->tracer->old_z;
			ret->frame |= ((leveltime % 10) / 2) + 5;
			ret->color = actor->cvmem;

			P_Thrust(actor, R_PointToAngle2(actor->x, actor->y, actor->tracer->x, actor->tracer->y), (7*actor->movefactor)/64);
			return;
		}
		else
			P_SetTarget(&actor->tracer, NULL);
	}

	if (actor->extravalue1) // Disable looking by setting this
		return;

	if (!actor->target || P_MobjWasRemoved(actor->target)) // No source!
		return;

	player = K_FindOldJawzTarget(actor, actor->target->player);
	if (player)
		P_SetTarget(&actor->tracer, player->mo);

	return;
}


void OBJ_JawzOldThink(mobj_t *mobj)
{

	//sector_t *sec2;
	fixed_t topspeed = mobj->movefactor;
	fixed_t distbarrier = 512*mapobjectscale;
	fixed_t distaway;
	boolean grounded = P_IsObjectOnGround(mobj);


	if (jawz_dropped(mobj))
	{
		if (grounded && (mobj->flags & MF_NOCLIPTHING))
		{
			mobj->momx = 1;
			mobj->momy = 0;
			S_StartSound(mobj, mobj->info->deathsound);
			mobj->flags &= ~MF_NOCLIPTHING;
		}

		return;
	}

	OBJ_JawzOldChase(mobj);
	P_SpawnGhostMobj(mobj);

	if (mobj->threshold > 0)
		mobj->threshold--;
	if (leveltime % TICRATE == 0)
		S_StartSound(mobj, mobj->info->activesound);

	if (gamespeed == 1 || gamespeed == 2)
		distbarrier = FixedMul(distbarrier, FRACUNIT+FRACUNIT/4);

	if ((gametyperules & GTR_CIRCUIT) && mobj->tracer)
	{
		distaway = P_AproxDistance(mobj->tracer->x - mobj->x, mobj->tracer->y - mobj->y);
		if (distaway < distbarrier)
		{
			if (mobj->tracer->player)
			{
				fixed_t speeddifference = abs(topspeed - min(mobj->tracer->player->speed, K_GetKartSpeed(mobj->tracer->player, false, false)));
				topspeed = topspeed - FixedMul(speeddifference, FRACUNIT-FixedDiv(distaway, distbarrier));
			}
		}
	}

	if ((gametyperules & GTR_BUMPERS))
	{
		mobj->friction -= 1228;
		if (mobj->friction > FRACUNIT)
			mobj->friction = FRACUNIT;
		if (mobj->friction < 0)
			mobj->friction = 0;
	}

	mobj->angle = R_PointToAngle2(0, 0, mobj->momx, mobj->momy);
	P_InstaThrust(mobj, mobj->angle, topspeed);

	if (mobj->tracer)
		mobj->angle = R_PointToAngle2(mobj->x, mobj->y, mobj->tracer->x, mobj->tracer->y);
	else
		mobj->angle = R_PointToAngle2(0, 0, mobj->momx, mobj->momy);

	K_DriftDustHandling(mobj);

	if (grounded) // Handle pogosprings.
	{
		if (P_MobjTouchingSectorSpecial(mobj, 3, 1) || P_MobjTouchingSectorSpecial(mobj, 3, 3) || (mobj->terrain && (mobj->terrain->pogoSpring > 0)))
			N_DoPogoSpring(mobj, 0, 1);
	}

}


void Obj_JawzOldThrown(mobj_t *th, fixed_t finalSpeed, SINT8 dir)
{
	INT32 lastTarg = -1;
	player_t *owner = NULL;

	if (jawz_owner(th) != NULL && P_MobjWasRemoved(jawz_owner(th)) == false
		&& jawz_owner(th)->player != NULL)
	{
		lastTarg = jawz_owner(th)->player->lastjawztarget;
		jawz_retcolor(th) = jawz_owner(th)->player->skincolor;
		owner = jawz_owner(th)->player;
	}
	else
	{
		jawz_retcolor(th) = SKINCOLOR_KETCHUP;
	}

	if (dir == -1)
	{
		;
	}
	else
	{
		if ((lastTarg >= 0 && lastTarg < MAXPLAYERS)
			&& playeringame[lastTarg] == true)
		{
			player_t *tryPlayer = &players[lastTarg];

			if (tryPlayer->spectator == false)
			{
				P_SetTarget(&jawz_chase(th), tryPlayer->mo);
			}
		}

		// Sealed Star: target the UFO immediately. I don't
		// wanna fuck with the lastjawztarget stuff, so just
		// do this if a target wasn't set.
		if (jawz_chase(th) == NULL || P_MobjWasRemoved(jawz_chase(th)) == true)
		{
			P_SetTarget(&jawz_chase(th), K_FindJawzTarget(th, owner, ANGLE_90));
		}
	}

	S_StartSound(th, th->info->activesound);
	jawz_speed(th) = finalSpeed;
}
