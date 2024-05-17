/// \file  k_collide.c
/// \brief SRB2Kart item collision hooks

#include "k_collide.h"
#include "doomtype.h"
#include "p_mobj.h"
#include "k_kart.h"
#include "p_local.h"
#include "s_sound.h"
#include "r_main.h" // R_PointToAngle2, R_PointToDist2
#include "hu_stuff.h" // Sink snipe print
#include "doomdef.h" // Sink snipe print
#include "g_game.h" // Sink snipe print

boolean K_OrbinautJawzCollide(mobj_t *t1, mobj_t *t2)
{
	boolean damageitem = false;
	boolean sprung = false;

	if (((t1->target == t2) || (t1->target == t2->target)) && (t1->threshold > 0 || (t2->type != MT_PLAYER && t2->threshold > 0)))
		return true;

	if (t1->health <= 0 || t2->health <= 0)
		return true;

	if ((t1->type == MT_ORBINAUT_SHIELD || t1->type == MT_JAWZ_SHIELD) && t1->lastlook
		&& (t2->type == MT_ORBINAUT_SHIELD || t2->type == MT_JAWZ_SHIELD) && t2->lastlook
		&& (t1->target == t2->target)) // Don't hit each other if you have the same target
		return true;

	if (t2->player)
	{
		if ((t2->player->powers[pw_flashing] > 0)
			&& !(t1->type == MT_ORBINAUT || t1->type == MT_JAWZ || t1->type == MT_JAWZ_DUD))
			return true;

		if (t2->player->kartstuff[k_hyudorotimer])
			return true; // no interaction

		if (t2->player->kartstuff[k_flamedash] && t2->player->kartstuff[k_itemtype] == KITEM_FLAMESHIELD)
		{
			// Melt item
			S_StartSound(t2, sfx_s3k43);
		}
		else
		{
			// Player Damage
			P_DamageMobj(t2, t1, t1->target, 1, DMG_WIPEOUT);
			K_KartBouncing(t2, t1, false, false);
			S_StartSound(t2, sfx_s3k7b);
		}

		damageitem = true;
	}
	else if (t2->type == MT_ORBINAUT || t2->type == MT_JAWZ || t2->type == MT_JAWZ_DUD
		|| t2->type == MT_ORBINAUT_SHIELD || t2->type == MT_JAWZ_SHIELD
		|| t2->type == MT_BANANA || t2->type == MT_BANANA_SHIELD
		|| t2->type == MT_BALLHOG)
	{
		// Other Item Damage
		S_StartSound(t2, t2->info->deathsound);
		P_KillMobj(t2, t1, t1, DMG_NORMAL);

		P_SetObjectMomZ(t2, 8*FRACUNIT, false);
		P_InstaThrust(t2, R_PointToAngle2(t1->x, t1->y, t2->x, t2->y)+ANGLE_90, 16*FRACUNIT);

		P_SpawnMobj(t2->x/2 + t1->x/2, t2->y/2 + t1->y/2, t2->z/2 + t1->z/2, MT_ITEMCLASH);

		damageitem = true;
	}
	else if (t2->type == MT_SSMINE_SHIELD || t2->type == MT_SSMINE || t2->type == MT_LANDMINE)
	{
		damageitem = true;
		// Bomb death
		P_KillMobj(t2, t1, t1, DMG_NORMAL);
	}
	else if (t2->flags & MF_SPRING && (t1->type != MT_ORBINAUT_SHIELD && t1->type != MT_JAWZ_SHIELD))
	{
		// Let thrown items hit springs!
		sprung = P_DoSpring(t2, t1);
	}
	else if (t2->flags & MF_SHOOTABLE)
	{
		// Shootable damage
		P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);
		damageitem = true;
	}

	if (damageitem)
	{
		// This Item Damage
		S_StartSound(t1, t1->info->deathsound);
		P_KillMobj(t1, t2, t2, DMG_NORMAL);

		P_SetObjectMomZ(t1, 8*FRACUNIT, false);
		P_InstaThrust(t1, R_PointToAngle2(t2->x, t2->y, t1->x, t1->y)+ANGLE_90, 16*FRACUNIT);
	}

	if (sprung)
	{
		return false;
	}

	return true;
}

boolean K_BananaBallhogCollide(mobj_t *t1, mobj_t *t2)
{
	boolean damageitem = false;

	if (((t1->target == t2) || (t1->target == t2->target)) && (t1->threshold > 0 || (t2->type != MT_PLAYER && t2->threshold > 0)))
		return true;

	if (t1->health <= 0 || t2->health <= 0)
		return true;

	if (((t1->type == MT_BANANA_SHIELD) && (t2->type == MT_BANANA_SHIELD))
		&& (t1->target == t2->target)) // Don't hit each other if you have the same target
		return true;

	if (t1->type == MT_BALLHOG && t2->type == MT_BALLHOG)
		return true; // Ballhogs don't collide with eachother

	if (t2->player)
	{
		if (t2->player->powers[pw_flashing] > 0)
			return true;

		// Banana snipe!
		if (t1->type == MT_BANANA && t1->health > 1)
			S_StartSound(t2, sfx_bsnipe);

		if (t2->player->kartstuff[k_flamedash] && t2->player->kartstuff[k_itemtype] == KITEM_FLAMESHIELD)
		{
			// Melt item
			S_StartSound(t2, sfx_s3k43);
		}
		else
		{
			P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);
		}

		damageitem = true;
	}
	else if (t2->type == MT_BANANA || t2->type == MT_BANANA_SHIELD
		|| t2->type == MT_ORBINAUT || t2->type == MT_ORBINAUT_SHIELD
		|| t2->type == MT_JAWZ || t2->type == MT_JAWZ_DUD || t2->type == MT_JAWZ_SHIELD
		|| t2->type == MT_BALLHOG)
	{
		// Other Item Damage
		S_StartSound(t2, t2->info->deathsound);
		P_KillMobj(t2, t1, t1, DMG_NORMAL);

		P_SetObjectMomZ(t2, 8*FRACUNIT, false);
		P_InstaThrust(t2, R_PointToAngle2(t1->x, t1->y, t2->x, t2->y)+ANGLE_90, 16*FRACUNIT);

		P_SpawnMobj(t2->x/2 + t1->x/2, t2->y/2 + t1->y/2, t2->z/2 + t1->z/2, MT_ITEMCLASH);

		damageitem = true;
	}
	else if (t2->type == MT_SSMINE_SHIELD || t2->type == MT_SSMINE || t2->type == MT_LANDMINE)
	{
		damageitem = true;
		// Bomb death
		P_KillMobj(t2, t1, t1, DMG_NORMAL);
	}
	else if (t2->flags & MF_SHOOTABLE)
	{
		// Shootable damage
		P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);
		damageitem = true;
	}

	if (damageitem)
	{
		// This Item Damage
		S_StartSound(t1, t1->info->deathsound);
		P_KillMobj(t1, t2, t2, DMG_NORMAL);

		P_SetObjectMomZ(t1, 8*FRACUNIT, false);
		P_InstaThrust(t1, R_PointToAngle2(t2->x, t2->y, t1->x, t1->y)+ANGLE_90, 16*FRACUNIT);
	}

	return true;
}

boolean K_EggItemCollide(mobj_t *t1, mobj_t *t2)
{
	// Push fakes out of other item boxes
	if (t2->type == MT_RANDOMITEM || t2->type == MT_EGGMANITEM)
	{
		P_InstaThrust(t1, R_PointToAngle2(t2->x, t2->y, t1->x, t1->y), t2->radius/4);
		return true;
	}

	if (t2->player)
	{
		if ((t1->target == t2 || t1->target == t2->target) && (t1->threshold > 0))
			return true;

		if (t1->health <= 0 || t2->health <= 0)
			return true;

		if (!P_CanPickupItem(t2->player, 2))
			return true;

		if ((gametyperules & GTR_BUMPERS) && t2->player->bumpers <= 0)
		{
#ifdef OTHERKARMAMODES
			if (t2->player->kartstuff[k_comebackmode] || t2->player->karmadelay)
				return true;
			t2->player->kartstuff[k_comebackmode] = 2;
#else
			return true;
#endif
		}
		else
		{
			K_DropItems(t2->player); //K_StripItems(t2->player);
			//K_StripOther(t2->player);
			t2->player->kartstuff[k_itemroulette] = 1;
			t2->player->kartstuff[k_roulettetype] = 2;
		}

		if (t2->player->kartstuff[k_flamedash] && t2->player->kartstuff[k_itemtype] == KITEM_FLAMESHIELD)
		{
			// Melt item
			S_StartSound(t2, sfx_s3k43);
			P_KillMobj(t1, t2, t2, DMG_NORMAL);
			return true;
		}
		else
		{
			mobj_t *poof = P_SpawnMobj(t1->x, t1->y, t1->z, MT_EXPLODE);
			S_StartSound(poof, t1->info->deathsound);

#if 0
			// Eggbox snipe!
			if (t1->type == MT_EGGMANITEM && t1->health > 1)
				S_StartSound(t2, sfx_bsnipe);
#endif

			if (t1->target && t1->target->player)
			{
				if ((gametyperules & GTR_CIRCUIT) || t1->target->player->bumpers > 0)
					t2->player->kartstuff[k_eggmanblame] = t1->target->player-players;
				else
					t2->player->kartstuff[k_eggmanblame] = t2->player-players;

				if (t1->target->hnext == t1)
				{
					P_SetTarget(&t1->target->hnext, NULL);
					t1->target->player->kartstuff[k_eggmanheld] = 0;
				}
			}

			P_RemoveMobj(t1);
			return true;
		}
	}

	return true;
}

boolean K_MineCollide(mobj_t *t1, mobj_t *t2)
{
	if (((t1->target == t2) || (t1->target == t2->target)) && (t1->threshold > 0 || (t2->type != MT_PLAYER && t2->threshold > 0)))
		return true;

	if (t1->health <= 0 || t2->health <= 0)
		return true;

	if (t2->player)
	{
		if (t2->player->powers[pw_flashing] > 0)
			return true;

		// Bomb punting
		if ((t1->state >= &states[S_SSMINE1] && t1->state <= &states[S_SSMINE4])
			|| (t1->state >= &states[S_SSMINE_DEPLOY8] && t1->state <= &states[S_SSMINE_DEPLOY13]))
		{
			P_KillMobj(t1, t2, t2, DMG_NORMAL);
		}
		else
		{
			K_PuntMine(t1, t2);
		}
	}
	else if (t2->type == MT_ORBINAUT || t2->type == MT_JAWZ || t2->type == MT_JAWZ_DUD
		|| t2->type == MT_ORBINAUT_SHIELD || t2->type == MT_JAWZ_SHIELD)
	{
		// Bomb death
		P_KillMobj(t1, t2, t2, DMG_NORMAL);

		// Other Item Damage
		S_StartSound(t2, t2->info->deathsound);
		P_KillMobj(t2, t1, t1, DMG_NORMAL);

		P_SetObjectMomZ(t2, 8*FRACUNIT, false);
		P_InstaThrust(t2, R_PointToAngle2(t1->x, t1->y, t2->x, t2->y)+ANGLE_90, 16*FRACUNIT);
	}
	else if (t2->flags & MF_SHOOTABLE)
	{
		// Bomb death
		P_KillMobj(t1, t2, t2, DMG_NORMAL);
		// Shootable damage
		P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);
	}

	return true;
}

boolean K_MineExplosionCollide(mobj_t *t1, mobj_t *t2)
{
	if (t2->player)
	{
		if (t2->player->powers[pw_flashing] > 0)
			return true;

		if (t1->state == &states[S_MINEEXPLOSION1])
		{
			P_DamageMobj(t2, t1, t1->target, 1, DMG_EXPLODE);
		}
		else
		{
			P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);
		}
	}
	else if (t2->flags & MF_SHOOTABLE)
	{
		// Shootable damage
		P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);
	}

	return true;
}

boolean K_LandMineCollide(mobj_t *t1, mobj_t *t2)
{
	if (((t1->target == t2) || (t1->target == t2->target)) && (t1->threshold > 0 || (t2->type != MT_PLAYER && t2->threshold > 0)))
		return true;

	if (t1->health <= 0 || t2->health <= 0)
		return true;

	if (t2->player)
	{
		if (t2->player->powers[pw_flashing])
			return true;

		// Banana snipe!
		if (t1->health > 1)
			S_StartSound(t2, sfx_bsnipe);

		if (t2->player->kartstuff[k_flamedash] && t2->player->kartstuff[k_itemtype] == KITEM_FLAMESHIELD)
		{
			// Melt item
			S_StartSound(t2, sfx_s3k43);
		}
		else
		{
			// Player Damage
			P_DamageMobj(t2, t1, t1->target, 1, DMG_WIPEOUT);
		}

		P_KillMobj(t1, t2, t2, DMG_NORMAL);
	}
	else if (t2->type == MT_BANANA || t2->type == MT_BANANA_SHIELD
		|| t2->type == MT_ORBINAUT || t2->type == MT_ORBINAUT_SHIELD
		|| t2->type == MT_JAWZ || t2->type == MT_JAWZ_DUD || t2->type == MT_JAWZ_SHIELD
		|| t2->type == MT_BALLHOG)
	{
		// Other Item Damage
		if (t2->eflags & MFE_VERTICALFLIP)
			t2->z -= t2->height;
		else
			t2->z += t2->height;

		S_StartSound(t2, t2->info->deathsound);
		P_KillMobj(t2, t1, t1, DMG_NORMAL);

		P_SetObjectMomZ(t2, 8*FRACUNIT, false);
		P_InstaThrust(t2, R_PointToAngle2(t1->x, t1->y, t2->x, t2->y)+ANGLE_90, 16*FRACUNIT);

		P_SpawnMobj(t2->x/2 + t1->x/2, t2->y/2 + t1->y/2, t2->z/2 + t1->z/2, MT_ITEMCLASH);

		P_KillMobj(t1, t2, t2, DMG_NORMAL);
	}
	else if (t2->type == MT_SSMINE_SHIELD || t2->type == MT_SSMINE || t2->type == MT_LANDMINE)
	{
		P_KillMobj(t1, t2, t2, DMG_NORMAL);
		// Bomb death
		P_KillMobj(t2, t1, t1, DMG_NORMAL);
	}
	else if (t2->flags & MF_SHOOTABLE)
	{
		// Shootable damage
		P_DamageMobj(t2, t1, t1->target, 1, DMG_NORMAL);
		P_KillMobj(t1, t2, t2, DMG_NORMAL);
	}

	return true;
}

boolean K_KitchenSinkCollide(mobj_t *t1, mobj_t *t2)
{
	if (((t1->target == t2) || (t1->target == t2->target)) && (t1->threshold > 0 || (t2->type != MT_PLAYER && t2->threshold > 0)))
		return true;

	if (t2->player)
	{
		if (t2->player->powers[pw_flashing] > 0)
			return true;

		S_StartSound(NULL, sfx_bsnipe); // let all players hear it.

		HU_SetCEchoFlags(0);
		HU_SetCEchoDuration(5);
		HU_DoCEcho(va("%s\\was hit by a kitchen sink.\\\\\\\\", player_names[t2->player-players]));
		I_OutputMsg("%s was hit by a kitchen sink.\n", player_names[t2->player-players]);

		P_DamageMobj(t2, t1, t1->target, 1, DMG_INSTAKILL);
		P_KillMobj(t1, t2, t2, DMG_NORMAL);
	}
	else if (t2->flags & MF_SHOOTABLE)
	{
		// Shootable damage
		P_KillMobj(t2, t2, t1->target, DMG_NORMAL);
		// This item damage
		P_KillMobj(t1, t2, t2, DMG_NORMAL);
	}

	return true;
}

boolean K_FallingRockCollide(mobj_t *t1, mobj_t *t2)
{
	if (t2->player || t2->type == MT_FALLINGROCK)
		K_KartBouncing(t2, t1, false, false);
	return true;
}

boolean K_SMKIceBlockCollide(mobj_t *t1, mobj_t *t2)
{
	if (!(t2->flags & MF_SOLID || t2->flags & MF_SHOOTABLE))
		return true;

	if (!(t2->health))
		return true;

	if (t2->type == MT_BANANA || t2->type == MT_BANANA_SHIELD
		|| t2->type == MT_EGGMANITEM || t2->type == MT_EGGMANITEM_SHIELD
		|| t2->type == MT_SSMINE || t2->type == MT_SSMINE_SHIELD
		|| t2->type == MT_ORBINAUT_SHIELD || t2->type == MT_JAWZ_SHIELD)
		return false;

	if (t1->health)
		P_KillMobj(t1, t2, t2, DMG_NORMAL);

	/*
	if (t2->player && (t2->player->kartstuff[k_invincibilitytimer] > 0
		|| t2->player->kartstuff[k_growshrinktimer] > 0))
		return true;
	*/

	K_KartBouncing(t2, t1, false, true);
	return false;
}

boolean K_PvPTouchDamage(mobj_t *t1, mobj_t *t2)
{
	boolean t1Condition = false;
	boolean t2Condition = false;
	boolean stung = false;

	// Grow damage
	t1Condition = (t1->scale > t2->scale + (mapobjectscale/8));
	t2Condition = (t2->scale > t1->scale + (mapobjectscale/8));

	if (t1Condition == true && t2Condition == false)
	{
		P_DamageMobj(t2, t1, t1, 1, DMG_SQUISH);
		return true;
	}
	else if (t1Condition == false && t2Condition == true)
	{
		P_DamageMobj(t1, t2, t2, 1, DMG_SQUISH);
		return true;
	}

	// Invincibility damage
	t1Condition = (t1->player->kartstuff[k_invincibilitytimer] > 0);
	t2Condition = (t2->player->kartstuff[k_invincibilitytimer] > 0);

	if (t1Condition == true && t2Condition == false)
	{
		P_DamageMobj(t2, t1, t1, 1, DMG_WIPEOUT);
		return true;
	}
	else if (t1Condition == false && t2Condition == true)
	{
		P_DamageMobj(t1, t2, t2, 1, DMG_WIPEOUT);
		return true;
	}

	// Flame Shield dash damage
	t1Condition = (t1->player->kartstuff[k_flamedash] > 0 && t1->player->kartstuff[k_itemtype] == KITEM_FLAMESHIELD);
	t2Condition = (t2->player->kartstuff[k_flamedash] > 0 && t2->player->kartstuff[k_itemtype] == KITEM_FLAMESHIELD);

	if (t1Condition == true && t2Condition == false)
	{
		P_DamageMobj(t2, t1, t1, 1, DMG_WIPEOUT);
		return true;
	}
	else if (t1Condition == false && t2Condition == true)
	{
		P_DamageMobj(t1, t2, t2, 1, DMG_WIPEOUT);
		return true;
	}

	// Battle Mode Sneaker damage
	// (Pogo Spring damage is handled in head-stomping code)
	if (gametyperules & GTR_BUMPERS)
	{
		t1Condition = (t1->player->kartstuff[k_sneakertimer] > 0 && t1->player->powers[pw_flashing] != 0);
		t2Condition = (t2->player->kartstuff[k_sneakertimer] > 0 && t2->player->powers[pw_flashing] != 0);

		if (t1Condition == true && t2Condition == false)
		{
			P_DamageMobj(t2, t1, t1, 1, DMG_WIPEOUT|DMG_STEAL);
			return true;
		}
		else if (t1Condition == false && t2Condition == true)
		{
			P_DamageMobj(t1, t2, t2, 1, DMG_WIPEOUT|DMG_STEAL);
			return true;
		}
	}

	/*
	// Ring sting, this is a bit more unique
	t1Condition = (K_GetShieldFromItem(t2->player->kartstuff[k_itemtype]) == KSHIELD_NONE);
	t2Condition = (K_GetShieldFromItem(t1->player->kartstuff[k_itemtype]) == KSHIELD_NONE);

	if (t1Condition == true)
	{
		if (t2->player->rings <= 0)
		{
			P_DamageMobj(t2, t1, t1, 1, DMG_STING);
			stung = true;
		}

		P_PlayerRingBurst(t2->player, 1);
		stung = true;
	}

	if (t2Condition == true)
	{
		if (t1->player->rings <= 0)
		{
			P_DamageMobj(t1, t2, t2, 1, DMG_STING);
			stung = true;
		}

		P_PlayerRingBurst(t2->player, 1);
	}

	return stung;*/
}
