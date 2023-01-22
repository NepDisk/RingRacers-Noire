
#include "k_follower.h"

#include "k_kart.h"

#include "doomtype.h"
#include "doomdef.h"
#include "g_game.h"
#include "g_demo.h"
#include "r_main.h"
#include "r_skins.h"
#include "p_local.h"
#include "p_mobj.h"
#include "m_cond.h"

INT32 numfollowers = 0;
follower_t followers[MAXSKINS];

INT32 numfollowercategories;
followercategory_t followercategories[MAXFOLLOWERCATEGORIES];

CV_PossibleValue_t Followercolor_cons_t[MAXSKINCOLORS+3];	// +3 to account for "Match", "Opposite" & NULL

/*--------------------------------------------------
	INT32 K_FollowerAvailable(const char *name)

		See header file for description.
--------------------------------------------------*/
INT32 K_FollowerAvailable(const char *name)
{
	INT32 i;

	for (i = 0; i < numfollowers; i++)
	{
		if (stricmp(followers[i].name, name) == 0)
			return i;
	}

	return -1;
}

/*--------------------------------------------------
	INT32 K_FollowerUsable(INT32 followernum)

		See header file for description.
--------------------------------------------------*/
boolean K_FollowerUsable(INT32 skinnum)
{
	// Unlike R_SkinUsable, not netsynced.
	// Solely used to prevent an invalid value being sent over the wire.
	UINT8 i;
	INT32 fid;

	if (skinnum == -1 || demo.playback)
	{
		// Simplifies things elsewhere, since there's already plenty of checks for less-than-0...
		return true;
	}

	// Determine if this follower is supposed to be unlockable or not
	for (i = 0; i < MAXUNLOCKABLES; i++)
	{
		if (unlockables[i].type != SECRET_FOLLOWER)
			continue;

		fid = M_UnlockableFollowerNum(&unlockables[i]);

		if (fid != skinnum)
			continue;

		// i is now the unlockable index, we can use this later
		break;
	}

	if (i == MAXUNLOCKABLES)
	{
		// Didn't trip anything, so we can use this follower.
		return true;
	}

	// Use the unlockables table directly
	// DEFINITELY not M_CheckNetUnlockByID
	return (boolean)(gamedata->unlocked[i]);
}

/*--------------------------------------------------
	boolean K_SetFollowerByName(INT32 playernum, const char *skinname)

		See header file for description.
--------------------------------------------------*/
boolean K_SetFollowerByName(INT32 playernum, const char *skinname)
{
	INT32 i;
	player_t *player = &players[playernum];

	if (stricmp("None", skinname) == 0)
	{
		K_SetFollowerByNum(playernum, -1); // reminder that -1 is nothing
		return true;
	}

	for (i = 0; i < numfollowers; i++)
	{
		// search in the skin list
		if (stricmp(followers[i].name, skinname) == 0)
		{
			K_SetFollowerByNum(playernum, i);
			return true;
		}
	}

	if (P_IsLocalPlayer(player))
	{
		CONS_Alert(CONS_WARNING, M_GetText("Follower '%s' not found.\n"), skinname);
	}
	else if (server || IsPlayerAdmin(consoleplayer))
	{
		CONS_Alert(CONS_WARNING, M_GetText("Player %d (%s) follower '%s' not found\n"), playernum, player_names[playernum], skinname);
	}

	K_SetFollowerByNum(playernum, -1); // reminder that -1 is nothing
	return false;
}

/*--------------------------------------------------
	void K_RemoveFollower(player_t *player)

		See header file for description.
--------------------------------------------------*/
void K_RemoveFollower(player_t *player)
{
	mobj_t *bub, *tmp;
	if (player->follower && !P_MobjWasRemoved(player->follower)) // this is also called when we change colour so don't respawn the follower unless we changed skins
	{
		// Remove follower's possible hnext list (bubble)
		bub = player->follower->hnext;

		while (bub && !P_MobjWasRemoved(bub))
		{
			tmp = bub->hnext;
			P_RemoveMobj(bub);
			bub = tmp;
		}

		P_RemoveMobj(player->follower);
		P_SetTarget(&player->follower, NULL);
	}
}

/*--------------------------------------------------
	void K_SetFollowerByNum(INT32 playernum, INT32 skinnum)

		See header file for description.
--------------------------------------------------*/
void K_SetFollowerByNum(INT32 playernum, INT32 skinnum)
{
	player_t *player = &players[playernum];

	player->followerready = true; // we are ready to perform follower related actions in the player thinker, now.

	if (skinnum >= -1 && skinnum <= numfollowers) // Make sure it exists!
	{
		/*
			We don't spawn the follower here since it'll be easier to handle all of it in the Player thinker itself.
			However, we will despawn it right here if there's any to make it easy for the player thinker to replace it or delete it.
		*/

		if (skinnum != player->followerskin)
			K_RemoveFollower(player);

		player->followerskin = skinnum;

		// for replays: We have changed our follower mid-game; let the game know so it can do the same in the replay!
		demo_extradata[playernum] |= DXD_FOLLOWER;
		return;
	}

	if (P_IsLocalPlayer(player))
	{
		CONS_Alert(CONS_WARNING, M_GetText("Follower %d not found\n"), skinnum);
	}
	else if (server || IsPlayerAdmin(consoleplayer))
	{
		CONS_Alert(CONS_WARNING, "Player %d (%s) follower %d not found\n", playernum, player_names[playernum], skinnum);
	}

	K_SetFollowerByNum(playernum, -1); // Not found, then set -1 (nothing) as our follower.
}

/*--------------------------------------------------
	static void K_SetFollowerState(mobj_t *f, statenum_t state)

		Sets a follower object's state.
		This is done as a separate function to prevent running follower actions.

	Input Arguments:-
		f - The follower's mobj_t.
		state - The state to set.

	Return:-
		None
--------------------------------------------------*/
static void K_SetFollowerState(mobj_t *f, statenum_t state)
{
	if (f == NULL || P_MobjWasRemoved(f) == true)
	{
		// safety net
		return;
	}

	// No, do NOT set the follower to S_NULL. Set it to S_INVISIBLE.
	if (state == S_NULL)
	{
		state = S_INVISIBLE;
		f->threshold = 1; // Threshold = 1 means stop doing anything related to setting states, so that we don't get out of S_INVISIBLE
	}

	// extravalue2 stores the last "first state" we used.
	// because states default to idlestates, if we use an animation that uses an "ongoing" state line, don't reset it!
	// this prevents it from looking very dumb
	if (state == (statenum_t)f->extravalue2)
	{
		return;
	}

	// we will save the state into extravalue2.
	f->extravalue2 = state;

	P_SetMobjStateNF(f, state);
	if (f->state->tics > 0)
	{
		f->tics++;
	}
}

/*--------------------------------------------------
	UINT16 K_GetEffectiveFollowerColor(UINT16 followercolor, UINT16 playercolor)

		See header file for description.
--------------------------------------------------*/
UINT16 K_GetEffectiveFollowerColor(UINT16 followercolor, UINT16 playercolor)
{
	if (followercolor < numskincolors) // bog standard
		return followercolor;

	if (followercolor == FOLLOWERCOLOR_OPPOSITE) // "Opposite"
		return skincolors[playercolor].invcolor;

	//if (followercolor == FOLLOWERCOLOR_MATCH) -- "Match"
	return playercolor;
}

/*--------------------------------------------------
	static void K_UpdateFollowerState(mobj_t *f, statenum_t state, followerstate_t type)

		Sets a follower object's state & current state type tracker.
		If the state tracker already matches, then this is ignored.

	Input Arguments:-
		f - The follower's mobj_t.
		state - The state to set.
		type - State type tracker.

	Return:-
		None
--------------------------------------------------*/
static void K_UpdateFollowerState(mobj_t *f, statenum_t state, followerstate_t type)
{
	if (f == NULL || P_MobjWasRemoved(f) == true)
	{
		// safety net
		return;
	}

	if (f->extravalue1 != (INT32)type)
	{
		K_SetFollowerState(f, state);
		f->extravalue1 = type;
	}
}

/*--------------------------------------------------
	void K_HandleFollower(player_t *player)

		See header file for description.
--------------------------------------------------*/
void K_HandleFollower(player_t *player)
{
	follower_t fl;
	angle_t an;
	fixed_t zoffs;
	fixed_t ourheight;
	fixed_t sx, sy, sz, deltaz;
	fixed_t fh = INT32_MIN, ch = INT32_MAX;
	UINT16 color;

	fixed_t bubble; // bubble scale (0 if no bubble)
	mobj_t *bmobj; // temp bubble mobj

	angle_t destAngle;
	INT32 angleDiff;

	if (player->followerready == false)
	{
		// we aren't ready to perform anything follower related yet.
		return;
	}

	// How about making sure our follower exists and is added before trying to spawn it n' all?
	if (player->followerskin >= numfollowers || player->followerskin < -1)
	{
		//CONS_Printf("Follower skin invlaid. Setting to -1.\n");
		player->followerskin = -1;
	}

	// don't do anything if we can't have a follower to begin with.
	// (It gets removed under those conditions)
	if (player->spectator || player->followerskin < 0
	|| player->mo == NULL || P_MobjWasRemoved(player->mo))
	{
		if (player->follower)
		{
			K_RemoveFollower(player);
		}
		return;
	}

	// Before we do anything, let's be sure of where we're supposed to be
	fl = followers[player->followerskin];

	an = player->mo->angle + fl.atangle;
	zoffs = fl.zoffs;
	bubble = fl.bubblescale; // 0 if no bubble to spawn.

	// do you like angle maths? I certainly don't...
	sx = player->mo->x + player->mo->momx + FixedMul(FixedMul(player->mo->scale, fl.dist), FINECOSINE((an) >> ANGLETOFINESHIFT));
	sy = player->mo->y + player->mo->momy + FixedMul(FixedMul(player->mo->scale, fl.dist), FINESINE((an) >> ANGLETOFINESHIFT));

	// interp info helps with stretchy fix
	deltaz = (player->mo->z - player->mo->old_z);

	// for the z coordinate, don't be a doof like Steel and forget that MFE_VERTICALFLIP exists :P
	sz = player->mo->z + player->mo->momz + FixedMul(player->mo->scale, zoffs * P_MobjFlip(player->mo));
	ourheight = FixedMul(fl.height, player->mo->scale);
	if (player->mo->eflags & MFE_VERTICALFLIP)
	{
		sz += ourheight;
	}

	fh = player->mo->floorz;
	ch = player->mo->ceilingz - ourheight;

	switch (fl.mode)
	{
		case FOLLOWERMODE_GROUND:
		{
			if (player->mo->eflags & MFE_VERTICALFLIP)
			{
				sz = ch;
			}
			else
			{
				sz = fh;
			}
			break;
		}
		case FOLLOWERMODE_FLOAT:
		default:
		{
			// finally, add a cool floating effect to the z height.
			// not stolen from k_kart I swear!!
			fixed_t sine = FixedMul(fl.bobamp, FINESINE(((FixedMul(4 * M_TAU_FIXED, fl.bobspeed) * leveltime) >> ANGLETOFINESHIFT) & FINEMASK));
			sz += FixedMul(player->mo->scale, sine) * P_MobjFlip(player->mo);
			break;
		}
	}

	// Set follower colour
	color = K_GetEffectiveFollowerColor(player->followercolor, player->skincolor);

	if (player->follower == NULL || P_MobjWasRemoved(player->follower)) // follower doesn't exist / isn't valid
	{
		//CONS_Printf("Spawning follower...\n");

		// so let's spawn one!
		P_SetTarget(&player->follower, P_SpawnMobj(sx, sy, sz, MT_FOLLOWER));
		if (player->follower == NULL)
			return;

		K_UpdateFollowerState(player->follower, fl.idlestate, FOLLOWERSTATE_IDLE);

		P_SetTarget(&player->follower->target, player->mo); // we need that to know when we need to disappear
		player->follower->angle = player->follower->old_angle = player->mo->angle;

		// This is safe to only spawn it here, the follower is removed then respawned when switched.
		if (bubble)
		{
			bmobj = P_SpawnMobj(player->follower->x, player->follower->y, player->follower->z, MT_FOLLOWERBUBBLE_FRONT);
			P_SetTarget(&player->follower->hnext, bmobj);
			P_SetTarget(&bmobj->target, player->follower); // Used to know if we have to despawn at some point.

			bmobj = P_SpawnMobj(player->follower->x, player->follower->y, player->follower->z, MT_FOLLOWERBUBBLE_BACK);
			P_SetTarget(&player->follower->hnext->hnext, bmobj); // this seems absolutely stupid, I know, but this will make updating the momentums/flags of these a bit easier.
			P_SetTarget(&bmobj->target, player->follower); // Ditto
		}
	}
	else // follower exists, woo!
	{
		if (player->follower->hitlag != 0)
		{
			// Don't update frames in hitlag
			return;
		}

		// first of all, handle states following the same model as above:
		if (player->follower->tics == 1)
		{
			K_SetFollowerState(player->follower, player->follower->state->nextstate);
		}

		// move the follower next to us (yes, this is really basic maths but it looks pretty damn clean in practice)!
		player->follower->momx = FixedDiv(sx - player->follower->x, fl.horzlag);
		player->follower->momy = FixedDiv(sy - player->follower->y, fl.horzlag);

		player->follower->z += FixedDiv(deltaz, fl.vertlag);

		if (fl.mode == FOLLOWERMODE_GROUND)
		{
			sector_t *sec = R_PointInSubsector(sx, sy)->sector;

			fh = min(fh, P_GetFloorZ(player->follower, sec, sx, sy, NULL));
			ch = max(ch, P_GetCeilingZ(player->follower, sec, sx, sy, NULL) - ourheight);

			if (P_IsObjectOnGround(player->mo) == false)
			{
				// In the air, match their momentum.
				player->follower->momz = player->mo->momz;
			}
			else
			{
				fixed_t fg = P_GetMobjGravity(player->mo);
				fixed_t fz = P_GetMobjZMovement(player->follower);

				player->follower->momz = fz;

				// Player is on the ground ... try to get the follower
				// back to the ground also if it is above it.
				player->follower->momz += FixedDiv(fg * 6, fl.vertlag); // Scaled against the default value of vertlag
			}
		}
		else
		{
			player->follower->momz = FixedDiv(sz - player->follower->z, fl.vertlag);
		}

		if (player->mo->colorized)
		{
			player->follower->color = player->mo->color;
		}
		else
		{
			player->follower->color = color;
		}

		player->follower->colorized = player->mo->colorized;

		P_SetScale(player->follower, FixedMul(fl.scale, player->mo->scale));
		K_GenericExtraFlagsNoZAdjust(player->follower, player->mo); // Not K_MatchGenericExtraFlag because the Z adjust it has only works properly if master & mo have the same Z height.

		// Match how the player is being drawn
		player->follower->renderflags = player->mo->renderflags;

		// Make the follower invisible if we no contest'd rather than removing it. No one will notice the diff seriously.
		if (player->pflags & PF_NOCONTEST)
		{
			player->follower->renderflags |= RF_DONTDRAW;
		}

		// if we're moving let's make the angle the direction we're moving towards. This is to avoid drifting / reverse looking awkward.
		if (FixedHypot(player->follower->momx, player->follower->momy) >= player->mo->scale)
		{
			destAngle = R_PointToAngle2(0, 0, player->follower->momx, player->follower->momy);
		}
		else
		{
			// Face the player's angle when standing still.
			destAngle = player->mo->angle;
		}

		// Sal: Turn the follower around when looking backwards.
		if ( player->cmd.buttons & BT_LOOKBACK )
		{
			destAngle += ANGLE_180;
		}

		// Sal: Smoothly rotate angle to the destination value.
		angleDiff = AngleDeltaSigned(destAngle, player->follower->angle);

		if (angleDiff != 0)
		{
			player->follower->angle += FixedDiv(angleDiff, fl.anglelag);
		}

		// Ground follower slope rotation
		if (fl.mode == FOLLOWERMODE_GROUND)
		{
			if (player->follower->z <= fh)
			{
				player->follower->z = fh;

				if (!(player->mo->eflags & MFE_VERTICALFLIP) && player->follower->momz <= 0 && fl.bobamp != 0)
				{
					// Ground bounce
					player->follower->momz = P_GetMobjZMovement(player->mo) + FixedMul(fl.bobamp, player->follower->scale);
					player->follower->extravalue1 = FOLLOWERSTATE_RESET;
				}
				else if (player->follower->momz < 0)
				{
					// Ceiling clip
					player->follower->momz = 0;
				}
			}
			else if (player->follower->z >= ch)
			{
				player->follower->z = ch;

				if ((player->mo->eflags & MFE_VERTICALFLIP) && player->follower->momz >= 0 && fl.bobamp != 0)
				{
					// Ground bounce
					player->follower->momz = P_GetMobjZMovement(player->mo) - FixedMul(fl.bobamp, player->follower->scale);
					player->follower->extravalue1 = FOLLOWERSTATE_RESET;
				}
				else if (player->follower->momz > 0)
				{
					// Ceiling clip
					player->follower->momz = 0;
				}
			}

			K_CalculateBananaSlope(
				player->follower,
				player->follower->x, player->follower->y, player->follower->z,
				player->follower->radius, ourheight,
				(player->mo->eflags & MFE_VERTICALFLIP),
				false
			);
		}

		// Finally, if the follower has bubbles, move them, set their scale, etc....
		// This is what I meant earlier by it being easier, now we can just use this weird lil loop to get the job done!

		bmobj = player->follower->hnext; // will be NULL if there's no bubble

		while (bmobj != NULL && P_MobjWasRemoved(bmobj) == false)
		{
			// match follower's momentums and (e)flags(2).
			bmobj->momx = player->follower->momx;
			bmobj->momy = player->follower->momy;
			bmobj->z += FixedDiv(deltaz, fl.vertlag);
			bmobj->momz = player->follower->momz;

			P_SetScale(bmobj, FixedMul(bubble, player->mo->scale));
			K_GenericExtraFlagsNoZAdjust(bmobj, player->follower);
			bmobj->renderflags = player->mo->renderflags;

			if (player->follower->threshold)
			{
				// threshold means the follower was "despawned" with S_NULL (is actually just set to S_INVISIBLE)
				P_SetMobjState(bmobj, S_INVISIBLE); // sooooo... let's do the same!
			}

			// switch to other bubble layer or exit
			bmobj = bmobj->hnext;
		}

		if (player->follower->threshold)
		{
			// Threshold means the follower was "despanwed" with S_NULL.
			return;
		}

		// However with how the code is factored, this is just a special case of S_INVISBLE to avoid having to add other player variables.

		// handle follower animations. Could probably be better...
		// hurt or dead
		if (P_PlayerInPain(player) == true || player->mo->state == &states[S_KART_SPINOUT] || player->mo->health <= 0)
		{
			// cancel hit confirm.
			player->follower->movecount = 0;

			// spin out
			player->follower->angle = player->drawangle;

			K_UpdateFollowerState(player->follower, fl.hurtstate, FOLLOWERSTATE_HURT);

			if (player->mo->health <= 0)
			{
				// if dead, follow the player's z momentum exactly so they both look like they die at the same speed.
				player->follower->momz = player->mo->momz;
			}
		}
		else if (player->exiting)
		{
			// win/ loss animations
			if (K_IsPlayerLosing(player))
			{
				// L
				K_UpdateFollowerState(player->follower, fl.losestate, FOLLOWERSTATE_LOSE);
			}
			else
			{
				// W
				K_UpdateFollowerState(player->follower, fl.winstate, FOLLOWERSTATE_WIN);
			}
		}
		else if (player->follower->movecount)
		{
			K_UpdateFollowerState(player->follower, fl.hitconfirmstate, FOLLOWERSTATE_HITCONFIRM);
			player->follower->movecount--;
		}
		else if (player->speed > 10*player->mo->scale) // animation for moving fast enough
		{
			K_UpdateFollowerState(player->follower, fl.followstate, FOLLOWERSTATE_FOLLOW);
		}
		else
		{
			K_UpdateFollowerState(player->follower, fl.idlestate, FOLLOWERSTATE_IDLE);
		}
	}

	if (player->mo->hitlag)
	{
		player->follower->hitlag = player->mo->hitlag;
		player->follower->eflags |= (player->mo->eflags & MFE_DAMAGEHITLAG);
		return;
	}
}
