// SONIC ROBO BLAST 2 KART ~ ZarroTsu
//-----------------------------------------------------------------------------
/// \file  k_kart.c
/// \brief SRB2kart general.
///        All of the SRB2kart-unique stuff.

#include "k_kart.h"
#include "command.h"
#include "d_player.h"
#include "info.h"
#include "k_battle.h"
#include "k_pwrlv.h"
#include "k_color.h"
#include "k_respawn.h"
#include "doomdef.h"
#include "hu_stuff.h"
#include "g_game.h"
#include "m_random.h"
#include "p_local.h"
#include "p_slopes.h"
#include "p_setup.h"
#include "r_draw.h"
#include "r_local.h"
#include "s_sound.h"
#include "st_stuff.h"
#include "v_video.h"
#include "z_zone.h"
#include "m_misc.h"
#include "m_cond.h"
#include "f_finale.h"
#include "lua_hud.h"	// For Lua hud checks
#include "lua_hook.h"	// For MobjDamage and ShouldDamage
#include "m_cheat.h"	// objectplacing
#include "p_spec.h"

#include "k_waypoint.h"
#include "k_bot.h"
#include "k_hud.h"


//hardcode saltyhop mhhm
consvar_t cv_saltyhop = {"hardcodehop", "Off", CV_SAVE, CV_OnOff, NULL};
consvar_t cv_saltyhopsfx = {"hardcodehopsfx", "On", CV_SAVE, CV_OnOff};
//consvar_t cv_saltysquish = {"hardcodehopsquish", "On", CV_SAVE, CV_OnOff};

// SOME IMPORTANT VARIABLES DEFINED IN DOOMDEF.H:
// gamespeed is cc (0 for easy, 1 for normal, 2 for hard)
// franticitems is Frantic Mode items, bool
// encoremode is Encore Mode (duh), bool
// comeback is Battle Mode's karma comeback, also bool
// battlewanted is an array of the WANTED player nums, -1 for no player in that slot
// indirectitemcooldown is timer before anyone's allowed another Shrink/SPB
// mapreset is set when enough players fill an empty server

UINT16 K_GetPlayerDontDrawFlag(player_t *player)
{
	UINT16 flag = 0;

	if (player == &players[displayplayers[0]])
		flag = MFD_DONTDRAWP1;
	else if (r_splitscreen >= 1 && player == &players[displayplayers[1]])
		flag = MFD_DONTDRAWP2;
	else if (r_splitscreen >= 2 && player == &players[displayplayers[2]])
		flag = MFD_DONTDRAWP3;
	else if (r_splitscreen >= 3 && player == &players[displayplayers[3]])
		flag = MFD_DONTDRAWP4;

	return flag;
}

player_t *K_GetItemBoxPlayer(mobj_t *mobj)
{
	fixed_t closest = INT32_MAX;
	player_t *player = NULL;
	UINT8 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!(playeringame[i] && players[i].mo && !P_MobjWasRemoved(players[i].mo) && !players[i].spectator))
		{
			continue;
		}

		// Always use normal item box rules -- could pass in "2" for fakes but they blend in better like this
		if (P_CanPickupItem(&players[i], 1))
		{
			fixed_t dist = P_AproxDistance(P_AproxDistance(
				players[i].mo->x - mobj->x,
				players[i].mo->y - mobj->y),
				players[i].mo->z - mobj->z
			);

			if (dist > 8192*mobj->scale)
			{
				continue;
			}

			if (dist < closest)
			{
				player = &players[i];
				closest = dist;
			}
		}
	}

	return player;
}

// Angle reflection used by springs & speed pads
angle_t K_ReflectAngle(angle_t yourangle, angle_t theirangle, fixed_t yourspeed, fixed_t theirspeed)
{
	INT32 angoffset;
	boolean subtract = false;

	angoffset = yourangle - theirangle;

	if ((angle_t)angoffset > ANGLE_180)
	{
		// Flip on wrong side
		angoffset = InvAngle((angle_t)angoffset);
		subtract = !subtract;
	}

	// Fix going directly against the spring's angle sending you the wrong way
	if ((angle_t)angoffset > ANGLE_90)
	{
		angoffset = ANGLE_180 - angoffset;
	}

	// Offset is reduced to cap it (90 / 2 = max of 45 degrees)
	angoffset /= 2;

	// Reduce further based on how slow your speed is compared to the spring's speed
	// (set both to 0 to ignore this)
	if (theirspeed != 0 && yourspeed != 0)
	{
		if (theirspeed > yourspeed)
		{
			angoffset = FixedDiv(angoffset, FixedDiv(theirspeed, yourspeed));
		}
	}

	if (subtract)
		angoffset = (signed)(theirangle) - angoffset;
	else
		angoffset = (signed)(theirangle) + angoffset;

	return (angle_t)angoffset;
}

//{ SRB2kart Net Variables

void K_RegisterKartStuff(void)
{
	CV_RegisterVar(&cv_sneaker);
	CV_RegisterVar(&cv_rocketsneaker);
	CV_RegisterVar(&cv_invincibility);
	CV_RegisterVar(&cv_banana);
	CV_RegisterVar(&cv_eggmanmonitor);
	CV_RegisterVar(&cv_orbinaut);
	CV_RegisterVar(&cv_jawz);
	CV_RegisterVar(&cv_mine);
	CV_RegisterVar(&cv_landmine);
	CV_RegisterVar(&cv_ballhog);
	CV_RegisterVar(&cv_selfpropelledbomb);
	CV_RegisterVar(&cv_grow);
	CV_RegisterVar(&cv_shrink);
	CV_RegisterVar(&cv_thundershield);
	CV_RegisterVar(&cv_bubbleshield);
	CV_RegisterVar(&cv_flameshield);
	CV_RegisterVar(&cv_hyudoro);
	CV_RegisterVar(&cv_pogospring);
	CV_RegisterVar(&cv_superring);
	CV_RegisterVar(&cv_kitchensink);

	CV_RegisterVar(&cv_dualsneaker);
	CV_RegisterVar(&cv_triplesneaker);
	CV_RegisterVar(&cv_triplebanana);
	CV_RegisterVar(&cv_decabanana);
	CV_RegisterVar(&cv_tripleorbinaut);
	CV_RegisterVar(&cv_quadorbinaut);
	CV_RegisterVar(&cv_dualjawz);

	CV_RegisterVar(&cv_kartminimap);
	CV_RegisterVar(&cv_kartcheck);
	CV_RegisterVar(&cv_kartinvinsfx);
	CV_RegisterVar(&cv_kartspeed);
	CV_RegisterVar(&cv_kartbumpers);
	CV_RegisterVar(&cv_kartfrantic);
	CV_RegisterVar(&cv_kartcomeback);
	CV_RegisterVar(&cv_kartencore);
	CV_RegisterVar(&cv_kartvoterulechanges);
	CV_RegisterVar(&cv_kartspeedometer);
	CV_RegisterVar(&cv_kartvoices);
	CV_RegisterVar(&cv_kartbot);
	CV_RegisterVar(&cv_karteliminatelast);
	CV_RegisterVar(&cv_kartusepwrlv);
	CV_RegisterVar(&cv_votetime);

	CV_RegisterVar(&cv_kartdebugitem);
	CV_RegisterVar(&cv_kartdebugamount);
	CV_RegisterVar(&cv_kartdebugshrink);
	CV_RegisterVar(&cv_kartallowgiveitem);
	CV_RegisterVar(&cv_kartdebugdistribution);
	CV_RegisterVar(&cv_kartdebughuddrop);
	CV_RegisterVar(&cv_kartdebugwaypoints);

	CV_RegisterVar(&cv_kartdebugcheckpoint);
	CV_RegisterVar(&cv_kartdebugnodes);
	CV_RegisterVar(&cv_kartdebugcolorize);
	
	CV_RegisterVar(&cv_saltyhop);
	CV_RegisterVar(&cv_saltyhopsfx);
}

//}

boolean K_IsPlayerLosing(player_t *player)
{
	INT32 winningpos = 1;
	UINT8 i, pcount = 0;

	if (battlecapsules && player->bumpers <= 0)
		return true; // DNF in break the capsules

	if (player->kartstuff[k_position] == 1)
		return false;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;
		if (players[i].kartstuff[k_position] > pcount)
			pcount = players[i].kartstuff[k_position];
	}

	if (pcount <= 1)
		return false;

	winningpos = pcount/2;
	if (pcount % 2) // any remainder?
		winningpos++;

	return (player->kartstuff[k_position] > winningpos);
}

fixed_t K_GetKartGameSpeedScalar(SINT8 value)
{
	// Easy = 81.25%
	// Normal = 100%
	// Hard = 118.75%
	// Nightmare = 137.5% ?!?!
	return ((13 + (3*value)) << FRACBITS) / 16;
}

//{ SRB2kart Roulette Code - Position Based

consvar_t *KartItemCVars[NUMKARTRESULTS-1] =
{
	&cv_sneaker,
	&cv_rocketsneaker,
	&cv_invincibility,
	&cv_banana,
	&cv_eggmanmonitor,
	&cv_orbinaut,
	&cv_jawz,
	&cv_mine,
	&cv_landmine,
	&cv_ballhog,
	&cv_selfpropelledbomb,
	&cv_grow,
	&cv_shrink,
	&cv_thundershield,
	&cv_bubbleshield,
	&cv_flameshield,
	&cv_hyudoro,
	&cv_pogospring,
	&cv_superring,
	&cv_kitchensink,
	&cv_dualsneaker,
	&cv_triplesneaker,
	&cv_triplebanana,
	&cv_decabanana,
	&cv_tripleorbinaut,
	&cv_quadorbinaut,
	&cv_dualjawz
};

#define NUMKARTODDS 	80

// Less ugly 2D arrays
static INT32 K_KartItemOddsRace[NUMKARTRESULTS-1][8] =
{
				//P-Odds	 0  1  2  3  4  5  6  7
			   /*Sneaker*/ { 0, 0, 2, 4, 6, 0, 0, 0 }, // Sneaker
		/*Rocket Sneaker*/ { 0, 0, 0, 0, 0, 2, 4, 6 }, // Rocket Sneaker
		 /*Invincibility*/ { 0, 0, 0, 0, 2, 4, 6, 9 }, // Invincibility
				/*Banana*/ { 5, 3, 1, 0, 0, 0, 0, 0 }, // Banana
		/*Eggman Monitor*/ { 1, 2, 0, 0, 0, 0, 0, 0 }, // Eggman Monitor
			  /*Orbinaut*/ { 7, 4, 2, 2, 0, 0, 0, 0 }, // Orbinaut
				  /*Jawz*/ { 0, 3, 2, 1, 1, 0, 0, 0 }, // Jawz
				  /*Mine*/ { 0, 2, 3, 1, 0, 0, 0, 0 }, // Mine
			 /*Land Mine*/ { 4, 0, 0, 0, 0, 0, 0, 0 }, // Land Mine
			   /*Ballhog*/ { 0, 0, 2, 1, 0, 0, 0, 0 }, // Ballhog
   /*Self-Propelled Bomb*/ { 0, 1, 2, 3, 4, 2, 2, 0 }, // Self-Propelled Bomb
				  /*Grow*/ { 0, 0, 0, 1, 2, 3, 0, 0 }, // Grow
				/*Shrink*/ { 0, 0, 0, 0, 0, 0, 2, 0 }, // Shrink
		/*Thunder Shield*/ { 1, 2, 0, 0, 0, 0, 0, 0 }, // Thunder Shield
		 /*Bubble Shield*/ { 0, 1, 2, 1, 0, 0, 0, 0 }, // Bubble Shield
		  /*Flame Shield*/ { 0, 0, 0, 0, 0, 1, 3, 5 }, // Flame Shield
			   /*Hyudoro*/ { 0, 0, 0, 1, 1, 0, 0, 0 }, // Hyudoro
		   /*Pogo Spring*/ { 0, 0, 0, 0, 0, 0, 0, 0 }, // Pogo Spring
			/*Super Ring*/ { 2, 1, 1, 0, 0, 0, 0, 0 }, // Super Ring
		  /*Kitchen Sink*/ { 0, 0, 0, 0, 0, 0, 0, 0 }, // Kitchen Sink
			/*Sneaker x2*/ { 0, 0, 2, 2, 1, 0, 0, 0 }, // Sneaker x2
			/*Sneaker x3*/ { 0, 0, 0, 2, 6,10, 5, 0 }, // Sneaker x3
			 /*Banana x3*/ { 0, 1, 1, 0, 0, 0, 0, 0 }, // Banana x3
			/*Banana x10*/ { 0, 0, 0, 1, 0, 0, 0, 0 }, // Banana x10
		   /*Orbinaut x3*/ { 0, 0, 1, 0, 0, 0, 0, 0 }, // Orbinaut x3
		   /*Orbinaut x4*/ { 0, 0, 0, 1, 1, 0, 0, 0 }, // Orbinaut x4
			   /*Jawz x2*/ { 0, 0, 1, 2, 0, 0, 0, 0 }  // Jawz x2
};

static INT32 K_KartItemOddsBattle[NUMKARTRESULTS][2] =
{
				//P-Odds	 0  1
			   /*Sneaker*/ { 2, 1 }, // Sneaker
		/*Rocket Sneaker*/ { 0, 0 }, // Rocket Sneaker
		 /*Invincibility*/ { 2, 1 }, // Invincibility
				/*Banana*/ { 1, 0 }, // Banana
		/*Eggman Monitor*/ { 1, 0 }, // Eggman Monitor
			  /*Orbinaut*/ { 8, 0 }, // Orbinaut
				  /*Jawz*/ { 8, 1 }, // Jawz
				  /*Mine*/ { 6, 1 }, // Mine
			 /*Land Mine*/ { 0, 0 }, // Land Mine
			   /*Ballhog*/ { 2, 1 }, // Ballhog
   /*Self-Propelled Bomb*/ { 0, 0 }, // Self-Propelled Bomb
				  /*Grow*/ { 2, 1 }, // Grow
				/*Shrink*/ { 0, 0 }, // Shrink
		/*Thunder Shield*/ { 4, 0 }, // Thunder Shield
		 /*Bubble Shield*/ { 1, 0 }, // Bubble Shield
		  /*Flame Shield*/ { 0, 0 }, // Flame Shield
			   /*Hyudoro*/ { 2, 0 }, // Hyudoro
		   /*Pogo Spring*/ { 2, 0 }, // Pogo Spring
			/*Super Ring*/ { 0, 0 }, // Super Ring
		  /*Kitchen Sink*/ { 0, 0 }, // Kitchen Sink
			/*Sneaker x2*/ { 0, 0 }, // Sneaker x2
			/*Sneaker x3*/ { 1, 1 }, // Sneaker x3
			 /*Banana x3*/ { 1, 0 }, // Banana x3
			/*Banana x10*/ { 1, 1 }, // Banana x10
		   /*Orbinaut x3*/ { 2, 0 }, // Orbinaut x3
		   /*Orbinaut x4*/ { 1, 1 }, // Orbinaut x4
			   /*Jawz x2*/ { 2, 1 }  // Jawz x2
};

#define DISTVAR (2048) // Magic number distance for use with item roulette tiers

INT32 K_GetShieldFromItem(INT32 item)
{
	switch (item)
	{
		case KITEM_THUNDERSHIELD: return KSHIELD_THUNDER;
		case KITEM_BUBBLESHIELD: return KSHIELD_BUBBLE;
		case KITEM_FLAMESHIELD: return KSHIELD_FLAME;
		default: return KSHIELD_NONE;
	}
}

/**	\brief	Item Roulette for Kart

	\param	player		player
	\param	getitem		what item we're looking for

	\return	void
*/
static void K_KartGetItemResult(player_t *player, SINT8 getitem)
{
	if (getitem == KITEM_SPB || getitem == KITEM_SHRINK) // Indirect items
		indirectitemcooldown = 20*TICRATE;

	if (getitem == KITEM_HYUDORO) // Hyudoro cooldown
		hyubgone = 5*TICRATE;

	player->botvars.itemdelay = TICRATE;
	player->botvars.itemconfirm = 0;

	switch (getitem)
	{
		// Special roulettes first, then the generic ones are handled by default
		case KRITEM_DUALSNEAKER: // Sneaker x2
			player->kartstuff[k_itemtype] = KITEM_SNEAKER;
			player->kartstuff[k_itemamount] = 2;
			break;
		case KRITEM_TRIPLESNEAKER: // Sneaker x3
			player->kartstuff[k_itemtype] = KITEM_SNEAKER;
			player->kartstuff[k_itemamount] = 3;
			break;
		case KRITEM_TRIPLEBANANA: // Banana x3
			player->kartstuff[k_itemtype] = KITEM_BANANA;
			player->kartstuff[k_itemamount] = 3;
			break;
		case KRITEM_TENFOLDBANANA: // Banana x10
			player->kartstuff[k_itemtype] = KITEM_BANANA;
			player->kartstuff[k_itemamount] = 10;
			break;
		case KRITEM_TRIPLEORBINAUT: // Orbinaut x3
			player->kartstuff[k_itemtype] = KITEM_ORBINAUT;
			player->kartstuff[k_itemamount] = 3;
			break;
		case KRITEM_QUADORBINAUT: // Orbinaut x4
			player->kartstuff[k_itemtype] = KITEM_ORBINAUT;
			player->kartstuff[k_itemamount] = 4;
			break;
		case KRITEM_DUALJAWZ: // Jawz x2
			player->kartstuff[k_itemtype] = KITEM_JAWZ;
			player->kartstuff[k_itemamount] = 2;
			break;
		default:
			if (getitem <= 0 || getitem >= NUMKARTRESULTS) // Sad (Fallback)
			{
				if (getitem != 0)
					CONS_Printf("ERROR: P_KartGetItemResult - Item roulette gave bad item (%d) :(\n", getitem);
				player->kartstuff[k_itemtype] = KITEM_SAD;
			}
			else
				player->kartstuff[k_itemtype] = getitem;
			player->kartstuff[k_itemamount] = 1;
			break;
	}
}

/**	\brief	Item Roulette for Kart

	\param	player	player object passed from P_KartPlayerThink

	\return	void
*/

INT32 K_KartGetItemOdds(UINT8 pos, SINT8 item, fixed_t mashed, boolean spbrush, boolean bot, boolean rival)
{
	INT32 newodds;
	INT32 i;
	UINT8 pingame = 0, pexiting = 0;
	SINT8 first = -1, second = -1;
	INT32 secondist = 0;
	INT32 shieldtype = KSHIELD_NONE;

	I_Assert(item > KITEM_NONE); // too many off by one scenarioes.
	I_Assert(KartItemCVars[NUMKARTRESULTS-2] != NULL); // Make sure this exists

	if (!KartItemCVars[item-1]->value && !modeattacking)
		return 0;

	if (gametype == GT_BATTLE)
	{
		I_Assert(pos < 6); // DO NOT allow positions past the bounds of the table
		newodds = K_KartItemOddsBattle[item-1][pos];
	}
	else
	{
		I_Assert(pos < 8); // Ditto
		newodds = K_KartItemOddsRace[item-1][pos];
	}

	// Base multiplication to ALL item odds to simulate fractional precision
	newodds *= 4;

	shieldtype = K_GetShieldFromItem(item);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;

		if (!(gametyperules & GTR_BUMPERS) || players[i].bumpers)
			pingame++;

		if (players[i].exiting)
			pexiting++;

		if (shieldtype != KSHIELD_NONE && shieldtype == K_GetShieldFromItem(players[i].kartstuff[k_itemtype]))
		{
			// Don't allow more than one of each shield type at a time
			return 0;
		}

		if (players[i].mo && gametype == GT_RACE)
		{
			if (players[i].kartstuff[k_position] == 1 && first == -1)
				first = i;
			if (players[i].kartstuff[k_position] == 2 && second == -1)
				second = i;
		}
	}

	if (first != -1 && second != -1) // calculate 2nd's distance from 1st, for SPB
	{
		secondist = players[second].distancetofinish - players[first].distancetofinish;
		if (franticitems)
			secondist = (15 * secondist) / 14;
		secondist = ((28 + (8-pingame)) * secondist) / 28;
	}

	// POWERITEMODDS handles all of the "frantic item" related functionality, for all of our powerful items.
	// First, it multiplies it by 2 if franticitems is true; easy-peasy.
	// Next, it multiplies it again if it's in SPB mode and 2nd needs to apply pressure to 1st.
	// Then, it multiplies it further if the player count isn't equal to 8.
	// This is done to make low player count races more interesting and high player count rates more fair.
	// (2P normal would be about halfway between 8P normal and 8P frantic.)
	// (This scaling is not done for SPB Rush, so that catchup strength is not weakened.)
	// Lastly, it *divides* it by your mashed value, which was determined in K_KartItemRoulette, for lesser items needed in a pinch.

#define PLAYERSCALING (8 - (spbrush ? 2 : pingame))

#define POWERITEMODDS(odds) {\
	if (franticitems) \
		odds *= 2; \
	if (rival) \
		odds *= 2; \
	odds = FixedMul(odds * FRACUNIT, FRACUNIT + ((PLAYERSCALING * FRACUNIT) / 25)) / FRACUNIT; \
	if (mashed > 0) \
		odds = FixedDiv(odds * FRACUNIT, FRACUNIT + mashed) / FRACUNIT; \
}

#define COOLDOWNONSTART (leveltime < (30*TICRATE)+starttime)

	/*
	if (bot)
	{
		// TODO: Item use on bots should all be passed-in functions.
		// Instead of manually inserting these, it should return 0
		// for any items without an item use function supplied

		switch (item)
		{
			case KITEM_SNEAKER:
			case KITEM_ROCKETSNEAKER:
			case KITEM_INVINCIBILITY:
			case KITEM_BANANA:
			case KITEM_EGGMAN:
			case KITEM_ORBINAUT:
			case KITEM_JAWZ:
			case KITEM_MINE:
			case KITEM_LANDMINE:
			case KITEM_BALLHOG:
			case KITEM_SPB:
			case KITEM_GROW:
			case KITEM_SHRINK:
			case KITEM_HYUDORO:
			case KITEM_SUPERRING:
			case KITEM_THUNDERSHIELD:
			case KITEM_BUBBLESHIELD:
			case KITEM_FLAMESHIELD:
			case KRITEM_DUALSNEAKER:
			case KRITEM_TRIPLESNEAKER:
			case KRITEM_TRIPLEBANANA:
			case KRITEM_TENFOLDBANANA:
			case KRITEM_TRIPLEORBINAUT:
			case KRITEM_QUADORBINAUT:
			case KRITEM_DUALJAWZ:
				break;
			default:
				return 0;
		}
	}
	*/
	(void)bot;

	switch (item)
	{
		case KITEM_ROCKETSNEAKER:
		case KITEM_JAWZ:
		case KITEM_LANDMINE:
		case KITEM_BALLHOG:
		case KRITEM_TRIPLESNEAKER:
		case KRITEM_TRIPLEBANANA:
		case KRITEM_TENFOLDBANANA:
		case KRITEM_TRIPLEORBINAUT:
		case KRITEM_QUADORBINAUT:
		case KRITEM_DUALJAWZ:
			POWERITEMODDS(newodds);
			break;
		case KITEM_INVINCIBILITY:
		case KITEM_MINE:
		case KITEM_GROW:
		case KITEM_BUBBLESHIELD:
		case KITEM_FLAMESHIELD:
			if (COOLDOWNONSTART)
				newodds = 0;
			else
				POWERITEMODDS(newodds);
			break;
		case KITEM_SPB:
			if ((indirectitemcooldown > 0) || COOLDOWNONSTART
				|| (first != -1 && players[first].distancetofinish < 8*DISTVAR)) // No SPB near the end of the race
			{
				newodds = 0;
			}
			else
			{
				INT32 multiplier = (secondist - (5*DISTVAR)) / DISTVAR;

				if (multiplier < 0)
					multiplier = 0;
				if (multiplier > 3)
					multiplier = 3;

				newodds *= multiplier;
			}
			break;
		case KITEM_SHRINK:
			if ((indirectitemcooldown > 0) || COOLDOWNONSTART || (pingame-1 <= pexiting))
				newodds = 0;
			else
				POWERITEMODDS(newodds);
			break;
		case KITEM_THUNDERSHIELD:
			if (spbplace != -1 || COOLDOWNONSTART)
				newodds = 0;
			else
				POWERITEMODDS(newodds);
			break;
		case KITEM_HYUDORO:
			if ((hyubgone > 0) || COOLDOWNONSTART)
				newodds = 0;
			break;
		default:
			break;
	}

#undef POWERITEMODDS

	return newodds;
}

//{ SRB2kart Roulette Code - Distance Based, yes waypoints

UINT8 K_FindUseodds(player_t *player, fixed_t mashed, UINT32 pdis, UINT8 bestbumper, boolean spbrush)
{
	UINT8 i;
	UINT8 useodds = 0;
	UINT8 disttable[14];
	UINT8 distlen = 0;
	boolean oddsvalid[8];

	// Unused now, oops :V
	(void)bestbumper;

	for (i = 0; i < 8; i++)
	{
		UINT8 j;
		boolean available = false;

		if (gametype == GT_BATTLE && i > 1)
		{
			oddsvalid[i] = false;
			break;
		}

		for (j = 1; j < NUMKARTRESULTS; j++)
		{
			if (K_KartGetItemOdds(i, j, mashed, spbrush, player->bot, (player->bot && player->botvars.rival)) > 0)
			{
				available = true;
				break;
			}
		}

		oddsvalid[i] = available;
	}

#define SETUPDISTTABLE(odds, num) \
	if (oddsvalid[odds]) \
		for (i = num; i; --i) \
			disttable[distlen++] = odds;

	if (gametype == GT_BATTLE) // Battle Mode
	{
		if (player->kartstuff[k_roulettetype] == 1 && oddsvalid[1] == true)
		{
			// 1 is the extreme odds of player-controlled "Karma" items
			useodds = 1;
		}
		else
		{
			useodds = 0;

			if (oddsvalid[0] == false && oddsvalid[1] == true)
			{
				// try to use karma odds as a fallback
				useodds = 1;
			}
		}
	}
	else
	{
		SETUPDISTTABLE(0,1);
		SETUPDISTTABLE(1,1);
		SETUPDISTTABLE(2,1);
		SETUPDISTTABLE(3,2);
		SETUPDISTTABLE(4,2);
		SETUPDISTTABLE(5,3);
		SETUPDISTTABLE(6,3);
		SETUPDISTTABLE(7,1);

		if (pdis == 0)
			useodds = disttable[0];
		else if (pdis > DISTVAR * ((12 * distlen) / 14))
			useodds = disttable[distlen-1];
		else
		{
			for (i = 1; i < 13; i++)
			{
				if (pdis <= DISTVAR * ((i * distlen) / 14))
				{
					useodds = disttable[((i * distlen) / 14)];
					break;
				}
			}
		}
	}

#undef SETUPDISTTABLE

	return useodds;
}

static void K_KartItemRoulette(player_t *player, ticcmd_t *cmd)
{
	INT32 i;
	UINT8 pingame = 0;
	UINT8 roulettestop;
	UINT32 pdis = 0;
	UINT8 useodds = 0;
	INT32 spawnchance[NUMKARTRESULTS];
	INT32 totalspawnchance = 0;
	UINT8 bestbumper = 0;
	fixed_t mashed = 0;
	boolean dontforcespb = false;
	boolean spbrush = false;

	// This makes the roulette cycle through items - if this is 0, you shouldn't be here.
	if (player->kartstuff[k_itemroulette])
		player->kartstuff[k_itemroulette]++;
	else
		return;

	// Gotta check how many players are active at this moment.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator)
			continue;
		pingame++;
		if (players[i].exiting)
			dontforcespb = true;
		if (players[i].bumpers > bestbumper)
			bestbumper = players[i].bumpers;
	}

	// No forced SPB in 1v1s, it has to be randomly rolled
	if (pingame <= 2)
		dontforcespb = true;

	// This makes the roulette produce the random noises.
	if ((player->kartstuff[k_itemroulette] % 3) == 1 && P_IsDisplayPlayer(player) && !demo.freecam)
	{
#define PLAYROULETTESND S_StartSound(NULL, sfx_itrol1 + ((player->kartstuff[k_itemroulette] / 3) % 8))
		for (i = 0; i <= r_splitscreen; i++)
		{
			if (player == &players[displayplayers[i]] && players[displayplayers[i]].kartstuff[k_itemroulette])
				PLAYROULETTESND;
		}
#undef PLAYROULETTESND
	}

	roulettestop = TICRATE + (3*(pingame - player->kartstuff[k_position]));

	// If the roulette finishes or the player presses BT_ATTACK, stop the roulette and calculate the item.
	// I'm returning via the exact opposite, however, to forgo having another bracket embed. Same result either way, I think.
	// Finally, if you get past this check, now you can actually start calculating what item you get.
	if ((cmd->buttons & BT_ATTACK) && (player->kartstuff[k_itemroulette] >= roulettestop)
		&& !(player->kartstuff[k_eggmanheld] || player->kartstuff[k_itemheld] || player->kartstuff[k_userings]))
	{
		// Mashing reduces your chances for the good items
		mashed = FixedDiv((player->kartstuff[k_itemroulette])*FRACUNIT, ((TICRATE*3)+roulettestop)*FRACUNIT) - FRACUNIT;
	}
	else if (!(player->kartstuff[k_itemroulette] >= (TICRATE*3)))
		return;

	if (cmd->buttons & BT_ATTACK)
		player->pflags |= PF_ATTACKDOWN;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && !players[i].spectator
			&& players[i].kartstuff[k_position] == 1)
		{
			// This player is first! Yay!

			if (player->distancetofinish <= players[i].distancetofinish)
			{
				// Guess you're in first / tied for first?
				pdis = 0;
			}
			else
			{
				// Subtract 1st's distance from your distance, to get your distance from 1st!
				pdis = player->distancetofinish - players[i].distancetofinish;
			}
			break;
		}
	}

	if (mapobjectscale != FRACUNIT)
		pdis = FixedDiv(pdis * FRACUNIT, mapobjectscale) / FRACUNIT;

	if (franticitems) // Frantic items make the distances between everyone artifically higher, for crazier items
	{
		pdis = (15 * pdis) / 14;
	}

	if (spbplace != -1 && player->kartstuff[k_position] == spbplace+1) // SPB Rush Mode: It's 2nd place's job to catch-up items and make 1st place's job hell
	{
		pdis = (3 * pdis) / 2;
		spbrush = true;
	}

	if (player->bot && player->botvars.rival)
	{
		// Rival has better odds :)
		pdis = (15 * pdis) / 14;
	}

	pdis = ((28 + (8-pingame)) * pdis) / 28; // scale with player count

	// SPECIAL CASE No. 1:
	// Fake Eggman items
	if (player->kartstuff[k_roulettetype] == 2)
	{
		player->kartstuff[k_eggmanexplode] = 4*TICRATE;
		//player->karthud[khud_itemblink] = TICRATE;
		//player->karthud[khud_itemblinkmode] = 1;
		player->kartstuff[k_itemroulette] = 0;
		player->kartstuff[k_roulettetype] = 0;
		if (P_IsDisplayPlayer(player) && !demo.freecam)
			S_StartSound(NULL, sfx_itrole);
		return;
	}

	// SPECIAL CASE No. 2:
	// Give a debug item instead if specified
	if (cv_kartdebugitem.value != 0 && !modeattacking)
	{
		K_KartGetItemResult(player, cv_kartdebugitem.value);
		player->kartstuff[k_itemamount] = cv_kartdebugamount.value;
		player->karthud[khud_itemblink] = TICRATE;
		player->karthud[khud_itemblinkmode] = 2;
		player->kartstuff[k_itemroulette] = 0;
		player->kartstuff[k_roulettetype] = 0;
		if (P_IsDisplayPlayer(player) && !demo.freecam)
			S_StartSound(NULL, sfx_dbgsal);
		return;
	}

	// SPECIAL CASE No. 3:
	// Record Attack / alone mashing behavior
	if (modeattacking || pingame == 1)
	{
		if (gametype == GT_RACE)
		{
			if (mashed && (modeattacking || cv_superring.value)) // ANY mashed value? You get rings.
			{
				K_KartGetItemResult(player, KITEM_SUPERRING);
				player->karthud[khud_itemblinkmode] = 1;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolm);
			}
			else
			{
				if (modeattacking || cv_sneaker.value) // Waited patiently? You get a sneaker!
					K_KartGetItemResult(player, KITEM_SNEAKER);
				else  // Default to sad if nothing's enabled...
					K_KartGetItemResult(player, KITEM_SAD);
				player->karthud[khud_itemblinkmode] = 0;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolf);
			}
		}
		else if (gametype == GT_BATTLE)
		{
			if (mashed && (modeattacking || cv_banana.value)) // ANY mashed value? You get a banana.
			{
				K_KartGetItemResult(player, KITEM_BANANA);
				player->karthud[khud_itemblinkmode] = 1;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolm);
			}
			else
			{
				if (modeattacking || cv_tripleorbinaut.value) // Waited patiently? You get Orbinaut x3!
					K_KartGetItemResult(player, KRITEM_TRIPLEORBINAUT);
				else  // Default to sad if nothing's enabled...
					K_KartGetItemResult(player, KITEM_SAD);
				player->karthud[khud_itemblinkmode] = 0;
				if (P_IsDisplayPlayer(player))
					S_StartSound(NULL, sfx_itrolf);
			}
		}

		player->karthud[khud_itemblink] = TICRATE;
		player->kartstuff[k_itemroulette] = 0;
		player->kartstuff[k_roulettetype] = 0;
		return;
	}

	// SPECIAL CASE No. 4:
	// Being in ring debt occasionally forces Super Ring on you if you mashed
	if (!(gametyperules & GTR_SPHERES) && mashed && player->rings < 0 && cv_superring.value)
	{
		INT32 debtamount = min(20, abs(player->rings));
		if (P_RandomChance((debtamount*FRACUNIT)/20))
		{
			K_KartGetItemResult(player, KITEM_SUPERRING);
			player->karthud[khud_itemblink] = TICRATE;
			player->karthud[khud_itemblinkmode] = 1;
			player->kartstuff[k_itemroulette] = 0;
			player->kartstuff[k_roulettetype] = 0;
			if (P_IsDisplayPlayer(player))
				S_StartSound(NULL, sfx_itrolm);
			return;
		}
	}

	// SPECIAL CASE No. 5:
	// Force SPB onto 2nd if they get too far behind
	if ((gametyperules & GTR_CIRCUIT) && player->kartstuff[k_position] == 2 && pdis > (DISTVAR*8)
		&& spbplace == -1 && !indirectitemcooldown && !dontforcespb
		&& cv_selfpropelledbomb.value)
	{
		K_KartGetItemResult(player, KITEM_SPB);
		player->karthud[khud_itemblink] = TICRATE;
		player->karthud[khud_itemblinkmode] = (mashed ? 1 : 0);
		player->kartstuff[k_itemroulette] = 0;
		player->kartstuff[k_roulettetype] = 0;
		if (P_IsDisplayPlayer(player))
			S_StartSound(NULL, (mashed ? sfx_itrolm : sfx_itrolf));
		return;
	}

	// NOW that we're done with all of those specialized cases, we can move onto the REAL item roulette tables.
	// Initializes existing spawnchance values
	for (i = 0; i < NUMKARTRESULTS; i++)
		spawnchance[i] = 0;

	// Split into another function for a debug function below
	useodds = K_FindUseodds(player, mashed, pdis, bestbumper, spbrush);

	for (i = 1; i < NUMKARTRESULTS; i++)
		spawnchance[i] = (totalspawnchance += K_KartGetItemOdds(useodds, i, mashed, spbrush, player->bot, (player->bot && player->botvars.rival)));

	// Award the player whatever power is rolled
	if (totalspawnchance > 0)
	{
		totalspawnchance = P_RandomKey(totalspawnchance);
		for (i = 0; i < NUMKARTRESULTS && spawnchance[i] <= totalspawnchance; i++);

		K_KartGetItemResult(player, i);
	}
	else
	{
		player->kartstuff[k_itemtype] = KITEM_SAD;
		player->kartstuff[k_itemamount] = 1;
	}

	if (P_IsDisplayPlayer(player) && !demo.freecam)
		S_StartSound(NULL, ((player->kartstuff[k_roulettetype] == 1) ? sfx_itrolk : (mashed ? sfx_itrolm : sfx_itrolf)));

	player->karthud[khud_itemblink] = TICRATE;
	player->karthud[khud_itemblinkmode] = ((player->kartstuff[k_roulettetype] == 1) ? 2 : (mashed ? 1 : 0));

	player->kartstuff[k_itemroulette] = 0; // Since we're done, clear the roulette number
	player->kartstuff[k_roulettetype] = 0; // This too
}

//}

//{ SRB2kart p_user.c Stuff

static fixed_t K_PlayerWeight(mobj_t *mobj, mobj_t *against)
{
	fixed_t weight = 5*FRACUNIT;

	if (!mobj->player)
		return weight;

	if (against && !P_MobjWasRemoved(against) && against->player
		&& ((!P_PlayerInPain(against->player) && P_PlayerInPain(mobj->player)) // You're hurt
		|| (against->player->kartstuff[k_itemtype] == KITEM_BUBBLESHIELD && mobj->player->kartstuff[k_itemtype] != KITEM_BUBBLESHIELD))) // They have a Bubble Shield
	{
		weight = 0; // This player does not cause any bump action
	}
	else
	{
		weight = (mobj->player->kartweight) * FRACUNIT;
		if (mobj->player->speed > K_GetKartSpeed(mobj->player, false))
			weight += (mobj->player->speed - K_GetKartSpeed(mobj->player, false))/8;
		if (mobj->player->kartstuff[k_itemtype] == KITEM_BUBBLESHIELD)
			weight += 9*FRACUNIT;
	}

	return weight;
}

fixed_t K_GetMobjWeight(mobj_t *mobj, mobj_t *against)
{
	fixed_t weight = 5*FRACUNIT;

	switch (mobj->type)
	{
		case MT_PLAYER:
			if (!mobj->player)
				break;
			weight = K_PlayerWeight(mobj, against);
			break;
		case MT_BUBBLESHIELD:
			weight = K_PlayerWeight(mobj->target, against);
			break;
		case MT_FALLINGROCK:
			if (against->player)
			{
				if (against->player->kartstuff[k_invincibilitytimer] || against->player->kartstuff[k_growshrinktimer] > 0)
					weight = 0;
				else
					weight = K_PlayerWeight(against, NULL);
			}
			break;
		case MT_ORBINAUT:
		case MT_ORBINAUT_SHIELD:
			if (against->player)
				weight = K_PlayerWeight(against, NULL);
			break;
		case MT_JAWZ:
		case MT_JAWZ_DUD:
		case MT_JAWZ_SHIELD:
			if (against->player)
				weight = K_PlayerWeight(against, NULL) + (3*FRACUNIT);
			else
				weight += 3*FRACUNIT;
			break;
		default:
			break;
	}

	return FixedMul(weight, mobj->scale);
}

boolean K_KartBouncing(mobj_t *mobj1, mobj_t *mobj2, boolean bounce, boolean solid)
{
	mobj_t *fx;
	fixed_t momdifx, momdify;
	fixed_t distx, disty;
	fixed_t dot, force;
	fixed_t mass1, mass2;

	if (!mobj1 || !mobj2)
		return false;

	// Don't bump when you're being reborn
	if ((mobj1->player && mobj1->player->playerstate != PST_LIVE)
		|| (mobj2->player && mobj2->player->playerstate != PST_LIVE))
		return false;

	if ((mobj1->player && mobj1->player->respawn.state != RESPAWNST_NONE)
		|| (mobj2->player && mobj2->player->respawn.state != RESPAWNST_NONE))
		return false;

	{ // Don't bump if you're flashing
		INT32 flash;

		flash = K_GetKartFlashing(mobj1->player);
		if (mobj1->player && mobj1->player->powers[pw_flashing] > 0 && mobj1->player->powers[pw_flashing] < flash)
		{
			if (mobj1->player->powers[pw_flashing] < flash-1)
				mobj1->player->powers[pw_flashing]++;
			return false;
		}

		flash = K_GetKartFlashing(mobj2->player);
		if (mobj2->player && mobj2->player->powers[pw_flashing] > 0 && mobj2->player->powers[pw_flashing] < flash)
		{
			if (mobj2->player->powers[pw_flashing] < flash-1)
				mobj2->player->powers[pw_flashing]++;
			return false;
		}
	}

	// Don't bump if you've recently bumped
	if (mobj1->player && mobj1->player->kartstuff[k_justbumped])
	{
		mobj1->player->kartstuff[k_justbumped] = bumptime;
		return false;
	}

	if (mobj2->player && mobj2->player->kartstuff[k_justbumped])
	{
		mobj2->player->kartstuff[k_justbumped] = bumptime;
		return false;
	}

	mass1 = K_GetMobjWeight(mobj1, mobj2);

	if (solid == true && mass1 > 0)
		mass2 = mass1;
	else
		mass2 = K_GetMobjWeight(mobj2, mobj1);

	momdifx = mobj1->momx - mobj2->momx;
	momdify = mobj1->momy - mobj2->momy;

	// Adds the OTHER player's momentum times a bunch, for the best chance of getting the correct direction
	distx = (mobj1->x + mobj2->momx*3) - (mobj2->x + mobj1->momx*3);
	disty = (mobj1->y + mobj2->momy*3) - (mobj2->y + mobj1->momy*3);

	if (distx == 0 && disty == 0)
	{
		// if there's no distance between the 2, they're directly on top of each other, don't run this
		return false;
	}

	{ // Normalize distance to the sum of the two objects' radii, since in a perfect world that would be the distance at the point of collision...
		fixed_t dist = P_AproxDistance(distx, disty);
		fixed_t nx, ny;

		dist = dist ? dist : 1;

		nx = FixedDiv(distx, dist);
		ny = FixedDiv(disty, dist);

		distx = FixedMul(mobj1->radius+mobj2->radius, nx);
		disty = FixedMul(mobj1->radius+mobj2->radius, ny);

		if (momdifx == 0 && momdify == 0)
		{
			// If there's no momentum difference, they're moving at exactly the same rate. Pretend they moved into each other.
			momdifx = -nx;
			momdify = -ny;
		}
	}

	// if the speed difference is less than this let's assume they're going proportionately faster from each other
	if (P_AproxDistance(momdifx, momdify) < (25*mapobjectscale))
	{
		fixed_t momdiflength = P_AproxDistance(momdifx, momdify);
		fixed_t normalisedx = FixedDiv(momdifx, momdiflength);
		fixed_t normalisedy = FixedDiv(momdify, momdiflength);
		momdifx = FixedMul((25*mapobjectscale), normalisedx);
		momdify = FixedMul((25*mapobjectscale), normalisedy);
	}

	dot = FixedMul(momdifx, distx) + FixedMul(momdify, disty);

	if (dot >= 0)
	{
		// They're moving away from each other
		return false;
	}

	force = FixedDiv(dot, FixedMul(distx, distx)+FixedMul(disty, disty));

	if (bounce == true && mass2 > 0) // Perform a Goomba Bounce.
		mobj1->momz = -mobj1->momz;
	else
	{
		fixed_t newz = mobj1->momz;
		if (mass2 > 0)
			mobj1->momz = mobj2->momz;
		if (mass1 > 0 && solid == false)
			mobj2->momz = newz;
	}

	if (mass2 > 0)
	{
		mobj1->momx = mobj1->momx - FixedMul(FixedMul(FixedDiv(2*mass2, mass1 + mass2), force), distx);
		mobj1->momy = mobj1->momy - FixedMul(FixedMul(FixedDiv(2*mass2, mass1 + mass2), force), disty);
	}

	if (mass1 > 0 && solid == false)
	{
		mobj2->momx = mobj2->momx - FixedMul(FixedMul(FixedDiv(2*mass1, mass1 + mass2), force), -distx);
		mobj2->momy = mobj2->momy - FixedMul(FixedMul(FixedDiv(2*mass1, mass1 + mass2), force), -disty);
	}

	// Do the bump fx when we've CONFIRMED we can bump.
	if ((mobj1->player && mobj1->player->kartstuff[k_itemtype] == KITEM_BUBBLESHIELD) || (mobj2->player && mobj2->player->kartstuff[k_itemtype] == KITEM_BUBBLESHIELD))
		S_StartSound(mobj1, sfx_s3k44);
	else
		S_StartSound(mobj1, sfx_s3k49);

	fx = P_SpawnMobj(mobj1->x/2 + mobj2->x/2, mobj1->y/2 + mobj2->y/2, mobj1->z/2 + mobj2->z/2, MT_BUMP);
	if (mobj1->eflags & MFE_VERTICALFLIP)
		fx->eflags |= MFE_VERTICALFLIP;
	else
		fx->eflags &= ~MFE_VERTICALFLIP;
	P_SetScale(fx, mobj1->scale);

	// Because this is done during collision now, rmomx and rmomy need to be recalculated
	// so that friction doesn't immediately decide to stop the player if they're at a standstill
	// Also set justbumped here
	if (mobj1->player)
	{
		mobj1->player->rmomx = mobj1->momx - mobj1->player->cmomx;
		mobj1->player->rmomy = mobj1->momy - mobj1->player->cmomy;
		mobj1->player->kartstuff[k_justbumped] = bumptime;
		mobj1->player->kartstuff[k_spindash] = 0;

		if (mobj1->player->kartstuff[k_spinouttimer])
		{
			mobj1->player->kartstuff[k_wipeoutslow] = wipeoutslowtime+1;
			mobj1->player->kartstuff[k_spinouttimer] = max(wipeoutslowtime+1, mobj1->player->kartstuff[k_spinouttimer]);
			//mobj1->player->kartstuff[k_spinouttype] = KSPIN_WIPEOUT; // Enforce type
		}
	}

	if (mobj2->player)
	{
		mobj2->player->rmomx = mobj2->momx - mobj2->player->cmomx;
		mobj2->player->rmomy = mobj2->momy - mobj2->player->cmomy;
		mobj2->player->kartstuff[k_justbumped] = bumptime;
		mobj2->player->kartstuff[k_spindash] = 0;

		if (mobj2->player->kartstuff[k_spinouttimer])
		{
			mobj2->player->kartstuff[k_wipeoutslow] = wipeoutslowtime+1;
			mobj2->player->kartstuff[k_spinouttimer] = max(wipeoutslowtime+1, mobj2->player->kartstuff[k_spinouttimer]);
			//mobj2->player->kartstuff[k_spinouttype] = KSPIN_WIPEOUT; // Enforce type
		}
	}

	return true;
}

/**	\brief	Checks that the player is on an offroad subsector for realsies. Also accounts for line riding to prevent cheese.

	\param	mo	player mobj object

	\return	boolean
*/
static UINT8 K_CheckOffroadCollide(mobj_t *mo)
{
	// Check for sectors in touching_sectorlist
	UINT8 i;			// special type iter
	msecnode_t *node;	// touching_sectorlist iter
	sector_t *s;		// main sector shortcut
	sector_t *s2;		// FOF sector shortcut
	ffloor_t *rover;	// FOF

	fixed_t flr;
	fixed_t cel;	// floor & ceiling for height checks to make sure we're touching the offroad sector.

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	for (node = mo->touching_sectorlist; node; node = node->m_sectorlist_next)
	{
		if (!node->m_sector)
			break;	// shouldn't happen.

		s = node->m_sector;
		// 1: Check for the main sector, make sure we're on the floor of that sector and see if we can apply offroad.
		// Make arbitrary Z checks because we want to check for 1 sector in particular, we don't want to affect the player if the offroad sector is way below them and they're lineriding a normal sector above.

		flr = P_MobjFloorZ(mo, s, s, mo->x, mo->y, NULL, false, true);
		cel = P_MobjCeilingZ(mo, s, s, mo->x, mo->y, NULL, true, true);	// get Z coords of both floors and ceilings for this sector (this accounts for slopes properly.)
		// NOTE: we don't use P_GetZAt with our x/y directly because the mobj won't have the same height because of its hitbox on the slope. Complex garbage but tldr it doesn't work.

		if ( ((s->flags & SF_FLIPSPECIAL_FLOOR) && mo->z == flr)	// floor check
			|| ((mo->eflags & MFE_VERTICALFLIP && (s->flags & SF_FLIPSPECIAL_CEILING) && (mo->z + mo->height) == cel)) )	// ceiling check.

			for (i = 2; i < 5; i++)	// check for sector special

				if (GETSECSPECIAL(s->special, 1) == i)
					return i-1;	// return offroad type

		// 2: If we're here, we haven't found anything. So let's try looking for FOFs in the sectors using the same logic.
		for (rover = s->ffloors; rover; rover = rover->next)
		{
			if (!(rover->flags & FF_EXISTS))	// This FOF doesn't exist anymore.
				continue;

			s2 = &sectors[rover->secnum];	// makes things easier for us

			flr = P_GetFOFBottomZ(mo, s, rover, mo->x, mo->y, NULL);
			cel = P_GetFOFTopZ(mo, s, rover, mo->x, mo->y, NULL);	// Z coords for fof top/bottom.

			// we will do essentially the same checks as above instead of bothering with top/bottom height of the FOF.
			// Reminder that an FOF's floor is its bottom, silly!
			if ( ((s2->flags & SF_FLIPSPECIAL_FLOOR) && mo->z == cel)	// "floor" check
				|| ((s2->flags & SF_FLIPSPECIAL_CEILING) && (mo->z + mo->height) == flr) )	// "ceiling" check.

				for (i = 2; i < 5; i++)	// check for sector special

					if (GETSECSPECIAL(s2->special, 1) == i)
						return i-1;	// return offroad type

		}
	}
	return 0;	// couldn't find any offroad
}

/**	\brief	Updates the Player's offroad value once per frame

	\param	player	player object passed from K_KartPlayerThink

	\return	void
*/
static void K_UpdateOffroad(player_t *player)
{
	fixed_t offroadstrength = (K_CheckOffroadCollide(player->mo) << FRACBITS);

	// If you are in offroad, a timer starts.
	if (offroadstrength)
	{
		if (player->kartstuff[k_offroad] < offroadstrength)
			player->kartstuff[k_offroad] += offroadstrength / TICRATE;

		if (player->kartstuff[k_offroad] > offroadstrength)
			player->kartstuff[k_offroad] = offroadstrength;
	}
	else
		player->kartstuff[k_offroad] = 0;
}

static void K_DrawDraftCombiring(player_t *player, player_t *victim, fixed_t curdist, fixed_t maxdist, boolean transparent)
{
#define CHAOTIXBANDLEN 15
#define CHAOTIXBANDCOLORS 9
	static const UINT8 colors[CHAOTIXBANDCOLORS] = {
		SKINCOLOR_SAPPHIRE,
		SKINCOLOR_PLATINUM,
		SKINCOLOR_TEA,
		SKINCOLOR_GARDEN,
		SKINCOLOR_BANANA,
		SKINCOLOR_GOLD,
		SKINCOLOR_ORANGE,
		SKINCOLOR_SCARLET,
		SKINCOLOR_TAFFY
	};
	fixed_t minimumdist = FixedMul(RING_DIST>>1, player->mo->scale);
	UINT8 n = CHAOTIXBANDLEN;
	UINT8 offset = ((leveltime / 3) % 3);
	fixed_t stepx, stepy, stepz;
	fixed_t curx, cury, curz;
	UINT8 c;

	if (maxdist == 0)
		c = 0;
	else
		c = FixedMul(CHAOTIXBANDCOLORS<<FRACBITS, FixedDiv(curdist-minimumdist, maxdist-minimumdist)) >> FRACBITS;

	stepx = (victim->mo->x - player->mo->x) / CHAOTIXBANDLEN;
	stepy = (victim->mo->y - player->mo->y) / CHAOTIXBANDLEN;
	stepz = ((victim->mo->z + (victim->mo->height / 2)) - (player->mo->z + (player->mo->height / 2))) / CHAOTIXBANDLEN;

	curx = player->mo->x + stepx;
	cury = player->mo->y + stepy;
	curz = player->mo->z + stepz;

	while (n)
	{
		if (offset == 0)
		{
			mobj_t *band = P_SpawnMobj(curx + (P_RandomRange(-12,12)*mapobjectscale),
				cury + (P_RandomRange(-12,12)*mapobjectscale),
				curz + (P_RandomRange(24,48)*mapobjectscale),
				MT_SIGNSPARKLE);

			P_SetMobjState(band, S_SIGNSPARK1 + (leveltime % 11));
			P_SetScale(band, (band->destscale = (3*player->mo->scale)/2));

			band->color = colors[c];
			band->colorized = true;

			band->fuse = 2;

			if (transparent)
				band->drawflags |= MFD_SHADOW;

			band->drawflags |= MFD_DONTDRAW & ~(K_GetPlayerDontDrawFlag(player) | K_GetPlayerDontDrawFlag(victim));
		}

		curx += stepx;
		cury += stepy;
		curz += stepz;

		offset = abs(offset-1) % 3;
		n--;
	}
#undef CHAOTIXBANDLEN
}

/**	\brief	Updates the player's drafting values once per frame

	\param	player	player object passed from K_KartPlayerThink

	\return	void
*/
static void K_UpdateDraft(player_t *player)
{
	fixed_t topspd = K_GetKartSpeed(player, false);
	fixed_t draftdistance;
	UINT8 leniency;
	UINT8 i;

	if (player->kartstuff[k_itemtype] == KITEM_FLAMESHIELD)
	{
		// Flame Shield gets infinite draft distance as its passive effect.
		draftdistance = 0;
	}
	else
	{
		// Distance you have to be to draft. If you're still accelerating, then this distance is lessened.
		// This distance biases toward low weight! (min weight gets 4096 units, max weight gets 3072 units)
		// This distance is also scaled based on game speed.
		draftdistance = (3072 + (128 * (9 - player->kartweight))) * player->mo->scale;
		if (player->speed < topspd)
			draftdistance = FixedMul(draftdistance, FixedDiv(player->speed, topspd));
		draftdistance = FixedMul(draftdistance, K_GetKartGameSpeedScalar(gamespeed));
	}

	// On the contrary, the leniency period biases toward high weight.
	// (See also: the leniency variable in K_SpawnDraftDust)
	leniency = (3*TICRATE)/4 + ((player->kartweight-1) * (TICRATE/4));

	// Not enough speed to draft.
	if (player->speed >= 20*player->mo->scale)
	{
//#define EASYDRAFTTEST
		// Let's hunt for players to draft off of!
		for (i = 0; i < MAXPLAYERS; i++)
		{
			fixed_t dist, olddraft;
#ifndef EASYDRAFTTEST
			angle_t yourangle, theirangle, diff;
#endif

			if (!playeringame[i] || players[i].spectator || !players[i].mo)
				continue;

#ifndef EASYDRAFTTEST
			// Don't draft on yourself :V
			if (&players[i] == player)
				continue;
#endif

			// Not enough speed to draft off of.
			if (players[i].speed < 20*players[i].mo->scale)
				continue;

			// No tethering off of the guy who got the starting bonus :P
			if (players[i].kartstuff[k_startboost] > 0)
				continue;

#ifndef EASYDRAFTTEST
			yourangle = K_MomentumAngle(player->mo);
			theirangle = K_MomentumAngle(players[i].mo);

			diff = R_PointToAngle2(player->mo->x, player->mo->y, players[i].mo->x, players[i].mo->y) - yourangle;
			if (diff > ANGLE_180)
				diff = InvAngle(diff);

			// Not in front of this player.
			if (diff > ANG10)
				continue;

			diff = yourangle - theirangle;
			if (diff > ANGLE_180)
				diff = InvAngle(diff);

			// Not moving in the same direction.
			if (diff > ANGLE_90)
				continue;
#endif

			dist = P_AproxDistance(P_AproxDistance(players[i].mo->x - player->mo->x, players[i].mo->y - player->mo->y), players[i].mo->z - player->mo->z);

#ifndef EASYDRAFTTEST
			// TOO close to draft.
			if (dist < FixedMul(RING_DIST>>1, player->mo->scale))
				continue;

			// Not close enough to draft.
			if (dist > draftdistance && draftdistance > 0)
				continue;
#endif

			olddraft = player->kartstuff[k_draftpower];

			player->kartstuff[k_draftleeway] = leniency;
			player->kartstuff[k_lastdraft] = i;

			// Draft power is used later in K_GetKartBoostPower, ranging from 0 for normal speed and FRACUNIT for max draft speed.
			// How much this increments every tic biases toward acceleration! (min speed gets 1.5% per tic, max speed gets 0.5% per tic)
			if (player->kartstuff[k_draftpower] < FRACUNIT)
				player->kartstuff[k_draftpower] += (FRACUNIT/200) + ((9 - player->kartspeed) * ((3*FRACUNIT)/1600));

			if (player->kartstuff[k_draftpower] > FRACUNIT)
				player->kartstuff[k_draftpower] = FRACUNIT;

			// Play draft finish noise
			if (olddraft < FRACUNIT && player->kartstuff[k_draftpower] >= FRACUNIT)
				S_StartSound(player->mo, sfx_cdfm62);

			// Spawn in the visual!
			K_DrawDraftCombiring(player, &players[i], dist, draftdistance, false);

			return; // Finished doing our draft.
		}
	}

	// No one to draft off of? Then you can knock that off.
	if (player->kartstuff[k_draftleeway]) // Prevent small disruptions from stopping your draft.
	{
		player->kartstuff[k_draftleeway]--;
		if (player->kartstuff[k_lastdraft] >= 0
			&& player->kartstuff[k_lastdraft] < MAXPLAYERS
			&& playeringame[player->kartstuff[k_lastdraft]]
			&& !players[player->kartstuff[k_lastdraft]].spectator
			&& players[player->kartstuff[k_lastdraft]].mo)
		{
			player_t *victim = &players[player->kartstuff[k_lastdraft]];
			fixed_t dist = P_AproxDistance(P_AproxDistance(victim->mo->x - player->mo->x, victim->mo->y - player->mo->y), victim->mo->z - player->mo->z);
			K_DrawDraftCombiring(player, victim, dist, draftdistance, true);
		}
	}
	else // Remove draft speed boost.
	{
		player->kartstuff[k_draftpower] = 0;
		player->kartstuff[k_lastdraft] = -1;
	}
}

void K_KartPainEnergyFling(player_t *player)
{
	static const UINT8 numfling = 5;
	INT32 i;
	mobj_t *mo;
	angle_t fa;
	fixed_t ns;
	fixed_t z;

	// Better safe than sorry.
	if (!player)
		return;

	// P_PlayerRingBurst: "There's no ring spilling in kart, so I'm hijacking this for the same thing as TD"
	// :oh:

	for (i = 0; i < numfling; i++)
	{
		INT32 objType = mobjinfo[MT_FLINGENERGY].reactiontime;
		fixed_t momxy, momz; // base horizonal/vertical thrusts

		z = player->mo->z;
		if (player->mo->eflags & MFE_VERTICALFLIP)
			z += player->mo->height - mobjinfo[objType].height;

		mo = P_SpawnMobj(player->mo->x, player->mo->y, z, objType);

		mo->fuse = 8*TICRATE;
		P_SetTarget(&mo->target, player->mo);

		mo->destscale = player->mo->scale;
		P_SetScale(mo, player->mo->scale);

		// Angle offset by player angle, then slightly offset by amount of fling
		fa = ((i*FINEANGLES/16) + (player->mo->angle>>ANGLETOFINESHIFT) - ((numfling-1)*FINEANGLES/32)) & FINEMASK;

		if (i > 15)
		{
			momxy = 3*FRACUNIT;
			momz = 4*FRACUNIT;
		}
		else
		{
			momxy = 28*FRACUNIT;
			momz = 3*FRACUNIT;
		}

		ns = FixedMul(momxy, mo->scale);
		mo->momx = FixedMul(FINECOSINE(fa),ns);

		ns = momz;
		P_SetObjectMomZ(mo, ns, false);

		if (i & 1)
			P_SetObjectMomZ(mo, ns, true);

		if (player->mo->eflags & MFE_VERTICALFLIP)
			mo->momz *= -1;
	}
}

// Adds gravity flipping to an object relative to its master and shifts the z coordinate accordingly.
void K_FlipFromObject(mobj_t *mo, mobj_t *master)
{
	mo->eflags = (mo->eflags & ~MFE_VERTICALFLIP)|(master->eflags & MFE_VERTICALFLIP);
	mo->flags2 = (mo->flags2 & ~MF2_OBJECTFLIP)|(master->flags2 & MF2_OBJECTFLIP);

	if (mo->eflags & MFE_VERTICALFLIP)
		mo->z += master->height - FixedMul(master->scale, mo->height);
}

void K_MatchGenericExtraFlags(mobj_t *mo, mobj_t *master)
{
	// flipping
	// handle z shifting from there too. This is here since there's no reason not to flip us if needed when we do this anyway;
	K_FlipFromObject(mo, master);

	// visibility (usually for hyudoro)
	mo->drawflags = (master->drawflags & MFD_DONTDRAW);
}

// same as above, but does not adjust Z height when flipping
void K_GenericExtraFlagsNoZAdjust(mobj_t *mo, mobj_t *master)
{
	// flipping
	mo->eflags = (mo->eflags & ~MFE_VERTICALFLIP)|(master->eflags & MFE_VERTICALFLIP);
	mo->flags2 = (mo->flags2 & ~MF2_OBJECTFLIP)|(master->flags2 & MF2_OBJECTFLIP);

	// visibility (usually for hyudoro)
	mo->drawflags = (master->drawflags & MFD_DONTDRAW);
}


void K_SpawnDashDustRelease(player_t *player)
{
	fixed_t newx;
	fixed_t newy;
	mobj_t *dust;
	angle_t travelangle;
	INT32 i;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	if (!P_IsObjectOnGround(player->mo))
		return;

	if (!player->speed && !player->kartstuff[k_startboost] && !player->kartstuff[k_spindash])
		return;

	travelangle = player->mo->angle;

	if (player->kartstuff[k_drift] || player->kartstuff[k_driftend])
		travelangle -= (ANGLE_45/5)*player->kartstuff[k_drift];

	for (i = 0; i < 2; i++)
	{
		newx = player->mo->x + P_ReturnThrustX(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_90, FixedMul(48*FRACUNIT, player->mo->scale));
		newy = player->mo->y + P_ReturnThrustY(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_90, FixedMul(48*FRACUNIT, player->mo->scale));
		dust = P_SpawnMobj(newx, newy, player->mo->z, MT_FASTDUST);

		P_SetTarget(&dust->target, player->mo);
		dust->angle = travelangle - ((i&1) ? -1 : 1)*ANGLE_45;
		dust->destscale = player->mo->scale;
		P_SetScale(dust, player->mo->scale);

		dust->momx = 3*player->mo->momx/5;
		dust->momy = 3*player->mo->momy/5;
		//dust->momz = 3*player->mo->momz/5;

		K_MatchGenericExtraFlags(dust, player->mo);
	}
}

static void K_SpawnBrakeDriftSparks(player_t *player) // Be sure to update the mobj thinker case too!
{
	mobj_t *sparks;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	// Position & etc are handled in its thinker, and its spawned invisible.
	// This avoids needing to dupe code if we don't need it.
	sparks = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_BRAKEDRIFT);
	P_SetTarget(&sparks->target, player->mo);
	P_SetScale(sparks, (sparks->destscale = player->mo->scale));
	K_MatchGenericExtraFlags(sparks, player->mo);
	sparks->drawflags |= MFD_DONTDRAW;
}

static fixed_t K_RandomFlip(fixed_t f)
{
	return ( ( leveltime & 1 ) ? f : -f );
}

void K_SpawnDriftBoostClip(player_t *player)
{
	mobj_t *clip;
	fixed_t scale = 115*FRACUNIT/100;
	fixed_t z;

	if (( player->mo->eflags & MFE_VERTICALFLIP ))
		z = player->mo->z;
	else
		z = player->mo->z + player->mo->height;

	clip = P_SpawnMobj(player->mo->x, player->mo->y, z, MT_DRIFTCLIP);

	P_SetTarget(&clip->target, player->mo);
	P_SetScale(clip, ( clip->destscale = FixedMul(scale, player->mo->scale) ));
	K_MatchGenericExtraFlags(clip, player->mo);

	clip->fuse = 105;
	clip->momz = 7 * P_MobjFlip(clip) * clip->scale;

	if (player->mo->momz > 0)
		clip->momz += player->mo->momz;

	P_InstaThrust(clip, player->mo->angle +
			K_RandomFlip(P_RandomRange(FRACUNIT/2, FRACUNIT)),
			FixedMul(scale, player->speed));
}

void K_SpawnDriftBoostClipSpark(mobj_t *clip)
{
	mobj_t *spark;

	spark = P_SpawnMobj(clip->x, clip->y, clip->z, MT_DRIFTCLIPSPARK);

	P_SetTarget(&spark->target, clip);
	P_SetScale(spark, ( spark->destscale = clip->scale ));
	K_MatchGenericExtraFlags(spark, clip);

	spark->momx = clip->momx/2;
	spark->momy = clip->momx/2;
}

/**	\brief Handles the state changing for moving players, moved here to eliminate duplicate code

	\param	player	player data

	\return	void
*/
void K_KartMoveAnimation(player_t *player)
{
	const INT16 minturn = KART_FULLTURN/8;
	SINT8 turndir = 0;

	const fixed_t fastspeed = (K_GetKartSpeed(player, false) * 17) / 20; // 85%
	const fixed_t speedthreshold = player->mo->scale / 8;

	const boolean onground = P_IsObjectOnGround(player->mo);

	ticcmd_t *cmd = &player->cmd;
	const boolean spinningwheels = ((cmd->buttons & BT_ACCELERATE) || (onground && player->speed > 0));

	if (cmd->turning < -minturn)
	{
		turndir = -1;
	}
	else if (cmd->turning > minturn)
	{
		turndir = 1;
	}

#define SetState(sn) \
	if (player->mo->state != &states[sn]) \
		P_SetPlayerMobjState(player->mo, sn)

	if (!onground)
	{
		// Only use certain frames in the air, to make it look like your tires are spinning fruitlessly!

		if (player->kartstuff[k_drift] > 0)
		{
			// Neutral drift
			SetState(S_KART_DRIFT_L);
		}
		else if (player->kartstuff[k_drift] > 0)
		{
			// Neutral drift
			SetState(S_KART_DRIFT_R);
		}
		else
		{
			if (turndir == -1)
			{
				SetState(S_KART_FAST_R);
			}
			else if (turndir == 1)
			{
				SetState(S_KART_FAST_L);
			}
			else if (turndir == 0)
			{
				SetState(S_KART_FAST);
			}
		}

		if (!spinningwheels)
		{
			// TODO: These should prooobably be different SPR2s
			// Just a quick hack to prevent needing to do that :V
			player->mo->frame = (player->mo->frame & ~FF_FRAMEMASK);
		}
	}
	else
	{
		if (player->kartstuff[k_drift] > 0)
		{
			// Drifting LEFT!

			if (turndir == -1)
			{
				// Right -- outwards drift
				SetState(S_KART_DRIFT_L_OUT);
			}
			else if (turndir == 1)
			{
				// Left -- inwards drift
				SetState(S_KART_DRIFT_L_IN);
			}
			else
			{
				// Neutral drift
				SetState(S_KART_DRIFT_L);
			}
		}
		else if (player->kartstuff[k_drift] < 0)
		{
			// Drifting RIGHT!

			if (turndir == -1)
			{
				// Right -- inwards drift
				SetState(S_KART_DRIFT_R_IN);
			}
			else if (turndir == 1)
			{
				// Left -- outwards drift
				SetState(S_KART_DRIFT_R_OUT);
			}
			else
			{
				// Neutral drift
				SetState(S_KART_DRIFT_R);
			}
		}
		else
		{
			if (player->speed >= fastspeed && player->speed >= (player->lastspeed - speedthreshold))
			{
				// Going REAL fast!

				if (turndir == -1)
				{
					SetState(S_KART_FAST_R);
				}
				else if (turndir == 1)
				{
					SetState(S_KART_FAST_L);
				}
				else
				{
					SetState(S_KART_FAST);
				}
			}
			else
			{
				if (spinningwheels)
				{
					// Drivin' slow.

					if (turndir == -1)
					{
						SetState(S_KART_SLOW_R);
					}
					else if (turndir == 1)
					{
						SetState(S_KART_SLOW_L);
					}
					else
					{
						SetState(S_KART_SLOW);
					}
				}
				else
				{
					// Completely still.

					if (turndir == -1)
					{
						SetState(S_KART_STILL_R);
					}
					else if (turndir == 1)
					{
						SetState(S_KART_STILL_L);
					}
					else
					{
						SetState(S_KART_STILL);
					}
				}
			}
		}
	}

#undef SetState

	// Update lastspeed value -- we use to display slow driving frames instead of fast driving when slowing down.
	player->lastspeed = player->speed;
}

static void K_TauntVoiceTimers(player_t *player)
{
	if (!player)
		return;

	player->karthud[khud_tauntvoices] = 6*TICRATE;
	player->karthud[khud_voices] = 4*TICRATE;
}

static void K_RegularVoiceTimers(player_t *player)
{
	if (!player)
		return;

	player->karthud[khud_voices] = 4*TICRATE;

	if (player->karthud[khud_tauntvoices] < 4*TICRATE)
		player->karthud[khud_tauntvoices] = 4*TICRATE;
}

void K_PlayAttackTaunt(mobj_t *source)
{
	sfxenum_t pick = P_RandomKey(2); // Gotta roll the RNG every time this is called for sync reasons
	boolean tasteful = (!source->player || !source->player->karthud[khud_tauntvoices]);

	if (cv_kartvoices.value && (tasteful || cv_kartvoices.value == 2))
		S_StartSound(source, sfx_kattk1+pick);

	if (!tasteful)
		return;

	K_TauntVoiceTimers(source->player);
}

void K_PlayBoostTaunt(mobj_t *source)
{
	sfxenum_t pick = P_RandomKey(2); // Gotta roll the RNG every time this is called for sync reasons
	boolean tasteful = (!source->player || !source->player->karthud[khud_tauntvoices]);

	if (cv_kartvoices.value && (tasteful || cv_kartvoices.value == 2))
		S_StartSound(source, sfx_kbost1+pick);

	if (!tasteful)
		return;

	K_TauntVoiceTimers(source->player);
}

void K_PlayOvertakeSound(mobj_t *source)
{
	boolean tasteful = (!source->player || !source->player->karthud[khud_voices]);

	if (!gametype == GT_RACE) // Only in race
		return;

	// 4 seconds from before race begins, 10 seconds afterwards
	if (leveltime < starttime+(10*TICRATE))
		return;

	if (cv_kartvoices.value && (tasteful || cv_kartvoices.value == 2))
		S_StartSound(source, sfx_kslow);

	if (!tasteful)
		return;

	K_RegularVoiceTimers(source->player);
}

void K_PlayPainSound(mobj_t *source)
{
	sfxenum_t pick = P_RandomKey(2); // Gotta roll the RNG every time this is called for sync reasons

	if (cv_kartvoices.value)
		S_StartSound(source, sfx_khurt1 + pick);

	K_RegularVoiceTimers(source->player);
}

void K_PlayHitEmSound(mobj_t *source)
{

	if (source->player->follower)
	{
		follower_t fl = followers[source->player->followerskin];
		source->player->follower->movecount = fl.hitconfirmtime;	// movecount is used to play the hitconfirm animation for followers.
	}

	if (cv_kartvoices.value)
		S_StartSound(source, sfx_khitem);
	else
		S_StartSound(source, sfx_s1c9); // The only lost gameplay functionality with voices disabled

	K_RegularVoiceTimers(source->player);
}

void K_PlayPowerGloatSound(mobj_t *source)
{
	if (cv_kartvoices.value)
		S_StartSound(source, sfx_kgloat);

	K_RegularVoiceTimers(source->player);
}

void K_MomentumToFacing(player_t *player)
{
	angle_t dangle = player->mo->angle - K_MomentumAngle(player->mo);

	if (dangle > ANGLE_180)
		dangle = InvAngle(dangle);

	// If you aren't on the ground or are moving in too different of a direction don't do this
	if (player->mo->eflags & MFE_JUSTHITFLOOR)
		; // Just hit floor ALWAYS redirects
	else if (!P_IsObjectOnGround(player->mo) || dangle > ANGLE_90)
		return;

	P_Thrust(player->mo, player->mo->angle, player->speed - FixedMul(player->speed, player->mo->friction));
	player->mo->momx = FixedMul(player->mo->momx - player->cmomx, player->mo->friction) + player->cmomx;
	player->mo->momy = FixedMul(player->mo->momy - player->cmomy, player->mo->friction) + player->cmomy;
}

boolean K_ApplyOffroad(player_t *player)
{
	if (player->kartstuff[k_invincibilitytimer] || player->kartstuff[k_hyudorotimer] || player->kartstuff[k_sneakertimer])
		return false;
	return true;
}

static fixed_t K_FlameShieldDashVar(INT32 val)
{
	// 1 second = 75% + 50% top speed
	return (3*FRACUNIT/4) + (((val * FRACUNIT) / TICRATE) / 2);
}

INT16 K_GetSpindashChargeTime(player_t *player)
{
	// more charge time for higher speed
	// Tails = 2s, Mighty = 3s, Fang = 4s, Metal = 4s
	return (player->kartspeed + 4) * (TICRATE/3); 
}

fixed_t K_GetSpindashChargeSpeed(player_t *player)
{
	// more speed for higher weight & speed
	// Tails = +6.25%, Fang = +20.31%, Mighty = +20.31%, Metal = +25%
	// (can be higher than this value when overcharged)
	return (player->kartspeed + player->kartweight) * (FRACUNIT/64);
}


// sets boostpower, speedboost, accelboost, and handleboost to whatever we need it to be
static void K_GetKartBoostPower(player_t *player)
{
	// Light weights have stronger boost stacking -- aka, better metabolism than heavies XD
	const fixed_t maxmetabolismincrease = FRACUNIT/2;
	const fixed_t metabolism = FRACUNIT - ((9-player->kartweight) * maxmetabolismincrease / 8);

	// v2 almost broke sliptiding when it fixed turning bugs!
	// This value is fine-tuned to feel like v1 again without reverting any of those changes.
	const fixed_t sliptidehandling = FRACUNIT/2;

	fixed_t boostpower = FRACUNIT;
	fixed_t speedboost = 0, accelboost = 0, handleboost = 0;
	UINT8 numboosts = 0;

	if (player->kartstuff[k_spinouttimer] && player->kartstuff[k_wipeoutslow] == 1) // Slow down after you've been bumped
	{
		player->kartstuff[k_boostpower] = player->kartstuff[k_speedboost] = player->kartstuff[k_accelboost] = 0;
		return;
	}

	// Offroad is separate, it's difficult to factor it in with a variable value anyway.
	if (K_ApplyOffroad(player) && player->kartstuff[k_offroad] >= 0)
		boostpower = FixedDiv(boostpower, FixedMul(player->kartstuff[k_offroad], K_GetKartGameSpeedScalar(gamespeed)) + FRACUNIT);

	if (player->kartstuff[k_bananadrag] > TICRATE)
		boostpower = (4*boostpower)/5;

	// Note: Handling will ONLY stack when sliptiding!
	// When you're not, it just uses the best instead of adding together, like the old behavior.
#define ADDBOOST(s,a,h) { \
	numboosts++; \
	speedboost += FixedDiv(s, FRACUNIT + (metabolism * (numboosts-1))); \
	accelboost += FixedDiv(a, FRACUNIT + (metabolism * (numboosts-1))); \
	if (player->kartstuff[k_aizdriftstrat]) \
		handleboost += FixedDiv(h, FRACUNIT + (metabolism * (numboosts-1))); \
	else \
		handleboost = max(h, handleboost); \
}

	if (player->kartstuff[k_sneakertimer]) // Sneaker
	{
		UINT8 i;
		for (i = 0; i < player->kartstuff[k_numsneakers]; i++)
		{
			ADDBOOST(FRACUNIT/2, 8*FRACUNIT, sliptidehandling); // + 50% top speed, + 800% acceleration, +50% handling
		}
	}

	if (player->kartstuff[k_invincibilitytimer]) // Invincibility
	{
		ADDBOOST(3*FRACUNIT/8, 3*FRACUNIT, sliptidehandling/2); // + 37.5% top speed, + 300% acceleration, +25% handling
	}

	if (player->kartstuff[k_growshrinktimer] > 0) // Grow
	{
		ADDBOOST(0, 0, sliptidehandling/2); // + 0% top speed, + 0% acceleration, +25% handling
	}

	if (player->kartstuff[k_flamedash]) // Flame Shield dash
	{
		fixed_t dash = K_FlameShieldDashVar(player->kartstuff[k_flamedash]);
		ADDBOOST(
			dash, // + infinite top speed
			3*FRACUNIT, // + 300% acceleration
			FixedMul(FixedDiv(dash, FRACUNIT/2), sliptidehandling/2) // + infinite handling
		);
	}

	if (player->kartstuff[k_spindashboost]) // Spindash boost
	{
		const fixed_t MAXCHARGESPEED = K_GetSpindashChargeSpeed(player);

		// character & charge dependent
		ADDBOOST(
			FixedMul(MAXCHARGESPEED, player->kartstuff[k_spindashspeed]), // + 0 to K_GetSpindashChargeSpeed()% top speed
			(4*FRACUNIT) + (36*player->kartstuff[k_spindashspeed]), // + 400% to 4000% acceleration
			0 // + 0% handling
		);
	}

	if (player->kartstuff[k_startboost]) // Startup Boost
	{
		ADDBOOST(FRACUNIT/2, 4*FRACUNIT, 0); // + 50% top speed, + 400% acceleration, +0% handling
	}

	if (player->kartstuff[k_driftboost]) // Drift Boost
	{
		ADDBOOST(FRACUNIT/4, 4*FRACUNIT, 0); // + 25% top speed, + 400% acceleration, +0% handling
	}

	if (player->kartstuff[k_ringboost]) // Ring Boost
	{
		ADDBOOST(FRACUNIT/5, 4*FRACUNIT, 0); // + 20% top speed, + 400% acceleration, +0% handling
	}

	if (player->kartstuff[k_eggmanexplode]) // Ready-to-explode
	{
		ADDBOOST(3*FRACUNIT/20, FRACUNIT, 0); // + 15% top speed, + 100% acceleration, +0% handling
	}

	if (player->kartstuff[k_draftpower] > 0) // Drafting
	{
		fixed_t draftspeed = ((3*FRACUNIT)/10) + ((player->kartspeed-1) * (FRACUNIT/50)); // min is 30%, max is 46%
		speedboost += FixedMul(draftspeed, player->kartstuff[k_draftpower]); // (Drafting suffers no boost stack penalty.)
		numboosts++;
	}

	player->kartstuff[k_boostpower] = boostpower;

	// value smoothing
	if (speedboost > player->kartstuff[k_speedboost])
	{
		player->kartstuff[k_speedboost] = speedboost;
	}
	else
	{
		player->kartstuff[k_speedboost] += (speedboost - player->kartstuff[k_speedboost]) / (TICRATE/2);
	}

	player->kartstuff[k_accelboost] = accelboost;
	player->kartstuff[k_handleboost] = handleboost;

	player->kartstuff[k_numboosts] = numboosts;
}

// Returns kart speed from a stat. Boost power and scale are NOT taken into account, no player or object is necessary.
fixed_t K_GetKartSpeedFromStat(UINT8 kartspeed)
{
	const fixed_t xspd = (3*FRACUNIT)/64;
	fixed_t g_cc = K_GetKartGameSpeedScalar(gamespeed) + xspd;
	fixed_t k_speed = 150;
	fixed_t finalspeed;

	k_speed += kartspeed*3; // 153 - 177

	finalspeed = FixedMul(k_speed<<14, g_cc);
	return finalspeed;
}

fixed_t K_GetKartSpeed(player_t *player, boolean doboostpower)
{
	fixed_t finalspeed;

	finalspeed = K_GetKartSpeedFromStat(player->kartspeed);

	if (player->spheres > 0)
	{
		fixed_t sphereAdd = (FRACUNIT/80); // 50% at max
		finalspeed = FixedMul(finalspeed, FRACUNIT + (sphereAdd * player->spheres));
	}

	if (K_PlayerUsesBotMovement(player))
	{
		// Give top speed a buff for bots, since it's a fairly weak stat without drifting
		fixed_t speedmul = ((player->kartspeed-1) * FRACUNIT / 8) / 10; // +10% for speed 9

		if (player->botvars.rival == true)
		{
			speedmul += FRACUNIT/10; // +10% for rival
		}

		finalspeed = FixedMul(finalspeed, FRACUNIT + speedmul);
	}

	if (player->mo && !P_MobjWasRemoved(player->mo))
		finalspeed = FixedMul(finalspeed, player->mo->scale);

	if (doboostpower)
	{
		if (K_PlayerUsesBotMovement(player))
		{
			finalspeed = FixedMul(finalspeed, K_BotTopSpeedRubberband(player));
		}

		return FixedMul(finalspeed, player->kartstuff[k_boostpower]+player->kartstuff[k_speedboost]);
	}

	return finalspeed;
}

fixed_t K_GetKartAccel(player_t *player)
{
	fixed_t k_accel = 32; // 36;

	//k_accel += 3 * (9 - player->kartspeed); // 36 - 60
	k_accel += 4 * (9 - player->kartspeed); // 32 - 64

	if (player->spheres > 0)
	{
		fixed_t sphereAdd = (FRACUNIT/10); // 500% at max
		k_accel = FixedMul(k_accel, FRACUNIT + (sphereAdd * player->spheres));
	}

	if (K_PlayerUsesBotMovement(player))
	{
		// Rubberbanding acceleration is waekened since it makes hits feel more meaningful
		fixed_t rubberband = K_BotRubberband(player) - FRACUNIT;
		k_accel = FixedMul(k_accel, FRACUNIT + (rubberband/2));
	}

	return FixedMul(k_accel, FRACUNIT+player->kartstuff[k_accelboost]);
}

UINT16 K_GetKartFlashing(player_t *player)
{
	UINT16 tics = flashingtics;

	if (!player)
		return tics;

	tics += (tics/8) * (player->kartspeed);

	return tics;
}

SINT8 K_GetForwardMove(player_t *player)
{
	SINT8 forwardmove = player->cmd.forwardmove;

	if ((player->pflags & PF_STASIS) || (player->pflags & PF_SLIDING))
	{
		return 0;
	}

	if (player->kartstuff[k_sneakertimer] || player->kartstuff[k_spindashboost])
	{
		return MAXPLMOVE;
	}

	if (player->kartstuff[k_spinouttimer] || K_PlayerEBrake(player))
	{
		return 0;
	}

	return forwardmove;
}

fixed_t K_3dKartMovement(player_t *player)
{
	const fixed_t accelmax = 4000;
	const fixed_t p_speed = K_GetKartSpeed(player, true);
	const fixed_t p_accel = K_GetKartAccel(player);
	fixed_t newspeed, oldspeed, finalspeed;
	fixed_t movemul = FRACUNIT;
	fixed_t orig = ORIG_FRICTION;
	SINT8 forwardmove = K_GetForwardMove(player);

	if (K_PlayerUsesBotMovement(player))
	{
		orig = K_BotFrictionRubberband(player, ORIG_FRICTION);
	}

	// ACCELCODE!!!1!11!
	oldspeed = R_PointToDist2(0, 0, player->rmomx, player->rmomy); // FixedMul(P_AproxDistance(player->rmomx, player->rmomy), player->mo->scale);
	// Don't calculate the acceleration as ever being above top speed
	if (oldspeed > p_speed)
		oldspeed = p_speed;
	newspeed = FixedDiv(FixedDiv(FixedMul(oldspeed, accelmax - p_accel) + FixedMul(p_speed, p_accel), accelmax), orig);

	if (player->kartstuff[k_pogospring]) // Pogo Spring minimum/maximum thrust
	{
		const fixed_t hscale = mapobjectscale /*+ (mapobjectscale - player->mo->scale)*/;
		const fixed_t minspeed = 24*hscale;
		const fixed_t maxspeed = 28*hscale;

		if (newspeed > maxspeed && player->kartstuff[k_pogospring] == 2)
			newspeed = maxspeed;
		if (newspeed < minspeed)
			newspeed = minspeed;
	}

	finalspeed = newspeed - oldspeed;
	movemul = abs(forwardmove * FRACUNIT) / 50;

	// forwardmove is:
	//  50 while accelerating,
	//   0 with no gas, and
	// -25 when only braking.
	if (forwardmove >= 0)
	{
		finalspeed = FixedMul(finalspeed, movemul);
	}
	else
	{
		finalspeed = FixedMul(-mapobjectscale/2, movemul);
	}

	return finalspeed;
}

angle_t K_MomentumAngle(mobj_t *mo)
{
	if (mo->momx || mo->momy)
	{
		return R_PointToAngle2(0, 0, mo->momx, mo->momy);
	}
	else
	{
		return mo->angle; // default to facing angle, rather than 0
	}
}

void K_DoInstashield(player_t *player)
{
	mobj_t *layera;
	mobj_t *layerb;

	if (player->kartstuff[k_instashield] > 0)
		return;

	player->kartstuff[k_instashield] = 15; // length of instashield animation
	S_StartSound(player->mo, sfx_cdpcm9);

	layera = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_INSTASHIELDA);
	P_SetTarget(&layera->target, player->mo);

	layerb = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_INSTASHIELDB);
	P_SetTarget(&layerb->target, player->mo);
}

void K_BattleAwardHit(player_t *player, player_t *victim, mobj_t *inflictor, UINT8 bumpersRemoved)
{
	UINT8 points = 1;
	boolean trapItem = false;

	if (player == NULL || victim == NULL)
	{
		// Invalid player or victim
		return;
	}

	if (player == victim)
	{
		// You cannot give yourself points
		return;
	}

	if ((inflictor && !P_MobjWasRemoved(inflictor)) && (inflictor->type == MT_BANANA && inflictor->health > 1))
	{
		trapItem = true;
	}

	// Only apply score bonuses to non-bananas
	if (trapItem == false)
	{
		if (K_IsPlayerWanted(victim))
		{
			// +3 points for hitting a wanted player
			points = 3;
		}
		else if (gametyperules & GTR_BUMPERS)
		{
			if ((victim->bumpers > 0) && (victim->bumpers <= bumpersRemoved))
			{
				// +2 points for finishing off a player
				points = 2;
			}
		}
	}

	if (gametyperules & GTR_POINTLIMIT)
	{
		P_AddPlayerScore(player, points);
		K_SpawnBattlePoints(player, victim, points);
	}
}

void K_SpinPlayer(player_t *player, mobj_t *inflictor, mobj_t *source, INT32 type)
{
	(void)inflictor;
	(void)source;

	player->kartstuff[k_spinouttype] = type;

	if (( player->kartstuff[k_spinouttype] & KSPIN_THRUST ))
	{
		// At spinout, player speed is increased to 1/4 their regular speed, moving them forward
		if (player->speed < K_GetKartSpeed(player, true)/4)
			P_InstaThrust(player->mo, player->mo->angle, FixedMul(K_GetKartSpeed(player, true)/4, player->mo->scale));
		S_StartSound(player->mo, sfx_slip);
	}

	player->kartstuff[k_spinouttimer] = (3*TICRATE/2)+2;
	P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);
}

static void K_RemoveGrowShrink(player_t *player)
{
	if (player->mo && !P_MobjWasRemoved(player->mo))
	{
		if (player->kartstuff[k_growshrinktimer] > 0) // Play Shrink noise
			S_StartSound(player->mo, sfx_kc59);
		else if (player->kartstuff[k_growshrinktimer] < 0) // Play Grow noise
			S_StartSound(player->mo, sfx_kc5a);

		if (player->kartstuff[k_invincibilitytimer] == 0)
			player->mo->color = player->skincolor;

		player->mo->scalespeed = mapobjectscale/TICRATE;
		player->mo->destscale = mapobjectscale;
		if (cv_kartdebugshrink.value && !modeattacking && !player->bot)
			player->mo->destscale = (6*player->mo->destscale)/8;
	}

	player->kartstuff[k_growshrinktimer] = 0;

	P_RestoreMusic(player);
}

void K_SquishPlayer(player_t *player, mobj_t *inflictor, mobj_t *source)
{
	// PS: Inflictor is unused for all purposes here and is actually only ever relevant to Lua. It may be nil too.
	boolean force = false;	// Used to check if Lua ShouldSquish should get us damaged reguardless of flashtics or heck knows what.
	//UINT8 shouldForce = LUAh_ShouldSquish(player, inflictor, source);
	if (P_MobjWasRemoved(player->mo))
		return; // mobj was removed (in theory that shouldn't happen)
	//if (shouldForce == 1)
	//	force = true;
	//else if (shouldForce == 2)
	//	return;

	//if (player->health <= 0)
		//return;

	if (player->powers[pw_flashing] > 0 || player->kartstuff[k_squishedtimer] > 0 || player->kartstuff[k_invincibilitytimer] > 0
		|| player->kartstuff[k_growshrinktimer] > 0 || player->kartstuff[k_hyudorotimer] > 0
		|| (/*G_BattleGametype() &&*/ ((player->kartstuff[k_bumper] <= 0 && player->kartstuff[k_comebacktimer]) || player->kartstuff[k_comebackmode] == 1)))
	{
		if (!force)	// You know the drill by now.
		{
			K_DoInstashield(player);
			return;
		}
	}

	//if (LUAh_PlayerSquish(player, inflictor, source))	// Let Lua do its thing or overwrite if it wants to. Make sure to let any possible instashield happen because we didn't get "damaged" in this case.
	//	return;

	player->kartstuff[k_sneakertimer] = 0;
	player->kartstuff[k_driftboost] = 0;

	player->kartstuff[k_drift] = 0;
	player->kartstuff[k_driftcharge] = 0;
	player->kartstuff[k_pogospring] = 0;

	/*if (G_BattleGametype())
	{
		if (source && source->player && player != source->player)
		{
			P_AddPlayerScore(source->player, scoremultiply);
			K_SpawnBattlePoints(source->player, player, scoremultiply);
			source->player->kartstuff[k_wanted] -= wantedreduce;
			player->kartstuff[k_wanted] -= (wantedreduce/2);
		}

		if (player->kartstuff[k_bumper] > 0)
		{
			if (player->kartstuff[k_bumper] == 1)
			{
				mobj_t *karmahitbox = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_KARMAHITBOX); // Player hitbox is too small!!
				P_SetTarget(&karmahitbox->target, player->mo);
				karmahitbox->destscale = player->mo->scale;
				P_SetScale(karmahitbox, player->mo->scale);
				CONS_Printf(M_GetText("%s lost all of their bumpers!\n"), player_names[player-players]);
			}
			player->kartstuff[k_bumper]--;
			if (K_IsPlayerWanted(player))
				K_CalculateBattleWanted();
		}

		if (!player->kartstuff[k_bumper])
		{
			player->kartstuff[k_comebacktimer] = comebacktime;
			if (player->kartstuff[k_comebackmode] == 2)
			{
				mobj_t *poof = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_EXPLODE);
				S_StartSound(poof, mobjinfo[MT_KARMAHITBOX].seesound);
				player->kartstuff[k_comebackmode] = 0;
			}
		}

		K_CheckBumpers();
	}*/

	player->kartstuff[k_squishedtimer] = TICRATE;

	// Reduce Shrink timer
	if (player->kartstuff[k_growshrinktimer] < 0)
	{
		player->kartstuff[k_growshrinktimer] += TICRATE;
		if (player->kartstuff[k_growshrinktimer] >= 0)
			K_RemoveGrowShrink(player);
	}

	player->powers[pw_flashing] = K_GetKartFlashing(player);

	player->mo->flags |= MF_NOCLIP;

	if (player->mo->state != &states[S_KART_SPINOUT]) // Squash
		P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);

	//P_PlayRinglossSound(player->mo);
	P_PlayerRingBurst(player, 5);
	
	player->kartstuff[k_instashield] = 15;
	if (cv_kartdebughuddrop.value && !modeattacking)
		K_DropItems(player);
	else
		K_DropHnextList(player,false);
	return;
}

void K_ExplodePlayer(player_t *player, mobj_t *inflictor, mobj_t *source) // A bit of a hack, we just throw the player up higher here and extend their spinout timer
{
	(void)source;

	player->mo->momz = 18*mapobjectscale*P_MobjFlip(player->mo); // please stop forgetting mobjflip checks!!!!
	player->mo->momx = player->mo->momy = 0;

	player->kartstuff[k_spinouttype] = KSPIN_EXPLOSION;
	player->kartstuff[k_spinouttimer] = (3*TICRATE/2)+2;

	if (inflictor && !P_MobjWasRemoved(inflictor))
	{
		if (inflictor->type == MT_SPBEXPLOSION && inflictor->extravalue1)
		{
			player->kartstuff[k_spinouttimer] = ((5*player->kartstuff[k_spinouttimer])/2)+1;
			player->mo->momz *= 2;
		}
	}

	if (player->mo->eflags & MFE_UNDERWATER)
		player->mo->momz = (117 * player->mo->momz) / 200;

	P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);

	if (P_IsDisplayPlayer(player))
		P_StartQuake(64<<FRACBITS, 5);
}

// This kind of wipeout happens with no rings -- doesn't remove a bumper, has no invulnerability, and is much shorter.
void K_DebtStingPlayer(player_t *player, mobj_t *source)
{
	INT32 length = TICRATE;

	if (source->player)
	{
		length += (4 * (source->player->kartweight - player->kartweight));
	}

	player->kartstuff[k_spinouttype] = KSPIN_STUNG;
	player->kartstuff[k_spinouttimer] = length;
	player->kartstuff[k_wipeoutslow] = min(length-1, wipeoutslowtime+1);

	P_SetPlayerMobjState(player->mo, S_KART_SPINOUT);
}

void K_HandleBumperChanges(player_t *player, UINT8 prevBumpers)
{
	if (!(gametyperules & GTR_BUMPERS))
	{
		// Bumpers aren't being used
		return;
	}

	// TODO: replace all console text print-outs with a real visual

	if (player->bumpers > 0 && prevBumpers == 0)
	{
		if (player->kartstuff[k_comebackmode] == 2)
		{
			mobj_t *poof = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_EXPLODE);
			S_StartSound(poof, mobjinfo[MT_KARMAHITBOX].seesound);
		}

		player->kartstuff[k_comebackmode] = 0;

		if (netgame)
		{
			CONS_Printf(M_GetText("%s is back in the game!\n"), player_names[player-players]);
		}
	}
	else if (player->bumpers == 0 && prevBumpers > 0)
	{
		mobj_t *karmahitbox = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_KARMAHITBOX);
		P_SetTarget(&karmahitbox->target, player->mo);

		karmahitbox->destscale = player->mo->destscale;
		P_SetScale(karmahitbox, player->mo->scale);

		if (netgame)
		{
			CONS_Printf(M_GetText("%s lost all of their bumpers!\n"), player_names[player-players]);
		}
	}

	player->karmadelay = comebacktime;
	K_CalculateBattleWanted();
	K_CheckBumpers();
}

void K_DestroyBumpers(player_t *player, UINT8 amount)
{
	UINT8 oldBumpers = player->bumpers;

	if (!(gametyperules & GTR_BUMPERS))
	{
		return;
	}

	amount = min(amount, player->bumpers);

	if (amount == 0)
	{
		return;
	}

	player->bumpers -= amount;
	K_HandleBumperChanges(player, oldBumpers);
}

void K_TakeBumpersFromPlayer(player_t *player, player_t *victim, UINT8 amount)
{
	UINT8 oldPlayerBumpers = player->bumpers;
	UINT8 oldVictimBumpers = victim->bumpers;

	UINT8 tookBumpers = 0;

	if (!(gametyperules & GTR_BUMPERS))
	{
		return;
	}

	amount = min(amount, victim->bumpers);

	if (amount == 0)
	{
		return;
	}

	while ((tookBumpers < amount) && (victim->bumpers > 0))
	{
		UINT8 newbumper = player->bumpers;

		angle_t newangle, diff;
		fixed_t newx, newy;

		mobj_t *newmo;

		if (newbumper <= 1)
		{
			diff = 0;
		}
		else
		{
			diff = FixedAngle(360*FRACUNIT/newbumper);
		}

		newangle = player->mo->angle;
		newx = player->mo->x + P_ReturnThrustX(player->mo, newangle + ANGLE_180, 64*FRACUNIT);
		newy = player->mo->y + P_ReturnThrustY(player->mo, newangle + ANGLE_180, 64*FRACUNIT);

		newmo = P_SpawnMobj(newx, newy, player->mo->z, MT_BATTLEBUMPER);
		newmo->threshold = newbumper;

		P_SetTarget(&newmo->tracer, victim->mo);
		P_SetTarget(&newmo->target, player->mo);

		newmo->angle = (diff * (newbumper-1));
		newmo->color = victim->skincolor;

		if (newbumper+1 < 2)
		{
			P_SetMobjState(newmo, S_BATTLEBUMPER3);
		}
		else if (newbumper+1 < 3)
		{
			P_SetMobjState(newmo, S_BATTLEBUMPER2);
		}
		else
		{
			P_SetMobjState(newmo, S_BATTLEBUMPER1);
		}

		player->bumpers++;
		victim->bumpers--;
		tookBumpers++;
	}

	if (tookBumpers == 0)
	{
		// No change occured.
		return;
	}

	// Play steal sound
	S_StartSound(player->mo, sfx_3db06);

	K_HandleBumperChanges(player, oldPlayerBumpers);
	K_HandleBumperChanges(victim, oldVictimBumpers);
}

// source is the mobj that originally threw the bomb that exploded etc.
// Spawns the sphere around the explosion that handles spinout
void K_SpawnKartExplosion(fixed_t x, fixed_t y, fixed_t z, fixed_t radius, INT32 number, mobjtype_t type, angle_t rotangle, boolean spawncenter, boolean ghostit, mobj_t *source)
{
	mobj_t *mobj;
	mobj_t *ghost = NULL;
	INT32 i;
	TVector v;
	TVector *res;
	fixed_t finalx, finaly, finalz, dist;
	//mobj_t hoopcenter;
	angle_t degrees, fa, closestangle;
	fixed_t mobjx, mobjy, mobjz;

	//hoopcenter.x = x;
	//hoopcenter.y = y;
	//hoopcenter.z = z;

	//hoopcenter.z = z - mobjinfo[type].height/2;

	degrees = FINEANGLES/number;

	closestangle = 0;

	// Create the hoop!
	for (i = 0; i < number; i++)
	{
		fa = (i*degrees);
		v[0] = FixedMul(FINECOSINE(fa),radius);
		v[1] = 0;
		v[2] = FixedMul(FINESINE(fa),radius);
		v[3] = FRACUNIT;

		res = VectorMatrixMultiply(v, *RotateXMatrix(rotangle));
		M_Memcpy(&v, res, sizeof (v));
		res = VectorMatrixMultiply(v, *RotateZMatrix(closestangle));
		M_Memcpy(&v, res, sizeof (v));

		finalx = x + v[0];
		finaly = y + v[1];
		finalz = z + v[2];

		mobj = P_SpawnMobj(finalx, finaly, finalz, type);

		mobj->z -= mobj->height>>1;

		// change angle
		mobj->angle = R_PointToAngle2(mobj->x, mobj->y, x, y);

		// change slope
		dist = P_AproxDistance(P_AproxDistance(x - mobj->x, y - mobj->y), z - mobj->z);

		if (dist < 1)
			dist = 1;

		mobjx = mobj->x;
		mobjy = mobj->y;
		mobjz = mobj->z;

		if (ghostit)
		{
			ghost = P_SpawnGhostMobj(mobj);
			P_SetMobjState(mobj, S_NULL);
			mobj = ghost;
		}

		if (spawncenter)
		{
			mobj->x = x;
			mobj->y = y;
			mobj->z = z;
		}

		mobj->momx = FixedMul(FixedDiv(mobjx - x, dist), FixedDiv(dist, 6*FRACUNIT));
		mobj->momy = FixedMul(FixedDiv(mobjy - y, dist), FixedDiv(dist, 6*FRACUNIT));
		mobj->momz = FixedMul(FixedDiv(mobjz - z, dist), FixedDiv(dist, 6*FRACUNIT));

		if (source && !P_MobjWasRemoved(source))
			P_SetTarget(&mobj->target, source);
	}
}

#define MINEQUAKEDIST 4096

// Spawns the purely visual explosion
void K_SpawnMineExplosion(mobj_t *source, UINT8 color)
{
	INT32 i, radius, height;
	mobj_t *smoldering = P_SpawnMobj(source->x, source->y, source->z, MT_SMOLDERING);
	mobj_t *dust;
	mobj_t *truc;
	INT32 speed, speed2;
	INT32 pnum;
	player_t *p;

	// check for potential display players near the source so we can have a sick earthquake / flashpal.
	for (pnum = 0; pnum < MAXPLAYERS; pnum++)
	{
		p = &players[pnum];

		if (!playeringame[pnum] || !P_IsDisplayPlayer(p))
			continue;

		if (R_PointToDist2(p->mo->x, p->mo->y, source->x, source->y) < mapobjectscale*MINEQUAKEDIST)
		{
			P_StartQuake(55<<FRACBITS, 12);
			if (!bombflashtimer && P_CheckSight(p->mo, source))
			{
				bombflashtimer = TICRATE*2;
				P_FlashPal(p, PAL_WHITE, 1);
			}
			break;	// we can break right now because quakes are global to all split players somehow.
		}
	}

	K_MatchGenericExtraFlags(smoldering, source);
	smoldering->tics = TICRATE*3;
	radius = source->radius>>FRACBITS;
	height = source->height>>FRACBITS;

	if (!color)
		color = SKINCOLOR_KETCHUP;

	for (i = 0; i < 32; i++)
	{
		dust = P_SpawnMobj(source->x, source->y, source->z, MT_SMOKE);
		P_SetMobjState(dust, S_OPAQUESMOKE1);
		dust->angle = (ANGLE_180/16) * i;
		P_SetScale(dust, source->scale);
		dust->destscale = source->scale*10;
		dust->scalespeed = source->scale/12;
		P_InstaThrust(dust, dust->angle, FixedMul(20*FRACUNIT, source->scale));

		truc = P_SpawnMobj(source->x + P_RandomRange(-radius, radius)*FRACUNIT,
			source->y + P_RandomRange(-radius, radius)*FRACUNIT,
			source->z + P_RandomRange(0, height)*FRACUNIT, MT_BOOMEXPLODE);
		K_MatchGenericExtraFlags(truc, source);
		P_SetScale(truc, source->scale);
		truc->destscale = source->scale*6;
		truc->scalespeed = source->scale/12;
		speed = FixedMul(10*FRACUNIT, source->scale)>>FRACBITS;
		truc->momx = P_RandomRange(-speed, speed)*FRACUNIT;
		truc->momy = P_RandomRange(-speed, speed)*FRACUNIT;
		speed = FixedMul(20*FRACUNIT, source->scale)>>FRACBITS;
		truc->momz = P_RandomRange(-speed, speed)*FRACUNIT*P_MobjFlip(truc);
		if (truc->eflags & MFE_UNDERWATER)
			truc->momz = (117 * truc->momz) / 200;
		truc->color = color;
	}

	for (i = 0; i < 16; i++)
	{
		dust = P_SpawnMobj(source->x + P_RandomRange(-radius, radius)*FRACUNIT,
			source->y + P_RandomRange(-radius, radius)*FRACUNIT,
			source->z + P_RandomRange(0, height)*FRACUNIT, MT_SMOKE);
		P_SetMobjState(dust, S_OPAQUESMOKE1);
		P_SetScale(dust, source->scale);
		dust->destscale = source->scale*10;
		dust->scalespeed = source->scale/12;
		dust->tics = 30;
		dust->momz = P_RandomRange(FixedMul(3*FRACUNIT, source->scale)>>FRACBITS, FixedMul(7*FRACUNIT, source->scale)>>FRACBITS)*FRACUNIT;

		truc = P_SpawnMobj(source->x + P_RandomRange(-radius, radius)*FRACUNIT,
			source->y + P_RandomRange(-radius, radius)*FRACUNIT,
			source->z + P_RandomRange(0, height)*FRACUNIT, MT_BOOMPARTICLE);
		K_MatchGenericExtraFlags(truc, source);
		P_SetScale(truc, source->scale);
		truc->destscale = source->scale*5;
		truc->scalespeed = source->scale/12;
		speed = FixedMul(20*FRACUNIT, source->scale)>>FRACBITS;
		truc->momx = P_RandomRange(-speed, speed)*FRACUNIT;
		truc->momy = P_RandomRange(-speed, speed)*FRACUNIT;
		speed = FixedMul(15*FRACUNIT, source->scale)>>FRACBITS;
		speed2 = FixedMul(45*FRACUNIT, source->scale)>>FRACBITS;
		truc->momz = P_RandomRange(speed, speed2)*FRACUNIT*P_MobjFlip(truc);
		if (P_RandomChance(FRACUNIT/2))
			truc->momz = -truc->momz;
		if (truc->eflags & MFE_UNDERWATER)
			truc->momz = (117 * truc->momz) / 200;
		truc->tics = TICRATE*2;
		truc->color = color;
	}
}

#undef MINEQUAKEDIST

static mobj_t *K_SpawnKartMissile(mobj_t *source, mobjtype_t type, angle_t an, INT32 flags2, fixed_t speed)
{
	mobj_t *th;
	fixed_t x, y, z;
	fixed_t finalspeed = speed;
	mobj_t *throwmo;

	if (source->player && source->player->speed > K_GetKartSpeed(source->player, false))
	{
		angle_t input = source->angle - an;
		boolean invert = (input > ANGLE_180);
		if (invert)
			input = InvAngle(input);

		finalspeed = max(speed, FixedMul(speed, FixedMul(
			FixedDiv(source->player->speed, K_GetKartSpeed(source->player, false)), // Multiply speed to be proportional to your own, boosted maxspeed.
			(((180<<FRACBITS) - AngleFixed(input)) / 180) // multiply speed based on angle diff... i.e: don't do this for firing backward :V
			)));
	}

	x = source->x + source->momx + FixedMul(finalspeed, FINECOSINE(an>>ANGLETOFINESHIFT));
	y = source->y + source->momy + FixedMul(finalspeed, FINESINE(an>>ANGLETOFINESHIFT));
	z = source->z; // spawn on the ground please

	th = P_SpawnMobj(x, y, z, type);

	K_FlipFromObject(th, source);

	th->flags2 |= flags2;
	th->threshold = 10;

	if (th->info->seesound)
		S_StartSound(source, th->info->seesound);

	P_SetTarget(&th->target, source);

	P_SetScale(th, source->scale);
	th->destscale = source->destscale;

	if (P_IsObjectOnGround(source))
	{
		// floorz and ceilingz aren't properly set to account for FOFs and Polyobjects on spawn
		// This should set it for FOFs
		P_TeleportMove(th, th->x, th->y, th->z);
		// spawn on the ground if the player is on the ground
		if (P_MobjFlip(source) < 0)
		{
			th->z = th->ceilingz - th->height;
			th->eflags |= MFE_VERTICALFLIP;
		}
		else
			th->z = th->floorz;
	}

	th->angle = an;

	th->momx = FixedMul(finalspeed, FINECOSINE(an>>ANGLETOFINESHIFT));
	th->momy = FixedMul(finalspeed, FINESINE(an>>ANGLETOFINESHIFT));
	th->momz = source->momz;

	switch (type)
	{
		case MT_ORBINAUT:
			if (source && source->player)
				th->color = source->player->skincolor;
			else
				th->color = SKINCOLOR_GREY;
			th->movefactor = finalspeed;
			break;
		case MT_JAWZ:
			if (source && source->player)
			{
				INT32 lasttarg = source->player->kartstuff[k_lastjawztarget];
				th->cvmem = source->player->skincolor;
				if ((lasttarg >= 0 && lasttarg < MAXPLAYERS)
					&& playeringame[lasttarg]
					&& !players[lasttarg].spectator
					&& players[lasttarg].mo)
				{
					P_SetTarget(&th->tracer, players[lasttarg].mo);
				}
			}
			else
				th->cvmem = SKINCOLOR_KETCHUP;
			/* FALLTHRU */
		case MT_JAWZ_DUD:
			S_StartSound(th, th->info->activesound);
			/* FALLTHRU */
		case MT_SPB:
			th->movefactor = finalspeed;
			break;
		case MT_BUBBLESHIELDTRAP:
			P_SetScale(th, ((5*th->destscale)>>2)*4);
			th->destscale = (5*th->destscale)>>2;
			S_StartSound(th, sfx_s3kbfl);
			S_StartSound(th, sfx_cdfm35);
			break;
		default:
			break;
	}

	if (type != MT_BUBBLESHIELDTRAP)
	{
		x = x + P_ReturnThrustX(source, an, source->radius + th->radius);
		y = y + P_ReturnThrustY(source, an, source->radius + th->radius);
		throwmo = P_SpawnMobj(x, y, z, MT_FIREDITEM);
		throwmo->movecount = 1;
		throwmo->movedir = source->angle - an;
		P_SetTarget(&throwmo->target, source);
	}

	return NULL;
}

UINT16 K_DriftSparkColor(player_t *player, INT32 charge)
{
	INT32 ds = K_GetKartDriftSparkValue(player);
	UINT16 color = SKINCOLOR_NONE;

	if (charge < 0)
	{
		// Stage 0: Yellow
		color = SKINCOLOR_GOLD;
	}
	else if (charge >= ds*4)
	{
		// Stage 3: Rainbow
		if (charge <= (ds*4)+(32*3))
		{
			// transition
			color = SKINCOLOR_SILVER;
		}
		else
		{
			color = K_RainbowColor(leveltime);
		}
	}
	else if (charge >= ds*2)
	{
		// Stage 2: Blue
		if (charge <= (ds*2)+(32*3))
		{
			// transition
			color = SKINCOLOR_PURPLE;
		}
		else
		{
			color = SKINCOLOR_SAPPHIRE;
		}
	}
	else if (charge >= ds)
	{
		// Stage 1: Red
		if (charge <= (ds)+(32*3))
		{
			// transition
			color = SKINCOLOR_TANGERINE;
		}
		else
		{
			color = SKINCOLOR_KETCHUP;
		}
	}

	return color;
}

static void K_SpawnDriftSparks(player_t *player)
{
	INT32 ds = K_GetKartDriftSparkValue(player);
	fixed_t newx;
	fixed_t newy;
	mobj_t *spark;
	angle_t travelangle;
	INT32 i;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	if (leveltime % 2 == 1)
		return;

	if (!player->kartstuff[k_drift]
		|| (player->kartstuff[k_driftcharge] < ds && !(player->kartstuff[k_driftcharge] < 0)))
		return;

	travelangle = player->mo->angle-(ANGLE_45/5)*player->kartstuff[k_drift];

	for (i = 0; i < 2; i++)
	{
		SINT8 size = 1;
		UINT8 trail = 0;

		newx = player->mo->x + P_ReturnThrustX(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(32*FRACUNIT, player->mo->scale));
		newy = player->mo->y + P_ReturnThrustY(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(32*FRACUNIT, player->mo->scale));
		spark = P_SpawnMobj(newx, newy, player->mo->z, MT_DRIFTSPARK);

		P_SetTarget(&spark->target, player->mo);
		spark->angle = travelangle-(ANGLE_45/5)*player->kartstuff[k_drift];
		spark->destscale = player->mo->scale;
		P_SetScale(spark, player->mo->scale);

		spark->momx = player->mo->momx/2;
		spark->momy = player->mo->momy/2;
		//spark->momz = player->mo->momz/2;

		spark->color = K_DriftSparkColor(player, player->kartstuff[k_driftcharge]);

		if (player->kartstuff[k_driftcharge] < 0)
		{
			// Stage 0: Yellow
			size = 0;
		}
		else if (player->kartstuff[k_driftcharge] >= ds*4)
		{
			// Stage 3: Rainbow
			size = 2;
			trail = 2;

			if (player->kartstuff[k_driftcharge] <= (ds*4)+(32*3))
			{
				// transition
				P_SetScale(spark, (spark->destscale = spark->scale*3/2));
				S_StartSound(player->mo, sfx_cock);
			}
			else
			{
				spark->colorized = true;
			}
		}
		else if (player->kartstuff[k_driftcharge] >= ds*2)
		{
			// Stage 2: Blue
			size = 2;
			trail = 1;

			if (player->kartstuff[k_driftcharge] <= (ds*2)+(32*3))
			{
				// transition
				P_SetScale(spark, (spark->destscale = spark->scale*3/2));
			}
		}
		else
		{
			// Stage 1: Red
			size = 1;

			if (player->kartstuff[k_driftcharge] <= (ds)+(32*3))
			{
				// transition
				P_SetScale(spark, (spark->destscale = spark->scale*2));
			}
		}

		if ((player->kartstuff[k_drift] > 0 && player->cmd.turning > 0) // Inward drifts
			|| (player->kartstuff[k_drift] < 0 && player->cmd.turning < 0))
		{
			if ((player->kartstuff[k_drift] < 0 && (i & 1))
				|| (player->kartstuff[k_drift] > 0 && !(i & 1)))
			{
				size++;
			}
			else if ((player->kartstuff[k_drift] < 0 && !(i & 1))
				|| (player->kartstuff[k_drift] > 0 && (i & 1)))
			{
				size--;
			}
		}
		else if ((player->kartstuff[k_drift] > 0 && player->cmd.turning < 0) // Outward drifts
			|| (player->kartstuff[k_drift] < 0 && player->cmd.turning > 0))
		{
			if ((player->kartstuff[k_drift] < 0 && (i & 1))
				|| (player->kartstuff[k_drift] > 0 && !(i & 1)))
			{
				size--;
			}
			else if ((player->kartstuff[k_drift] < 0 && !(i & 1))
				|| (player->kartstuff[k_drift] > 0 && (i & 1)))
			{
				size++;
			}
		}

		if (size == 2)
			P_SetMobjState(spark, S_DRIFTSPARK_A1);
		else if (size < 1)
			P_SetMobjState(spark, S_DRIFTSPARK_C1);
		else if (size > 2)
			P_SetMobjState(spark, S_DRIFTSPARK_D1);

		if (trail > 0)
			spark->tics += trail;

		K_MatchGenericExtraFlags(spark, player->mo);
	}
}

static void K_SpawnAIZDust(player_t *player)
{
	fixed_t newx;
	fixed_t newy;
	mobj_t *spark;
	angle_t travelangle;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	if (leveltime % 2 == 1)
		return;

	if (!P_IsObjectOnGround(player->mo))
		return;

	if (player->speed <= K_GetKartSpeed(player, false))
		return;

	travelangle = K_MomentumAngle(player->mo);
	//S_StartSound(player->mo, sfx_s3k47);

	{
		newx = player->mo->x + P_ReturnThrustX(player->mo, travelangle - (player->kartstuff[k_aizdriftstrat]*ANGLE_45), FixedMul(24*FRACUNIT, player->mo->scale));
		newy = player->mo->y + P_ReturnThrustY(player->mo, travelangle - (player->kartstuff[k_aizdriftstrat]*ANGLE_45), FixedMul(24*FRACUNIT, player->mo->scale));
		spark = P_SpawnMobj(newx, newy, player->mo->z, MT_AIZDRIFTSTRAT);

		spark->angle = travelangle+(player->kartstuff[k_aizdriftstrat]*ANGLE_90);
		P_SetScale(spark, (spark->destscale = (3*player->mo->scale)>>2));

		spark->momx = (6*player->mo->momx)/5;
		spark->momy = (6*player->mo->momy)/5;
		//spark->momz = player->mo->momz/2;

		K_MatchGenericExtraFlags(spark, player->mo);
	}
}

static void K_QuiteSaltyHop(player_t *p) 
{
	// what the fuck is this haya
	fixed_t mos = FRACUNIT; // doesnt work correctly if it isnt :/

	// ready?
	if (!p->kartstuff[k_jmp]) {
		p->mo->salty_ready = true;
		p->mo->salty_tapping = false;
	} else if (p->mo->salty_ready) {
		p->mo->salty_ready = false;
		p->mo->salty_tapping = true;
	} else {
		p->mo->salty_tapping = false;
	}

	// GO!
	if (!p->mo->init_salty) {
		p->mo->salty_jump = false;
		p->mo->salty_zoffset = 0;
		p->mo->salty_momz = 0;
		p->mo->init_salty = true;
	}	
	else if ((p->mo->salty_jump)) {
		if (p->mo->eflags & MFE_JUSTHITFLOOR) {
			p->mo->salty_zoffset = 0;
		} else if (P_IsObjectOnGround(p->mo)) {
			p->mo->salty_zoffset += p->mo->salty_momz;
			p->mo->salty_momz -= (mos*3/2);
		} else {
			p->mo->salty_zoffset *= (49/50)*mos;
			p->mo->salty_momz = 0;
		}
		if (p->mo->salty_zoffset <= 0) {
			if (!(p->mo->eflags & MFE_JUSTHITFLOOR) && P_IsObjectOnGround(p->mo) && cv_saltyhopsfx.value)
				S_StartSound(p->mo, sfx_s268);
			p->mo->salty_jump = false;
			p->mo->salty_zoffset = 0;
			p->mo->salty_momz = 0;
			// shlamma damma
			//p->mo->stretchslam += cv_saltysquish.value ? (8*mos) : 0;
		} //else if (p->mo->salty_zoffset >= 0 && cv_saltysquish.value) {
			// goofy ahh hack
			//p->mo->spriteyscale += (mos/8);
			//p->mo->spritexscale -= (mos/8);
		//}
		p->mo->sprzoff = p->mo->salty_zoffset;
		if (S_SoundPlaying(p->mo, sfx_screec))
			S_StopSoundByID(p->mo, sfx_screec);
		if (S_SoundPlaying(p->mo, sfx_drift))
			S_StopSoundByID(p->mo, sfx_drift);
	}
	else if (p->mo->salty_tapping && P_IsObjectOnGround(p->mo) && !p->kartstuff[k_spinouttimer] && !p->kartstuff[k_squishedtimer]) {
		p->mo->salty_jump = true;
		p->mo->salty_zoffset = 0;
		p->mo->salty_momz = 6*mos;
		if (cv_saltyhopsfx.value) 
			S_StartSound(p->mo, sfx_s25a);
	}
}

void K_SpawnBoostTrail(player_t *player)
{
	fixed_t newx, newy, newz;
	fixed_t ground;
	mobj_t *flame;
	angle_t travelangle;
	INT32 i;

	I_Assert(player != NULL);
	I_Assert(player->mo != NULL);
	I_Assert(!P_MobjWasRemoved(player->mo));

	if (!P_IsObjectOnGround(player->mo)
		|| player->kartstuff[k_hyudorotimer] != 0
		|| ((gametyperules & GTR_BUMPERS) && player->bumpers <= 0 && player->karmadelay))
		return;

	if (player->mo->eflags & MFE_VERTICALFLIP)
		ground = player->mo->ceilingz;
	else
		ground = player->mo->floorz;

	if (player->kartstuff[k_drift] != 0)
		travelangle = player->mo->angle;
	else
		travelangle = K_MomentumAngle(player->mo);

	for (i = 0; i < 2; i++)
	{
		newx = player->mo->x + P_ReturnThrustX(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(24*FRACUNIT, player->mo->scale));
		newy = player->mo->y + P_ReturnThrustY(player->mo, travelangle + ((i&1) ? -1 : 1)*ANGLE_135, FixedMul(24*FRACUNIT, player->mo->scale));
		newz = P_GetZAt(player->mo->standingslope, newx, newy, ground);

		if (player->mo->eflags & MFE_VERTICALFLIP)
		{
			newz -= FixedMul(mobjinfo[MT_SNEAKERTRAIL].height, player->mo->scale);
		}

		flame = P_SpawnMobj(newx, newy, ground, MT_SNEAKERTRAIL);

		P_SetTarget(&flame->target, player->mo);
		flame->angle = travelangle;
		flame->fuse = TICRATE*2;
		flame->destscale = player->mo->scale;
		P_SetScale(flame, player->mo->scale);
		// not K_MatchGenericExtraFlags so that a stolen sneaker can be seen
		K_FlipFromObject(flame, player->mo);

		flame->momx = 8;
		P_XYMovement(flame);
		if (P_MobjWasRemoved(flame))
			continue;

		if (player->mo->eflags & MFE_VERTICALFLIP)
		{
			if (flame->z + flame->height < flame->ceilingz)
				P_RemoveMobj(flame);
		}
		else if (flame->z > flame->floorz)
			P_RemoveMobj(flame);
	}
}

void K_SpawnSparkleTrail(mobj_t *mo)
{
	const INT32 rad = (mo->radius*2)>>FRACBITS;
	mobj_t *sparkle;
	INT32 i;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	for (i = 0; i < 3; i++)
	{
		fixed_t newx = mo->x + mo->momx + (P_RandomRange(-rad, rad)<<FRACBITS);
		fixed_t newy = mo->y + mo->momy + (P_RandomRange(-rad, rad)<<FRACBITS);
		fixed_t newz = mo->z + mo->momz + (P_RandomRange(0, mo->height>>FRACBITS)<<FRACBITS);

		sparkle = P_SpawnMobj(newx, newy, newz, MT_SPARKLETRAIL);
		K_FlipFromObject(sparkle, mo);

		//if (i == 0)
			//P_SetMobjState(sparkle, S_KARTINVULN_LARGE1);

		P_SetTarget(&sparkle->target, mo);
		sparkle->destscale = mo->destscale;
		P_SetScale(sparkle, mo->scale);
		sparkle->color = mo->color;
		//sparkle->colorized = mo->colorized;
	}

	P_SetMobjState(sparkle, S_KARTINVULN_LARGE1);
}

void K_SpawnWipeoutTrail(mobj_t *mo, boolean translucent)
{
	mobj_t *dust;
	angle_t aoff;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	if (mo->player)
		aoff = (mo->player->drawangle + ANGLE_180);
	else
		aoff = (mo->angle + ANGLE_180);

	if ((leveltime / 2) & 1)
		aoff -= ANGLE_45;
	else
		aoff += ANGLE_45;

	dust = P_SpawnMobj(mo->x + FixedMul(24*mo->scale, FINECOSINE(aoff>>ANGLETOFINESHIFT)) + (P_RandomRange(-8,8) << FRACBITS),
		mo->y + FixedMul(24*mo->scale, FINESINE(aoff>>ANGLETOFINESHIFT)) + (P_RandomRange(-8,8) << FRACBITS),
		mo->z, MT_WIPEOUTTRAIL);

	P_SetTarget(&dust->target, mo);
	dust->angle = K_MomentumAngle(mo);
	dust->destscale = mo->scale;
	P_SetScale(dust, mo->scale);
	K_FlipFromObject(dust, mo);

	if (translucent) // offroad effect
	{
		dust->momx = mo->momx/2;
		dust->momy = mo->momy/2;
		dust->momz = mo->momz/2;
	}

	if (translucent)
		dust->drawflags |= MFD_SHADOW;
}

void K_SpawnDraftDust(mobj_t *mo)
{
	UINT8 i;

	I_Assert(mo != NULL);
	I_Assert(!P_MobjWasRemoved(mo));

	for (i = 0; i < 2; i++)
	{
		angle_t ang, aoff;
		SINT8 sign = 1;
		UINT8 foff = 0;
		mobj_t *dust;
		boolean drifting = false;

		if (mo->player)
		{
			UINT8 leniency = (3*TICRATE)/4 + ((mo->player->kartweight-1) * (TICRATE/4));

			ang = mo->player->drawangle;

			if (mo->player->kartstuff[k_drift] != 0)
			{
				drifting = true;
				ang += (mo->player->kartstuff[k_drift] * ((ANGLE_270 + ANGLE_22h) / 5)); // -112.5 doesn't work. I fucking HATE SRB2 angles
				if (mo->player->kartstuff[k_drift] < 0)
					sign = 1;
				else
					sign = -1;
			}

			foff = 5 - ((mo->player->kartstuff[k_draftleeway] * 5) / leniency);

			// this shouldn't happen
			if (foff > 4)
				foff = 4;
		}
		else
			ang = mo->angle;

		if (!drifting)
		{
			if (i & 1)
				sign = -1;
			else
				sign = 1;
		}

		aoff = (ang + ANGLE_180) + (ANGLE_45 * sign);

		dust = P_SpawnMobj(mo->x + FixedMul(24*mo->scale, FINECOSINE(aoff>>ANGLETOFINESHIFT)),
			mo->y + FixedMul(24*mo->scale, FINESINE(aoff>>ANGLETOFINESHIFT)),
			mo->z, MT_DRAFTDUST);

		P_SetMobjState(dust, S_DRAFTDUST1 + foff);

		P_SetTarget(&dust->target, mo);
		dust->angle = ang - (ANGLE_90 * sign); // point completely perpendicular from the player
		dust->destscale = mo->scale;
		P_SetScale(dust, mo->scale);
		K_FlipFromObject(dust, mo);

		if (leveltime & 1)
			dust->tics++; // "randomize" animation

		dust->momx = (4*mo->momx)/5;
		dust->momy = (4*mo->momy)/5;
		//dust->momz = (4*mo->momz)/5;

		P_Thrust(dust, dust->angle, 4*mo->scale);

		if (drifting) // only 1 trail while drifting
			break;
	}
}

//	K_DriftDustHandling
//	Parameters:
//		spawner: The map object that is spawning the drift dust
//	Description: Spawns the drift dust for objects, players use rmomx/y, other objects use regular momx/y.
//		Also plays the drift sound.
//		Other objects should be angled towards where they're trying to go so they don't randomly spawn dust
//		Do note that most of the function won't run in odd intervals of frames
void K_DriftDustHandling(mobj_t *spawner)
{
	angle_t anglediff;
	const INT16 spawnrange = spawner->radius >> FRACBITS;

	if (!P_IsObjectOnGround(spawner) || leveltime % 2 != 0)
		return;

	if (spawner->player)
	{
		if (spawner->player->pflags & PF_FAULT)
		{
			anglediff = abs((signed)(spawner->angle - spawner->player->drawangle));
			if (leveltime % 6 == 0)
				S_StartSound(spawner, sfx_screec); // repeated here because it doesn't always happen to be within the range when this is the case
		}
		else
		{
			angle_t playerangle = spawner->angle;

			if (spawner->player->speed < 5*spawner->scale)
				return;

			if (K_GetForwardMove(spawner->player) < 0)
				playerangle += ANGLE_180;

			anglediff = abs((signed)(playerangle - R_PointToAngle2(0, 0, spawner->player->rmomx, spawner->player->rmomy)));
		}
	}
	else
	{
		if (P_AproxDistance(spawner->momx, spawner->momy) < 5*spawner->scale)
			return;

		anglediff = abs((signed)(spawner->angle - K_MomentumAngle(spawner)));
	}

	if (anglediff > ANGLE_180)
		anglediff = InvAngle(anglediff);

	if (anglediff > ANG10*4) // Trying to turn further than 40 degrees
	{
		fixed_t spawnx = P_RandomRange(-spawnrange, spawnrange) << FRACBITS;
		fixed_t spawny = P_RandomRange(-spawnrange, spawnrange) << FRACBITS;
		INT32 speedrange = 2;
		mobj_t *dust = P_SpawnMobj(spawner->x + spawnx, spawner->y + spawny, spawner->z, MT_DRIFTDUST);
		dust->momx = FixedMul(spawner->momx + (P_RandomRange(-speedrange, speedrange) * spawner->scale), 3*FRACUNIT/4);
		dust->momy = FixedMul(spawner->momy + (P_RandomRange(-speedrange, speedrange) * spawner->scale), 3*FRACUNIT/4);
		dust->momz = P_MobjFlip(spawner) * (P_RandomRange(1, 4) * (spawner->scale));
		P_SetScale(dust, spawner->scale/2);
		dust->destscale = spawner->scale * 3;
		dust->scalespeed = spawner->scale/12;

		if (leveltime % 6 == 0)
			S_StartSound(spawner, sfx_screec);

		K_MatchGenericExtraFlags(dust, spawner);

		// Sparkle-y warning for when you're about to change drift sparks!
		if (spawner->player && spawner->player->kartstuff[k_drift])
		{
			INT32 driftval = K_GetKartDriftSparkValue(spawner->player);
			INT32 warntime = driftval/3;
			INT32 dc = spawner->player->kartstuff[k_driftcharge];
			UINT8 c = SKINCOLOR_NONE;
			boolean rainbow = false;

			if (dc >= 0)
			{
				dc += warntime;
			}

			c = K_DriftSparkColor(spawner->player, dc);

			if (dc > (4*driftval)+(32*3))
			{
				rainbow = true;
			}

			if (c != SKINCOLOR_NONE)
			{
				P_SetMobjState(dust, S_DRIFTWARNSPARK1);
				dust->color = c;
				dust->colorized = rainbow;
			}
		}
	}
}

static mobj_t *K_FindLastTrailMobj(player_t *player)
{
	mobj_t *trail;

	if (!player || !(trail = player->mo) || !player->mo->hnext || !player->mo->hnext->health)
		return NULL;

	while (trail->hnext && !P_MobjWasRemoved(trail->hnext) && trail->hnext->health)
	{
		trail = trail->hnext;
	}

	return trail;
}

static mobj_t *K_ThrowKartItem(player_t *player, boolean missile, mobjtype_t mapthing, INT32 defaultDir, INT32 altthrow)
{
	mobj_t *mo;
	INT32 dir;
	fixed_t PROJSPEED;
	angle_t newangle;
	fixed_t newx, newy, newz;
	mobj_t *throwmo;

	if (!player)
		return NULL;

	// Figure out projectile speed by game speed
	if (missile)
	{
		// Use info->speed for missiles
		PROJSPEED = FixedMul(mobjinfo[mapthing].speed, K_GetKartGameSpeedScalar(gamespeed));
	}
	else
	{
		// Use pre-determined speed for tossing
		PROJSPEED = FixedMul(82 * FRACUNIT, K_GetKartGameSpeedScalar(gamespeed));
	}

	// Scale to map scale
	// Intentionally NOT player scale, that doesn't work.
	PROJSPEED = FixedMul(PROJSPEED, mapobjectscale);

	if (altthrow)
	{
		if (altthrow == 2) // Kitchen sink throwing
		{
#if 0
			if (player->kartstuff[k_throwdir] == 1)
				dir = 3;
			else if (player->kartstuff[k_throwdir] == -1)
				dir = 1;
			else
				dir = 2;
#else
			if (player->kartstuff[k_throwdir] == 1)
				dir = 2;
			else
				dir = 1;
#endif
		}
		else
		{
			if (player->kartstuff[k_throwdir] == 1)
				dir = 2;
			else if (player->kartstuff[k_throwdir] == -1)
				dir = -1;
			else
				dir = 1;
		}
	}
	else
	{
		if (player->kartstuff[k_throwdir] != 0)
			dir = player->kartstuff[k_throwdir];
		else
			dir = defaultDir;
	}

	if (missile) // Shootables
	{
		if (mapthing == MT_BALLHOG) // Messy
		{
			if (dir == -1)
			{
				// Shoot backward
				mo = K_SpawnKartMissile(player->mo, mapthing, (player->mo->angle + ANGLE_180) - 0x06000000, 0, PROJSPEED/8);
				K_SpawnKartMissile(player->mo, mapthing, (player->mo->angle + ANGLE_180) - 0x03000000, 0, PROJSPEED/8);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + ANGLE_180, 0, PROJSPEED/8);
				K_SpawnKartMissile(player->mo, mapthing, (player->mo->angle + ANGLE_180) + 0x03000000, 0, PROJSPEED/8);
				K_SpawnKartMissile(player->mo, mapthing, (player->mo->angle + ANGLE_180) + 0x06000000, 0, PROJSPEED/8);
			}
			else
			{
				// Shoot forward
				mo = K_SpawnKartMissile(player->mo, mapthing, player->mo->angle - 0x06000000, 0, PROJSPEED);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle - 0x03000000, 0, PROJSPEED);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle, 0, PROJSPEED);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + 0x03000000, 0, PROJSPEED);
				K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + 0x06000000, 0, PROJSPEED);
			}
		}
		else
		{
			if (dir == -1 && mapthing != MT_SPB)
			{
				// Shoot backward
				mo = K_SpawnKartMissile(player->mo, mapthing, player->mo->angle + ANGLE_180, 0, PROJSPEED/8);
			}
			else
			{
				// Shoot forward
				mo = K_SpawnKartMissile(player->mo, mapthing, player->mo->angle, 0, PROJSPEED);
			}
		}
	}
	else
	{
		player->kartstuff[k_bananadrag] = 0; // RESET timer, for multiple bananas

		if (dir > 0)
		{
			// Shoot forward
			mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z + player->mo->height/2, mapthing);
			//K_FlipFromObject(mo, player->mo);
			// These are really weird so let's make it a very specific case to make SURE it works...
			if (player->mo->eflags & MFE_VERTICALFLIP)
			{
				mo->z -= player->mo->height;
				mo->eflags |= MFE_VERTICALFLIP;
				mo->flags2 |= (player->mo->flags2 & MF2_OBJECTFLIP);
			}

			mo->threshold = 10;
			P_SetTarget(&mo->target, player->mo);

			S_StartSound(player->mo, mo->info->seesound);

			if (mo)
			{
				angle_t fa = player->mo->angle>>ANGLETOFINESHIFT;
				fixed_t HEIGHT = ((20 + (dir*10)) * FRACUNIT) + (player->mo->momz*P_MobjFlip(player->mo)); // Also intentionally not player scale

				P_SetObjectMomZ(mo, HEIGHT, false);
				mo->momx = player->mo->momx + FixedMul(FINECOSINE(fa), PROJSPEED*dir);
				mo->momy = player->mo->momy + FixedMul(FINESINE(fa), PROJSPEED*dir);

				mo->extravalue2 = dir;

				if (mo->eflags & MFE_UNDERWATER)
					mo->momz = (117 * mo->momz) / 200;
			}

			// this is the small graphic effect that plops in you when you throw an item:
			throwmo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z + player->mo->height/2, MT_FIREDITEM);
			P_SetTarget(&throwmo->target, player->mo);
			// Ditto:
			if (player->mo->eflags & MFE_VERTICALFLIP)
			{
				throwmo->z -= player->mo->height;
				throwmo->eflags |= MFE_VERTICALFLIP;
				mo->flags2 |= (player->mo->flags2 & MF2_OBJECTFLIP);
			}

			throwmo->movecount = 0; // above player
		}
		else
		{
			mobj_t *lasttrail = K_FindLastTrailMobj(player);

			if (mapthing == MT_BUBBLESHIELDTRAP) // Drop directly on top of you.
			{
				newangle = player->mo->angle;
				newx = player->mo->x + player->mo->momx;
				newy = player->mo->y + player->mo->momy;
				newz = player->mo->z;
			}
			else if (lasttrail)
			{
				newangle = lasttrail->angle;
				newx = lasttrail->x;
				newy = lasttrail->y;
				newz = lasttrail->z;
			}
			else
			{
				// Drop it directly behind you.
				fixed_t dropradius = FixedHypot(player->mo->radius, player->mo->radius) + FixedHypot(mobjinfo[mapthing].radius, mobjinfo[mapthing].radius);

				newangle = player->mo->angle;

				newx = player->mo->x + P_ReturnThrustX(player->mo, newangle + ANGLE_180, dropradius);
				newy = player->mo->y + P_ReturnThrustY(player->mo, newangle + ANGLE_180, dropradius);
				newz = player->mo->z;
			}

			mo = P_SpawnMobj(newx, newy, newz, mapthing); // this will never return null because collision isn't processed here
			K_FlipFromObject(mo, player->mo);

			mo->threshold = 10;
			P_SetTarget(&mo->target, player->mo);

			P_SetScale(mo, player->mo->scale);
			mo->destscale = player->mo->destscale;

			if (P_IsObjectOnGround(player->mo))
			{
				// floorz and ceilingz aren't properly set to account for FOFs and Polyobjects on spawn
				// This should set it for FOFs
				P_TeleportMove(mo, mo->x, mo->y, mo->z); // however, THIS can fuck up your day. just absolutely ruin you.
				if (P_MobjWasRemoved(mo))
					return NULL;

				if (P_MobjFlip(mo) > 0)
				{
					if (mo->floorz > mo->target->z - mo->height)
					{
						mo->z = mo->floorz;
					}
				}
				else
				{
					if (mo->ceilingz < mo->target->z + mo->target->height + mo->height)
					{
						mo->z = mo->ceilingz - mo->height;
					}
				}
			}

			if (player->mo->eflags & MFE_VERTICALFLIP)
				mo->eflags |= MFE_VERTICALFLIP;

			if (mapthing == MT_SSMINE)
				mo->extravalue1 = 49; // Pads the start-up length from 21 frames to a full 2 seconds
			else if (mapthing == MT_BUBBLESHIELDTRAP)
			{
				P_SetScale(mo, ((5*mo->destscale)>>2)*4);
				mo->destscale = (5*mo->destscale)>>2;
				S_StartSound(mo, sfx_s3kbfl);
			}
		}
	}

	return mo;
}

void K_PuntMine(mobj_t *origMine, mobj_t *punter)
{
	angle_t fa = K_MomentumAngle(punter);
	fixed_t z = (punter->momz * P_MobjFlip(punter)) + (30 * FRACUNIT);
	fixed_t spd;
	mobj_t *mine;

	if (!origMine || P_MobjWasRemoved(origMine))
		return;
	
	// This guarantees you hit a mine being dragged
	if (origMine->type == MT_SSMINE_SHIELD) // Create a new mine, and clean up the old one
	{
		mobj_t *mineOwner = origMine->target;

		mine = P_SpawnMobj(origMine->x, origMine->y, origMine->z, MT_SSMINE);

		P_SetTarget(&mine->target, mineOwner);
		mine->angle = origMine->angle;
		mine->flags2 = origMine->flags2;
		mine->floorz = origMine->floorz;
		mine->ceilingz = origMine->ceilingz;

		// Since we aren't using P_KillMobj, we need to clean up the hnext reference
		P_SetTarget(&mineOwner->hnext, NULL);
		mineOwner->player->kartstuff[k_bananadrag] = 0;
		mineOwner->player->kartstuff[k_itemheld] = 0;

		if (--mineOwner->player->kartstuff[k_itemamount] <= 0)
		{
			mineOwner->player->kartstuff[k_itemtype] = KITEM_NONE;
		}

		P_RemoveMobj(origMine);
	}
	else
	{
		mine = origMine;
	}

	if (!mine || P_MobjWasRemoved(mine))
		return;

	if (mine->threshold > 0)
		return;

	spd = FixedMul(82 * punter->scale, K_GetKartGameSpeedScalar(gamespeed)); // Avg Speed is 41 in Normal

	mine->flags |= MF_NOCLIPTHING;

	P_SetMobjState(mine, S_SSMINE_AIR1);
	mine->threshold = 10;
	mine->reactiontime = mine->info->reactiontime;

	mine->momx = punter->momx + FixedMul(FINECOSINE(fa >> ANGLETOFINESHIFT), spd);
	mine->momy = punter->momy + FixedMul(FINESINE(fa >> ANGLETOFINESHIFT), spd);
	P_SetObjectMomZ(mine, z, false);

	mine->flags &= ~MF_NOCLIPTHING;
}

#define THUNDERRADIUS 320

static void K_DoThunderShield(player_t *player)
{
	mobj_t *mo;
	int i = 0;
	fixed_t sx;
	fixed_t sy;
	angle_t an;

	S_StartSound(player->mo, sfx_zio3);
	P_NukeEnemies(player->mo, player->mo, RING_DIST/4);

	// spawn vertical bolt
	mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_THOK);
	P_SetTarget(&mo->target, player->mo);
	P_SetMobjState(mo, S_LZIO11);
	mo->color = SKINCOLOR_TEAL;
	mo->scale = player->mo->scale*3 + (player->mo->scale/2);

	mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_THOK);
	P_SetTarget(&mo->target, player->mo);
	P_SetMobjState(mo, S_LZIO21);
	mo->color = SKINCOLOR_CYAN;
	mo->scale = player->mo->scale*3 + (player->mo->scale/2);

	// spawn horizontal bolts;
	for (i=0; i<7; i++)
	{
		mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_THOK);
		mo->angle = P_RandomRange(0, 359)*ANG1;
		mo->fuse = P_RandomRange(20, 50);
		P_SetTarget(&mo->target, player->mo);
		P_SetMobjState(mo, S_KLIT1);
	}

	// spawn the radius thing:
	an = ANGLE_22h;
	for (i=0; i<15; i++)
	{
		sx = player->mo->x + FixedMul((player->mo->scale*THUNDERRADIUS), FINECOSINE((an*i)>>ANGLETOFINESHIFT));
		sy = player->mo->y + FixedMul((player->mo->scale*THUNDERRADIUS), FINESINE((an*i)>>ANGLETOFINESHIFT));
		mo = P_SpawnMobj(sx, sy, player->mo->z, MT_THOK);
		mo-> angle = an*i;
		mo->extravalue1 = THUNDERRADIUS;	// Used to know whether we should teleport by radius or something.
		mo->scale = player->mo->scale*3;
		P_SetTarget(&mo->target, player->mo);
		P_SetMobjState(mo, S_KSPARK1);
	}
}

#undef THUNDERRADIUS

static void K_FlameDashLeftoverSmoke(mobj_t *src)
{
	UINT8 i;

	for (i = 0; i < 2; i++)
	{
		mobj_t *smoke = P_SpawnMobj(src->x, src->y, src->z+(8<<FRACBITS), MT_BOOSTSMOKE);

		P_SetScale(smoke, src->scale);
		smoke->destscale = 3*src->scale/2;
		smoke->scalespeed = src->scale/12;

		smoke->momx = 3*src->momx/4;
		smoke->momy = 3*src->momy/4;
		smoke->momz = 3*src->momz/4;

		P_Thrust(smoke, src->angle + FixedAngle(P_RandomRange(135, 225)<<FRACBITS), P_RandomRange(0, 8) * src->scale);
		smoke->momz += P_RandomRange(0, 4) * src->scale;
	}
}

static void K_DoHyudoroSteal(player_t *player)
{
	INT32 i, numplayers = 0;
	INT32 playerswappable[MAXPLAYERS];
	INT32 stealplayer = -1; // The player that's getting stolen from
	INT32 prandom = 0;
	boolean sink = P_RandomChance(FRACUNIT/64);
	INT32 hyu = hyudorotime;

	if (gametype == GT_RACE)
		hyu *= 2; // double in race

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].mo && players[i].mo->health > 0 && players[i].playerstate == PST_LIVE
			&& player != &players[i] && !players[i].exiting && !players[i].spectator // Player in-game

			// Can steal from this player
			&& (gametype == GT_RACE //&& players[i].kartstuff[k_position] < player->kartstuff[k_position])
			|| ((gametyperules & GTR_BUMPERS) && players[i].bumpers > 0))

			// Has an item
			&& (players[i].kartstuff[k_itemtype]
			&& players[i].kartstuff[k_itemamount]
			&& !players[i].kartstuff[k_itemheld]
			&& !players[i].karthud[khud_itemblink]))
		{
			playerswappable[numplayers] = i;
			numplayers++;
		}
	}

	prandom = P_RandomFixed();
	S_StartSound(player->mo, sfx_s3k92);

	if (sink && numplayers > 0 && cv_kitchensink.value) // BEHOLD THE KITCHEN SINK
	{
		player->kartstuff[k_hyudorotimer] = hyu;
		player->kartstuff[k_stealingtimer] = stealtime;

		player->kartstuff[k_itemtype] = KITEM_KITCHENSINK;
		player->kartstuff[k_itemamount] = 1;
		player->kartstuff[k_itemheld] = 0;
		return;
	}
	else if ((gametype == GT_RACE && player->kartstuff[k_position] == 1) || numplayers == 0) // No-one can be stolen from? Oh well...
	{
		player->kartstuff[k_hyudorotimer] = hyu;
		player->kartstuff[k_stealingtimer] = stealtime;
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
		player->kartstuff[k_hyudorotimer] = hyu;
		player->kartstuff[k_stealingtimer] = stealtime;
		players[stealplayer].kartstuff[k_stolentimer] = stealtime;

		player->kartstuff[k_itemtype] = players[stealplayer].kartstuff[k_itemtype];
		player->kartstuff[k_itemamount] = players[stealplayer].kartstuff[k_itemamount];
		player->kartstuff[k_itemheld] = 0;

		players[stealplayer].kartstuff[k_itemtype] = KITEM_NONE;
		players[stealplayer].kartstuff[k_itemamount] = 0;
		players[stealplayer].kartstuff[k_itemheld] = 0;

		if (P_IsDisplayPlayer(&players[stealplayer]) && !r_splitscreen)
			S_StartSound(NULL, sfx_s3k92);
	}
}

void K_DoSneaker(player_t *player, INT32 type)
{
	const fixed_t intendedboost = FRACUNIT/2;

	if (!player->kartstuff[k_floorboost] || player->kartstuff[k_floorboost] == 3)
	{
		const sfxenum_t normalsfx = sfx_cdfm01;
		const sfxenum_t smallsfx = sfx_cdfm40;
		sfxenum_t sfx = normalsfx;

		if (player->kartstuff[k_numsneakers])
		{
			// Use a less annoying sound when stacking sneakers.
			sfx = smallsfx;
		}

		S_StopSoundByID(player->mo, normalsfx);
		S_StopSoundByID(player->mo, smallsfx);
		S_StartSound(player->mo, sfx);

		K_SpawnDashDustRelease(player);
		if (intendedboost > player->kartstuff[k_speedboost])
			player->karthud[khud_destboostcam] = FixedMul(FRACUNIT, FixedDiv((intendedboost - player->kartstuff[k_speedboost]), intendedboost));

		player->kartstuff[k_numsneakers]++;
	}

	if (!player->kartstuff[k_sneakertimer])
	{
		if (type == 2)
		{
			if (player->mo->hnext)
			{
				mobj_t *cur = player->mo->hnext;
				while (cur && !P_MobjWasRemoved(cur))
				{
					if (!cur->tracer)
					{
						mobj_t *overlay = P_SpawnMobj(cur->x, cur->y, cur->z, MT_BOOSTFLAME);
						P_SetTarget(&overlay->target, cur);
						P_SetTarget(&cur->tracer, overlay);
						P_SetScale(overlay, (overlay->destscale = 3*cur->scale/4));
						K_FlipFromObject(overlay, cur);
					}
					cur = cur->hnext;
				}
			}
		}
		else
		{
			mobj_t *overlay = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_BOOSTFLAME);
			P_SetTarget(&overlay->target, player->mo);
			P_SetScale(overlay, (overlay->destscale = player->mo->scale));
			K_FlipFromObject(overlay, player->mo);
		}
	}

	if (type != 0)
	{
		player->pflags |= PF_ATTACKDOWN;
		K_PlayBoostTaunt(player->mo);

	}

	player->kartstuff[k_sneakertimer] = sneakertime;

	// set angle for spun out players:
	player->kartstuff[k_boostangle] = (INT32)player->mo->angle;
}

static void K_DoShrink(player_t *user)
{
	INT32 i;
	mobj_t *mobj, *next;

	S_StartSound(user->mo, sfx_kc46); // Sound the BANG!
	user->pflags |= PF_ATTACKDOWN;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i] || players[i].spectator || !players[i].mo)
			continue;
		if (&players[i] == user)
			continue;
		if (players[i].kartstuff[k_position] < user->kartstuff[k_position])
		{
			//P_FlashPal(&players[i], PAL_NUKE, 10);

			// Grow should get taken away.
			if (players[i].kartstuff[k_growshrinktimer] > 0)
				K_RemoveGrowShrink(&players[i]);
			else
			{
				// Start shrinking!
				K_DropItems(&players[i]);
				players[i].kartstuff[k_growshrinktimer] = -(15*TICRATE);

				if (players[i].mo && !P_MobjWasRemoved(players[i].mo))
				{
					players[i].mo->scalespeed = mapobjectscale/TICRATE;
					players[i].mo->destscale = (6*mapobjectscale)/8;
					if (cv_kartdebugshrink.value && !modeattacking && !players[i].bot)
						players[i].mo->destscale = (6*players[i].mo->destscale)/8;
					S_StartSound(players[i].mo, sfx_kc59);
				}
			}
		}
	}

	// kill everything in the kitem list while we're at it:
	for (mobj = kitemcap; mobj; mobj = next)
	{
		next = mobj->itnext;

		// check if the item is being held by a player behind us before removing it.
		// check if the item is a "shield" first, bc i'm p sure thrown items keep the player that threw em as target anyway

		if (mobj->type == MT_BANANA_SHIELD || mobj->type == MT_JAWZ_SHIELD ||
		mobj->type == MT_SSMINE_SHIELD || mobj->type == MT_EGGMANITEM_SHIELD ||
		mobj->type == MT_SINK_SHIELD || mobj->type == MT_ORBINAUT_SHIELD)
		{
			if (mobj->target && mobj->target->player)
			{
				if (mobj->target->player->kartstuff[k_position] > user->kartstuff[k_position])
					continue; // this guy's behind us, don't take his stuff away!
			}
		}

		mobj->destscale = 0;
		mobj->flags &= ~(MF_SOLID|MF_SHOOTABLE|MF_SPECIAL);
		mobj->flags |= MF_NOCLIPTHING; // Just for safety

		if (mobj->type == MT_SPB)
			spbplace = -1;
	}
}


void K_DoPogoSpring(mobj_t *mo, fixed_t vertispeed, UINT8 sound)
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
			if (mo->player->kartstuff[k_pogospring] != 2)
			{
				if (mo->player->kartstuff[k_sneakertimer])
					thrust = FixedMul(thrust, (5*FRACUNIT)/4);
				else if (mo->player->kartstuff[k_invincibilitytimer])
					thrust = FixedMul(thrust, (9*FRACUNIT)/8);
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

static void K_ThrowLandMine(player_t *player)
{
	mobj_t *landMine;
	mobj_t *throwmo;

	landMine = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z + player->mo->height/2, MT_LANDMINE);
	K_FlipFromObject(landMine, player->mo);
	landMine->threshold = 10;

	if (landMine->info->seesound)
		S_StartSound(player->mo, landMine->info->seesound);

	P_SetTarget(&landMine->target, player->mo);

	P_SetScale(landMine, player->mo->scale);
	landMine->destscale = player->mo->destscale;

	landMine->angle = player->mo->angle;

	landMine->momz = (30 * mapobjectscale * P_MobjFlip(player->mo)) + player->mo->momz;
	landMine->color = player->skincolor;

	throwmo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z + player->mo->height/2, MT_FIREDITEM);
	P_SetTarget(&throwmo->target, player->mo);
	// Ditto:
	if (player->mo->eflags & MFE_VERTICALFLIP)
	{
		throwmo->z -= player->mo->height;
		throwmo->eflags |= MFE_VERTICALFLIP;
	}

	throwmo->movecount = 0; // above player
}

void K_KillBananaChain(mobj_t *banana, mobj_t *inflictor, mobj_t *source)
{
	mobj_t *cachenext;

killnext:
	cachenext = banana->hnext;

	if (banana->health)
	{
		if (banana->eflags & MFE_VERTICALFLIP)
			banana->z -= banana->height;
		else
			banana->z += banana->height;

		S_StartSound(banana, banana->info->deathsound);
		P_KillMobj(banana, inflictor, source, DMG_NORMAL);

		P_SetObjectMomZ(banana, 8*FRACUNIT, false);
		if (inflictor)
			P_InstaThrust(banana, R_PointToAngle2(inflictor->x, inflictor->y, banana->x, banana->y)+ANGLE_90, 16*FRACUNIT);
	}

	if ((banana = cachenext))
		goto killnext;
}

// Just for firing/dropping items.
void K_UpdateHnextList(player_t *player, boolean clean)
{
	mobj_t *work = player->mo, *nextwork;

	if (!work)
		return;

	nextwork = work->hnext;

	while ((work = nextwork) && !(work == NULL || P_MobjWasRemoved(work)))
	{
		nextwork = work->hnext;

		if (!clean && (!work->movedir || work->movedir <= (UINT16)player->kartstuff[k_itemamount]))
		{
			continue;
		}

		P_RemoveMobj(work);
	}

	if (player->mo->hnext == NULL || P_MobjWasRemoved(player->mo->hnext))
	{
		// Like below, try to clean up the pointer if it's NULL.
		// Maybe this was a cause of the shrink/eggbox fails?
		P_SetTarget(&player->mo->hnext, NULL);
	}
}

// For getting hit!
void K_DropHnextList(player_t *player, boolean keepshields)
{
	mobj_t *work = player->mo, *nextwork, *dropwork;
	INT32 flip;
	mobjtype_t type;
	boolean orbit, ponground, dropall = true;
	INT32 shield = K_GetShieldFromItem(player->kartstuff[k_itemtype]);

	if (work == NULL || P_MobjWasRemoved(work))
	{
		return;
	}

	flip = P_MobjFlip(player->mo);
	ponground = P_IsObjectOnGround(player->mo);

	if (shield != KSHIELD_NONE && !keepshields)
	{
		if (shield == KSHIELD_THUNDER)
		{
			K_DoThunderShield(player);
		}

		player->kartstuff[k_curshield] = KSHIELD_NONE;
		player->kartstuff[k_itemtype] = KITEM_NONE;
		player->kartstuff[k_itemamount] = player->kartstuff[k_itemheld] = 0;
	}

	nextwork = work->hnext;

	while ((work = nextwork) && !(work == NULL || P_MobjWasRemoved(work)))
	{
		nextwork = work->hnext;

		switch (work->type)
		{
			// Kart orbit items
			case MT_ORBINAUT_SHIELD:
				orbit = true;
				type = MT_ORBINAUT;
				break;
			case MT_JAWZ_SHIELD:
				orbit = true;
				type = MT_JAWZ_DUD;
				break;
			// Kart trailing items
			case MT_BANANA_SHIELD:
				orbit = false;
				type = MT_BANANA;
				break;
			case MT_SSMINE_SHIELD:
				orbit = false;
				dropall = false;
				type = MT_SSMINE;
				break;
			case MT_EGGMANITEM_SHIELD:
				orbit = false;
				type = MT_EGGMANITEM;
				break;
			// intentionally do nothing
			case MT_ROCKETSNEAKER:
			case MT_SINK_SHIELD:
				return;
			default:
				continue;
		}

		dropwork = P_SpawnMobj(work->x, work->y, work->z, type);

		P_SetTarget(&dropwork->target, player->mo);
		P_AddKartItem(dropwork); // needs to be called here so shrink can bust items off players in front of the user.

		dropwork->angle = work->angle;

		dropwork->flags |= MF_NOCLIPTHING;
		dropwork->flags2 = work->flags2;
		dropwork->eflags = work->eflags;

		dropwork->floorz = work->floorz;
		dropwork->ceilingz = work->ceilingz;

		if (ponground)
		{
			// floorz and ceilingz aren't properly set to account for FOFs and Polyobjects on spawn
			// This should set it for FOFs
			//P_TeleportMove(dropwork, dropwork->x, dropwork->y, dropwork->z); -- handled better by above floorz/ceilingz passing

			if (flip == 1)
			{
				if (dropwork->floorz > dropwork->target->z - dropwork->height)
				{
					dropwork->z = dropwork->floorz;
				}
			}
			else
			{
				if (dropwork->ceilingz < dropwork->target->z + dropwork->target->height + dropwork->height)
				{
					dropwork->z = dropwork->ceilingz - dropwork->height;
				}
			}
		}

		if (orbit) // splay out
		{
			dropwork->flags2 |= MF2_AMBUSH;

			dropwork->z += flip;

			dropwork->momx = player->mo->momx>>1;
			dropwork->momy = player->mo->momy>>1;
			dropwork->momz = 3*flip*mapobjectscale;

			if (dropwork->eflags & MFE_UNDERWATER)
				dropwork->momz = (117 * dropwork->momz) / 200;

			P_Thrust(dropwork, work->angle - ANGLE_90, 6*mapobjectscale);

			dropwork->movecount = 2;
			dropwork->movedir = work->angle - ANGLE_90;

			P_SetMobjState(dropwork, dropwork->info->deathstate);

			dropwork->tics = -1;

			if (type == MT_JAWZ_DUD)
			{
				dropwork->z += 20*flip*dropwork->scale;
			}
			else
			{
				dropwork->color = work->color;
				dropwork->angle -= ANGLE_90;
			}
		}
		else // plop on the ground
		{
			dropwork->flags &= ~MF_NOCLIPTHING;
			dropwork->threshold = 10;
		}

		P_RemoveMobj(work);
	}

	// we need this here too because this is done in afterthink - pointers are cleaned up at the START of each tic...
	P_SetTarget(&player->mo->hnext, NULL);

	player->kartstuff[k_bananadrag] = 0;

	if (player->kartstuff[k_eggmanheld])
	{
		player->kartstuff[k_eggmanheld] = 0;
	}
	else if (player->kartstuff[k_itemheld]
		&& (dropall || (--player->kartstuff[k_itemamount] <= 0)))
	{
		player->kartstuff[k_itemamount] = player->kartstuff[k_itemheld] = 0;
		player->kartstuff[k_itemtype] = KITEM_NONE;
	}
}

mobj_t *K_CreatePaperItem(fixed_t x, fixed_t y, fixed_t z, angle_t angle, SINT8 flip, UINT8 type, UINT8 amount)
{
	mobj_t *drop = P_SpawnMobj(x, y, z, MT_FLOATINGITEM);
	P_SetScale(drop, drop->scale>>4);
	drop->destscale = (3*drop->destscale)/2;

	drop->angle = angle;
	P_Thrust(drop,
		FixedAngle(P_RandomFixed() * 180) + angle,
		16*mapobjectscale);

	drop->momz = flip * 3 * mapobjectscale;
	if (drop->eflags & MFE_UNDERWATER)
		drop->momz = (117 * drop->momz) / 200;

	if (type == 0)
	{
		UINT8 useodds = 0;
		INT32 spawnchance[NUMKARTRESULTS];
		INT32 totalspawnchance = 0;
		INT32 i;

		memset(spawnchance, 0, sizeof (spawnchance));

		useodds = amount;

		for (i = 1; i < NUMKARTRESULTS; i++)
			spawnchance[i] = (totalspawnchance += K_KartGetItemOdds(useodds, i, 0, false, false, false));

		if (totalspawnchance > 0)
		{
			UINT8 newType;
			UINT8 newAmount;

			totalspawnchance = P_RandomKey(totalspawnchance);
			for (i = 0; i < NUMKARTRESULTS && spawnchance[i] <= totalspawnchance; i++);

			// TODO: this is bad!
			// K_KartGetItemResult requires a player
			// but item roulette will need rewritten to change this

			switch (i)
			{
				// Special roulettes first, then the generic ones are handled by default
				case KRITEM_DUALSNEAKER: // Sneaker x2
					newType = KITEM_SNEAKER;
					newAmount = 2;
					break;
				case KRITEM_TRIPLESNEAKER: // Sneaker x3
					newType = KITEM_SNEAKER;
					newAmount = 3;
					break;
				case KRITEM_TRIPLEBANANA: // Banana x3
					newType = KITEM_BANANA;
					newAmount = 3;
					break;
				case KRITEM_TENFOLDBANANA: // Banana x10
					newType = KITEM_BANANA;
					newAmount = 10;
					break;
				case KRITEM_TRIPLEORBINAUT: // Orbinaut x3
					newType = KITEM_ORBINAUT;
					newAmount = 3;
					break;
				case KRITEM_QUADORBINAUT: // Orbinaut x4
					newType = KITEM_ORBINAUT;
					newAmount = 4;
					break;
				case KRITEM_DUALJAWZ: // Jawz x2
					newType = KITEM_JAWZ;
					newAmount = 2;
					break;
				default:
					newType = i;
					newAmount = 1;
					break;
			}

			if (newAmount > 1)
			{
				UINT8 j;

				for (j = 0; j < newAmount-1; j++)
				{
					K_CreatePaperItem(
						x, y, z,
						angle, flip,
						newType, 1
					);
				}
			}

			drop->threshold = newType;
			drop->movecount = 1;
		}
		else
		{
			drop->threshold = 1;
			drop->movecount = 1;
		}
	}
	else
	{
		drop->threshold = type;
		drop->movecount = amount;
	}

	drop->flags |= MF_NOCLIPTHING;

	return drop;
}

// For getting EXTRA hit!
void K_DropItems(player_t *player)
{
	K_DropHnextList(player, true);

	if (player->mo && !P_MobjWasRemoved(player->mo) && player->kartstuff[k_itemamount] > 0)
	{
		mobj_t *drop = K_CreatePaperItem(
			player->mo->x, player->mo->y, player->mo->z + player->mo->height/2,
			player->mo->angle + ANGLE_90, P_MobjFlip(player->mo),
			player->kartstuff[k_itemtype], player->kartstuff[k_itemamount]
		);

		K_FlipFromObject(drop, player->mo);
	}

	K_StripItems(player);
}

void K_DropRocketSneaker(player_t *player)
{
	mobj_t *shoe = player->mo;
	fixed_t flingangle;
	boolean leftshoe = true; //left shoe is first

	if (!(player->mo && !P_MobjWasRemoved(player->mo) && player->mo->hnext && !P_MobjWasRemoved(player->mo->hnext)))
		return;

	while ((shoe = shoe->hnext) && !P_MobjWasRemoved(shoe))
	{
		if (shoe->type != MT_ROCKETSNEAKER)
			return; //woah, not a rocketsneaker, bail! safeguard in case this gets used when you're holding non-rocketsneakers

		shoe->drawflags &= ~MFD_DONTDRAW;
		shoe->flags &= ~MF_NOGRAVITY;
		shoe->angle += ANGLE_45;

		if (shoe->eflags & MFE_VERTICALFLIP)
			shoe->z -= shoe->height;
		else
			shoe->z += shoe->height;

		//left shoe goes off tot eh left, right shoe off to the right
		if (leftshoe)
			flingangle = -(ANG60);
		else
			flingangle = ANG60;

		S_StartSound(shoe, shoe->info->deathsound);
		P_SetObjectMomZ(shoe, 8*FRACUNIT, false);
		P_InstaThrust(shoe, R_PointToAngle2(shoe->target->x, shoe->target->y, shoe->x, shoe->y)+flingangle, 16*FRACUNIT);
		shoe->momx += shoe->target->momx;
		shoe->momy += shoe->target->momy;
		shoe->momz += shoe->target->momz;
		shoe->extravalue2 = 1;

		leftshoe = false;
	}
	P_SetTarget(&player->mo->hnext, NULL);
	player->kartstuff[k_rocketsneakertimer] = 0;
}

void K_DropKitchenSink(player_t *player)
{
	if (!(player->mo && !P_MobjWasRemoved(player->mo) && player->mo->hnext && !P_MobjWasRemoved(player->mo->hnext)))
		return;

	if (player->mo->hnext->type != MT_SINK_SHIELD)
		return; //so we can just call this function regardless of what is being held

	P_KillMobj(player->mo->hnext, NULL, NULL, DMG_NORMAL);

	P_SetTarget(&player->mo->hnext, NULL);
}

// When an item in the hnext chain dies.
void K_RepairOrbitChain(mobj_t *orbit)
{
	mobj_t *cachenext = orbit->hnext;

	// First, repair the chain
	if (orbit->hnext && orbit->hnext->health && !P_MobjWasRemoved(orbit->hnext))
	{
		P_SetTarget(&orbit->hnext->hprev, orbit->hprev);
		P_SetTarget(&orbit->hnext, NULL);
	}

	if (orbit->hprev && orbit->hprev->health && !P_MobjWasRemoved(orbit->hprev))
	{
		P_SetTarget(&orbit->hprev->hnext, cachenext);
		P_SetTarget(&orbit->hprev, NULL);
	}

	// Then recount to make sure item amount is correct
	if (orbit->target && orbit->target->player)
	{
		INT32 num = 0;

		mobj_t *cur = orbit->target->hnext;
		mobj_t *prev = NULL;

		while (cur && !P_MobjWasRemoved(cur))
		{
			prev = cur;
			cur = cur->hnext;
			if (++num > orbit->target->player->kartstuff[k_itemamount])
				P_RemoveMobj(prev);
			else
				prev->movedir = num;
		}

		if (orbit->target->player->kartstuff[k_itemamount] != num)
			orbit->target->player->kartstuff[k_itemamount] = num;
	}
}

// Simplified version of a code bit in P_MobjFloorZ
static fixed_t K_BananaSlopeZ(pslope_t *slope, fixed_t x, fixed_t y, fixed_t z, fixed_t radius, boolean ceiling)
{
	fixed_t testx, testy;

	if (slope == NULL)
	{
		testx = x;
		testy = y;
	}
	else
	{
		if (slope->d.x < 0)
			testx = radius;
		else
			testx = -radius;

		if (slope->d.y < 0)
			testy = radius;
		else
			testy = -radius;

		if ((slope->zdelta > 0) ^ !!(ceiling))
		{
			testx = -testx;
			testy = -testy;
		}

		testx += x;
		testy += y;
	}

	return P_GetZAt(slope, testx, testy, z);
}

static void K_CalculateBananaSlope(mobj_t *mobj, fixed_t x, fixed_t y, fixed_t z, fixed_t radius, fixed_t height, boolean flip, boolean player)
{
	fixed_t newz;
	sector_t *sec;
	pslope_t *slope = NULL;

	sec = R_PointInSubsector(x, y)->sector;

	if (flip)
	{
		slope = sec->c_slope;
		newz = K_BananaSlopeZ(slope, x, y, sec->ceilingheight, radius, true);
	}
	else
	{
		slope = sec->f_slope;
		newz = K_BananaSlopeZ(slope, x, y, sec->floorheight, radius, true);
	}

	// Check FOFs for a better suited slope
	if (sec->ffloors)
	{
		ffloor_t *rover;

		for (rover = sec->ffloors; rover; rover = rover->next)
		{
			fixed_t top, bottom;
			fixed_t d1, d2;

			if (!(rover->flags & FF_EXISTS))
				continue;

			if ((!(((rover->flags & FF_BLOCKPLAYER && player)
				|| (rover->flags & FF_BLOCKOTHERS && !player))
				|| (rover->flags & FF_QUICKSAND))
				|| (rover->flags & FF_SWIMMABLE)))
				continue;

			top = K_BananaSlopeZ(*rover->t_slope, x, y, *rover->topheight, radius, false);
			bottom = K_BananaSlopeZ(*rover->b_slope, x, y, *rover->bottomheight, radius, true);

			if (flip)
			{
				if (rover->flags & FF_QUICKSAND)
				{
					if (z < top && (z + height) > bottom)
					{
						if (newz > (z + height))
						{
							newz = (z + height);
							slope = NULL;
						}
					}
					continue;
				}

				d1 = (z + height) - (top + ((bottom - top)/2));
				d2 = z - (top + ((bottom - top)/2));

				if (bottom < newz && abs(d1) < abs(d2))
				{
					newz = bottom;
					slope = *rover->b_slope;
				}
			}
			else
			{
				if (rover->flags & FF_QUICKSAND)
				{
					if (z < top && (z + height) > bottom)
					{
						if (newz < z)
						{
							newz = z;
							slope = NULL;
						}
					}
					continue;
				}

				d1 = z - (bottom + ((top - bottom)/2));
				d2 = (z + height) - (bottom + ((top - bottom)/2));

				if (top > newz && abs(d1) < abs(d2))
				{
					newz = top;
					slope = *rover->t_slope;
				}
			}
		}
	}

	//mobj->standingslope = slope;
	P_SetPitchRollFromSlope(mobj, slope);
}

// Move the hnext chain!
static void K_MoveHeldObjects(player_t *player)
{
	if (!player->mo)
		return;

	if (!player->mo->hnext)
	{
		player->kartstuff[k_bananadrag] = 0;
		if (player->kartstuff[k_eggmanheld])
			player->kartstuff[k_eggmanheld] = 0;
		else if (player->kartstuff[k_itemheld])
		{
			player->kartstuff[k_itemamount] = player->kartstuff[k_itemheld] = 0;
			player->kartstuff[k_itemtype] = KITEM_NONE;
		}
		return;
	}

	if (P_MobjWasRemoved(player->mo->hnext))
	{
		// we need this here too because this is done in afterthink - pointers are cleaned up at the START of each tic...
		P_SetTarget(&player->mo->hnext, NULL);
		player->kartstuff[k_bananadrag] = 0;
		if (player->kartstuff[k_eggmanheld])
			player->kartstuff[k_eggmanheld] = 0;
		else if (player->kartstuff[k_itemheld])
		{
			player->kartstuff[k_itemamount] = player->kartstuff[k_itemheld] = 0;
			player->kartstuff[k_itemtype] = KITEM_NONE;
		}
		return;
	}

	switch (player->mo->hnext->type)
	{
		case MT_ORBINAUT_SHIELD: // Kart orbit items
		case MT_JAWZ_SHIELD:
			{
				mobj_t *cur = player->mo->hnext;
				fixed_t speed = ((8 - min(4, player->kartstuff[k_itemamount])) * cur->info->speed) / 7;

				player->kartstuff[k_bananadrag] = 0; // Just to make sure

				while (cur && !P_MobjWasRemoved(cur))
				{
					const fixed_t radius = FixedHypot(player->mo->radius, player->mo->radius) + FixedHypot(cur->radius, cur->radius); // mobj's distance from its Target, or Radius.
					fixed_t z;

					if (!cur->health)
					{
						cur = cur->hnext;
						continue;
					}

					cur->color = player->skincolor;

					cur->angle -= ANGLE_90;
					cur->angle += FixedAngle(speed);

					if (cur->extravalue1 < radius)
						cur->extravalue1 += P_AproxDistance(cur->extravalue1, radius) / 12;
					if (cur->extravalue1 > radius)
						cur->extravalue1 = radius;

					// If the player is on the ceiling, then flip your items as well.
					if (player && player->mo->eflags & MFE_VERTICALFLIP)
						cur->eflags |= MFE_VERTICALFLIP;
					else
						cur->eflags &= ~MFE_VERTICALFLIP;

					// Shrink your items if the player shrunk too.
					P_SetScale(cur, (cur->destscale = FixedMul(FixedDiv(cur->extravalue1, radius), player->mo->scale)));

					if (P_MobjFlip(cur) > 0)
						z = player->mo->z;
					else
						z = player->mo->z + player->mo->height - cur->height;

					cur->flags |= MF_NOCLIPTHING; // temporarily make them noclip other objects so they can't hit anyone while in the player
					P_TeleportMove(cur, player->mo->x, player->mo->y, z);
					cur->momx = FixedMul(FINECOSINE(cur->angle>>ANGLETOFINESHIFT), cur->extravalue1);
					cur->momy = FixedMul(FINESINE(cur->angle>>ANGLETOFINESHIFT), cur->extravalue1);
					cur->flags &= ~MF_NOCLIPTHING;

					if (!P_TryMove(cur, player->mo->x + cur->momx, player->mo->y + cur->momy, true))
						P_SlideMove(cur);

					if (P_IsObjectOnGround(player->mo))
					{
						if (P_MobjFlip(cur) > 0)
						{
							if (cur->floorz > player->mo->z - cur->height)
								z = cur->floorz;
						}
						else
						{
							if (cur->ceilingz < player->mo->z + player->mo->height + cur->height)
								z = cur->ceilingz - cur->height;
						}
					}

					// Center it during the scale up animation
					z += (FixedMul(mobjinfo[cur->type].height, player->mo->scale - cur->scale)>>1) * P_MobjFlip(cur);

					cur->z = z;
					cur->momx = cur->momy = 0;
					cur->angle += ANGLE_90;

					cur = cur->hnext;
				}
			}
			break;
		case MT_BANANA_SHIELD: // Kart trailing items
		case MT_SSMINE_SHIELD:
		case MT_EGGMANITEM_SHIELD:
		case MT_SINK_SHIELD:
			{
				mobj_t *cur = player->mo->hnext;
				mobj_t *targ = player->mo;

				if (P_IsObjectOnGround(player->mo) && player->speed > 0)
					player->kartstuff[k_bananadrag]++;

				while (cur && !P_MobjWasRemoved(cur))
				{
					const fixed_t radius = FixedHypot(targ->radius, targ->radius) + FixedHypot(cur->radius, cur->radius);
					angle_t ang;
					fixed_t targx, targy, targz;
					fixed_t speed, dist;

					if (cur->type == MT_EGGMANITEM_SHIELD)
					{
						// Decided that this should use their "canon" color.
						cur->color = SKINCOLOR_BLACK;
					}

					cur->flags &= ~MF_NOCLIPTHING;

					if ((player->mo->eflags & MFE_VERTICALFLIP) != (cur->eflags & MFE_VERTICALFLIP))
						K_FlipFromObject(cur, player->mo);

					if (!cur->health)
					{
						cur = cur->hnext;
						continue;
					}

					if (cur->extravalue1 < radius)
						cur->extravalue1 += FixedMul(P_AproxDistance(cur->extravalue1, radius), FRACUNIT/12);
					if (cur->extravalue1 > radius)
						cur->extravalue1 = radius;

					if (cur != player->mo->hnext)
					{
						targ = cur->hprev;
						dist = cur->extravalue1/4;
					}
					else
						dist = cur->extravalue1/2;

					if (!targ || P_MobjWasRemoved(targ))
						continue;

					// Shrink your items if the player shrunk too.
					P_SetScale(cur, (cur->destscale = FixedMul(FixedDiv(cur->extravalue1, radius), player->mo->scale)));

					ang = targ->angle;
					targx = targ->x + P_ReturnThrustX(cur, ang + ANGLE_180, dist);
					targy = targ->y + P_ReturnThrustY(cur, ang + ANGLE_180, dist);
					targz = targ->z;

					speed = FixedMul(R_PointToDist2(cur->x, cur->y, targx, targy), 3*FRACUNIT/4);
					if (P_IsObjectOnGround(targ))
						targz = cur->floorz;

					cur->angle = R_PointToAngle2(cur->x, cur->y, targx, targy);

					/*if (P_IsObjectOnGround(player->mo) && player->speed > 0 && player->kartstuff[k_bananadrag] > TICRATE
						&& P_RandomChance(min(FRACUNIT/2, FixedDiv(player->speed, K_GetKartSpeed(player, false))/2)))
					{
						if (leveltime & 1)
							targz += 8*(2*FRACUNIT)/7;
						else
							targz -= 8*(2*FRACUNIT)/7;
					}*/

					if (speed > dist)
						P_InstaThrust(cur, cur->angle, speed-dist);

					P_SetObjectMomZ(cur, FixedMul(targz - cur->z, 7*FRACUNIT/8) - gravity, false);

					if (R_PointToDist2(cur->x, cur->y, targx, targy) > 768*FRACUNIT)
						P_TeleportMove(cur, targx, targy, cur->z);

					if (P_IsObjectOnGround(cur))
					{
						K_CalculateBananaSlope(cur, cur->x, cur->y, cur->z,
							cur->radius, cur->height, (cur->eflags & MFE_VERTICALFLIP), false);
					}

					cur = cur->hnext;
				}
			}
			break;
		case MT_ROCKETSNEAKER: // Special rocket sneaker stuff
			{
				mobj_t *cur = player->mo->hnext;
				INT32 num = 0;

				while (cur && !P_MobjWasRemoved(cur))
				{
					const fixed_t radius = FixedHypot(player->mo->radius, player->mo->radius) + FixedHypot(cur->radius, cur->radius);
					boolean vibrate = ((leveltime & 1) && !cur->tracer);
					angle_t angoffset;
					fixed_t targx, targy, targz;

					cur->flags &= ~MF_NOCLIPTHING;

					if (player->kartstuff[k_rocketsneakertimer] <= TICRATE && (leveltime & 1))
						cur->drawflags |= MFD_DONTDRAW;
					else
						cur->drawflags &= ~MFD_DONTDRAW;

					if (num & 1)
						P_SetMobjStateNF(cur, (vibrate ? S_ROCKETSNEAKER_LVIBRATE : S_ROCKETSNEAKER_L));
					else
						P_SetMobjStateNF(cur, (vibrate ? S_ROCKETSNEAKER_RVIBRATE : S_ROCKETSNEAKER_R));

					if (!player->kartstuff[k_rocketsneakertimer] || cur->extravalue2 || !cur->health)
					{
						num = (num+1) % 2;
						cur = cur->hnext;
						continue;
					}

					if (cur->extravalue1 < radius)
						cur->extravalue1 += FixedMul(P_AproxDistance(cur->extravalue1, radius), FRACUNIT/12);
					if (cur->extravalue1 > radius)
						cur->extravalue1 = radius;

					// Shrink your items if the player shrunk too.
					P_SetScale(cur, (cur->destscale = FixedMul(FixedDiv(cur->extravalue1, radius), player->mo->scale)));

#if 1
					{
						angle_t input = player->drawangle - cur->angle;
						boolean invert = (input > ANGLE_180);
						if (invert)
							input = InvAngle(input);

						input = FixedAngle(AngleFixed(input)/4);
						if (invert)
							input = InvAngle(input);

						cur->angle = cur->angle + input;
					}
#else
					cur->angle = player->drawangle;
#endif

					angoffset = ANGLE_90 + (ANGLE_180 * num);

					targx = player->mo->x + P_ReturnThrustX(cur, cur->angle + angoffset, cur->extravalue1);
					targy = player->mo->y + P_ReturnThrustY(cur, cur->angle + angoffset, cur->extravalue1);

					{ // bobbing, copy pasted from my kimokawaiii entry
						const fixed_t pi = (22<<FRACBITS) / 7; // loose approximation, this doesn't need to be incredibly precise
						fixed_t sine = FixedMul(player->mo->scale, 8 * FINESINE((((2*pi*(4*TICRATE)) * leveltime)>>ANGLETOFINESHIFT) & FINEMASK));
						targz = (player->mo->z + (player->mo->height/2)) + sine;
						if (player->mo->eflags & MFE_VERTICALFLIP)
							targz += (player->mo->height/2 - 32*player->mo->scale)*6;

					}

					if (cur->tracer)
					{
						fixed_t diffx, diffy, diffz;

						diffx = targx - cur->x;
						diffy = targy - cur->y;
						diffz = targz - cur->z;

						P_TeleportMove(cur->tracer, cur->tracer->x + diffx + P_ReturnThrustX(cur, cur->angle + angoffset, 6*cur->scale),
							cur->tracer->y + diffy + P_ReturnThrustY(cur, cur->angle + angoffset, 6*cur->scale), cur->tracer->z + diffz);
						P_SetScale(cur->tracer, (cur->tracer->destscale = 3*cur->scale/4));
					}

					P_TeleportMove(cur, targx, targy, targz);
					K_FlipFromObject(cur, player->mo);	// Update graviflip in real time thanks.

					cur->roll = player->mo->roll;
					cur->pitch = player->mo->pitch;

					num = (num+1) % 2;
					cur = cur->hnext;
				}
			}
			break;
		default:
			break;
	}
}

player_t *K_FindJawzTarget(mobj_t *actor, player_t *source)
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
		if (player->kartstuff[k_hyudorotimer])
			continue;

		// Find the angle, see who's got the best.
		thisang = actor->angle - R_PointToAngle2(actor->x, actor->y, player->mo->x, player->mo->y);
		if (thisang > ANGLE_180)
			thisang = InvAngle(thisang);

		// Jawz only go after the person directly ahead of you in race... sort of literally now!
		if (gametyperules & GTR_CIRCUIT)
		{
			// Don't go for people who are behind you
			if (thisang > ANGLE_67h)
				continue;
			// Don't pay attention to people who aren't above your position
			if (player->kartstuff[k_position] >= source->kartstuff[k_position])
				continue;
			if ((best == -1) || (player->kartstuff[k_position] > best))
			{
				wtarg = player;
				best = player->kartstuff[k_position];
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
			if (player->bumpers <= 0)
				continue;

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

// Engine Sounds.
static void K_UpdateEngineSounds(player_t *player, ticcmd_t *cmd)
{
	const INT32 numsnds = 13;

	const fixed_t closedist = 160*FRACUNIT;
	const fixed_t fardist = 1536*FRACUNIT;

	const UINT8 dampenval = 48; // 255 * 48 = close enough to FRACUNIT/6

	INT32 class, s, w; // engine class number

	UINT8 volume = 255;
	fixed_t volumedampen = FRACUNIT;

	INT32 targetsnd = 0;
	INT32 i;

	s = (player->kartspeed - 1) / 3;
	w = (player->kartweight - 1) / 3;

#define LOCKSTAT(stat) \
	if (stat < 0) { stat = 0; } \
	if (stat > 2) { stat = 2; }
	LOCKSTAT(s);
	LOCKSTAT(w);
#undef LOCKSTAT

	class = s + (3*w);

	if (leveltime < 8 || player->spectator)
	{
		// Silence the engines, and reset sound number while we're at it.
		player->karthud[khud_enginesnd] = 0;
		return;
	}

#if 0
	if ((leveltime % 8) != ((player-players) % 8)) // Per-player offset, to make engines sound distinct!
#else
	if (leveltime % 8)
#endif
	{
		// .25 seconds of wait time between each engine sound playback
		return;
	}

	if (player->respawn.state == RESPAWNST_DROP) // Dropdashing
	{
		// Dropdashing
		targetsnd = ((cmd->buttons & BT_ACCELERATE) ? 12 : 0);
	}
	else if (K_PlayerEBrake(player) == true)
	{
		// Spindashing
		targetsnd = ((cmd->buttons & BT_DRIFT) ? 12 : 0);
	}
	else
	{
		// Average out the value of forwardmove and the speed that you're moving at.
		targetsnd = (((6 * cmd->forwardmove) / 25) + ((player->speed / mapobjectscale) / 5)) / 2;
	}

	if (targetsnd < 0) { targetsnd = 0; }
	if (targetsnd > 12) { targetsnd = 12; }

	if (player->karthud[khud_enginesnd] < targetsnd) { player->karthud[khud_enginesnd]++; }
	if (player->karthud[khud_enginesnd] > targetsnd) { player->karthud[khud_enginesnd]--; }

	if (player->karthud[khud_enginesnd] < 0) { player->karthud[khud_enginesnd] = 0; }
	if (player->karthud[khud_enginesnd] > 12) { player->karthud[khud_enginesnd] = 12; }

	// This code calculates how many players (and thus, how many engine sounds) are within ear shot,
	// and rebalances the volume of your engine sound based on how far away they are.

	// This results in multiple things:
	// - When on your own, you will hear your own engine sound extremely clearly.
	// - When you were alone but someone is gaining on you, yours will go quiet, and you can hear theirs more clearly.
	// - When around tons of people, engine sounds will try to rebalance to not be as obnoxious.

	for (i = 0; i < MAXPLAYERS; i++)
	{
		UINT8 thisvol = 0;
		fixed_t dist;

		if (!playeringame[i] || !players[i].mo)
		{
			// This player doesn't exist.
			continue;
		}

		if (players[i].spectator)
		{
			// This player isn't playing an engine sound.
			continue;
		}

		if (P_IsDisplayPlayer(&players[i]))
		{
			// Don't dampen yourself!
			continue;
		}

		dist = P_AproxDistance(
			P_AproxDistance(
				player->mo->x - players[i].mo->x,
				player->mo->y - players[i].mo->y),
				player->mo->z - players[i].mo->z) / 2;

		dist = FixedDiv(dist, mapobjectscale);

		if (dist > fardist)
		{
			// ENEMY OUT OF RANGE !
			continue;
		}
		else if (dist < closedist)
		{
			// engine sounds' approx. range
			thisvol = 255;
		}
		else
		{
			thisvol = (15 * ((closedist - dist) / FRACUNIT)) / ((fardist - closedist) >> (FRACBITS+4));
		}

		volumedampen += (thisvol * dampenval);
	}

	if (volumedampen > FRACUNIT)
	{
		volume = FixedDiv(volume * FRACUNIT, volumedampen) / FRACUNIT;
	}

	if (volume <= 0)
	{
		// Don't need to play the sound at all.
		return;
	}

	S_StartSoundAtVolume(player->mo, (sfx_krta00 + player->karthud[khud_enginesnd]) + (class * numsnds), volume);
}

static void K_UpdateInvincibilitySounds(player_t *player)
{
	INT32 sfxnum = sfx_None;

	if (player->mo->health > 0 && !P_IsDisplayPlayer(player))
	{
		if (cv_kartinvinsfx.value)
		{
			if (player->kartstuff[k_invincibilitytimer] > 0) // Prioritize invincibility
				sfxnum = sfx_alarmi;
			else if (player->kartstuff[k_growshrinktimer] > 0)
				sfxnum = sfx_alarmg;
		}
		else
		{
			if (player->kartstuff[k_invincibilitytimer] > 0)
				sfxnum = sfx_kinvnc;
			else if (player->kartstuff[k_growshrinktimer] > 0)
				sfxnum = sfx_kgrow;
		}
	}

	if (sfxnum != sfx_None && !S_SoundPlaying(player->mo, sfxnum))
		S_StartSound(player->mo, sfxnum);

#define STOPTHIS(this) \
	if (sfxnum != this && S_SoundPlaying(player->mo, this)) \
		S_StopSoundByID(player->mo, this);
	STOPTHIS(sfx_alarmi);
	STOPTHIS(sfx_alarmg);
	STOPTHIS(sfx_kinvnc);
	STOPTHIS(sfx_kgrow);
#undef STOPTHIS
}

void K_KartPlayerHUDUpdate(player_t *player)
{
	if (player->karthud[khud_lapanimation])
		player->karthud[khud_lapanimation]--;

	if (player->karthud[khud_yougotem])
		player->karthud[khud_yougotem]--;

	if (player->karthud[khud_voices])
		player->karthud[khud_voices]--;

	if (player->karthud[khud_tauntvoices])
		player->karthud[khud_tauntvoices]--;

	if (!(player->pflags & PF_FAULT))
		player->karthud[khud_fault] = 0;
	else if (player->karthud[khud_fault] > 0 && player->karthud[khud_fault] < 2*TICRATE)
		player->karthud[khud_fault]++;

	if (gametype == GT_RACE)
	{
		// 0 is the fast spin animation, set at 30 tics of ring boost or higher!
		if (player->kartstuff[k_ringboost] >= 30)
			player->karthud[khud_ringdelay] = 0;
		else
			player->karthud[khud_ringdelay] = ((RINGANIM_DELAYMAX+1) * (30 - player->kartstuff[k_ringboost])) / 30;

		if (player->karthud[khud_ringframe] == 0 && player->karthud[khud_ringdelay] > RINGANIM_DELAYMAX)
		{
			player->karthud[khud_ringframe] = 0;
			player->karthud[khud_ringtics] = 0;
		}
		else if ((player->karthud[khud_ringtics]--) <= 0)
		{
			if (player->karthud[khud_ringdelay] == 0) // fast spin animation
			{
				player->karthud[khud_ringframe] = ((player->karthud[khud_ringframe]+2) % RINGANIM_NUMFRAMES);
				player->karthud[khud_ringtics] = 0;
			}
			else
			{
				player->karthud[khud_ringframe] = ((player->karthud[khud_ringframe]+1) % RINGANIM_NUMFRAMES);
				player->karthud[khud_ringtics] = min(RINGANIM_DELAYMAX, player->karthud[khud_ringdelay])-1;
			}
		}

		if (player->kartstuff[k_ringlock])
		{
			UINT8 normalanim = (leveltime % 14);
			UINT8 debtanim = 14 + (leveltime % 2);

			if (player->karthud[khud_ringspblock] >= 14) // debt animation
			{
				if ((player->rings > 0) // Get out of 0 ring animation
					&& (normalanim == 3 || normalanim == 10)) // on these transition frames.
					player->karthud[khud_ringspblock] = normalanim;
				else
					player->karthud[khud_ringspblock] = debtanim;
			}
			else // normal animation
			{
				if ((player->rings <= 0) // Go into 0 ring animation
					&& (player->karthud[khud_ringspblock] == 1 || player->karthud[khud_ringspblock] == 8)) // on these transition frames.
					player->karthud[khud_ringspblock] = debtanim;
				else
					player->karthud[khud_ringspblock] = normalanim;
			}
		}
		else
			player->karthud[khud_ringspblock] = (leveltime % 14); // reset to normal anim next time
	}

	if ((gametyperules & GTR_BUMPERS) && (player->exiting || player->karmadelay))
	{
		if (player->exiting)
		{
			if (player->exiting < 6*TICRATE)
				player->karthud[khud_cardanimation] += ((164-player->karthud[khud_cardanimation])/8)+1;
			else if (player->exiting == 6*TICRATE)
				player->karthud[khud_cardanimation] = 0;
			else if (player->karthud[khud_cardanimation] < 2*TICRATE)
				player->karthud[khud_cardanimation]++;
		}
		else
		{
			if (player->karmadelay < 6*TICRATE)
				player->karthud[khud_cardanimation] -= ((164-player->karthud[khud_cardanimation])/8)+1;
			else if (player->karmadelay < 9*TICRATE)
				player->karthud[khud_cardanimation] += ((164-player->karthud[khud_cardanimation])/8)+1;
		}

		if (player->karthud[khud_cardanimation] > 164)
			player->karthud[khud_cardanimation] = 164;
		if (player->karthud[khud_cardanimation] < 0)
			player->karthud[khud_cardanimation] = 0;
	}
	else if (gametype == GT_RACE && player->exiting)
	{
		if (player->karthud[khud_cardanimation] < 2*TICRATE)
			player->karthud[khud_cardanimation]++;
	}
	else
		player->karthud[khud_cardanimation] = 0;
}

#undef RINGANIM_DELAYMAX

// SRB2Kart: blockmap iterate for attraction shield users
static mobj_t *attractmo;
static fixed_t attractdist;
static inline boolean PIT_AttractingRings(mobj_t *thing)
{
	if (!attractmo || P_MobjWasRemoved(attractmo))
		return false;

	if (!attractmo->player)
		return false; // not a player

	if (thing->health <= 0 || !thing)
		return true; // dead

	if (thing->type != MT_RING && thing->type != MT_FLINGRING)
		return true; // not a ring

	if (thing->extravalue1)
		return true; // in special ring animation

	if (thing->cusval)
		return true; // already attracted

	// see if it went over / under
	if (attractmo->z - (attractdist>>2) > thing->z + thing->height)
		return true; // overhead
	if (attractmo->z + attractmo->height + (attractdist>>2) < thing->z)
		return true; // underneath

	if (P_AproxDistance(attractmo->x - thing->x, attractmo->y - thing->y) < attractdist)
		return true; // Too far away

	// set target
	P_SetTarget(&thing->tracer, attractmo);
	// flag to show it's been attracted once before
	thing->cusval = 1;
	return true; // find other rings
}

/** Looks for rings near a player in the blockmap.
  *
  * \param pmo Player object looking for rings to attract
  * \sa A_AttractChase
  */
static void K_LookForRings(mobj_t *pmo)
{
	INT32 bx, by, xl, xh, yl, yh;
	attractdist = FixedMul(RING_DIST, pmo->scale)>>2;

	// Use blockmap to check for nearby rings
	yh = (unsigned)(pmo->y + attractdist - bmaporgy)>>MAPBLOCKSHIFT;
	yl = (unsigned)(pmo->y - attractdist - bmaporgy)>>MAPBLOCKSHIFT;
	xh = (unsigned)(pmo->x + attractdist - bmaporgx)>>MAPBLOCKSHIFT;
	xl = (unsigned)(pmo->x - attractdist - bmaporgx)>>MAPBLOCKSHIFT;

	attractmo = pmo;

	for (by = yl; by <= yh; by++)
		for (bx = xl; bx <= xh; bx++)
			P_BlockThingsIterator(bx, by, PIT_AttractingRings);
}

void K_PogoSidemove(player_t *player)
{
	fixed_t movepushside = 0;
	angle_t movepushangle = 0, movepushsideangle = 0;
	fixed_t sidemove[2] = {2<<FRACBITS>>16, 4<<FRACBITS>>16};
	fixed_t side = player->kartstuff[k_pogosidemove];

	if (player->kartstuff[k_drift] != 0)
		movepushangle = player->mo->angle-(ANGLE_45/5)*player->kartstuff[k_drift];
	else
		movepushangle = player->mo->angle;

	movepushsideangle = movepushangle-ANGLE_90;

	// let movement keys cancel each other out
	if (player->cmd.turning < 0)
	{;
		side += sidemove[1];
	}
	else if (player->cmd.turning > 0 )
	{
		side -= sidemove[1];
	}

	if (side > MAXPLMOVE)
		side = MAXPLMOVE;
	else if (side < -MAXPLMOVE)
		side = -MAXPLMOVE;

	if (side !=0 && (!player->kartstuff[k_pogospring]))
		side = 0;

	// Sideways movement
	if (side != 0 && !((player->exiting || mapreset)))
	{
		if (side > 0)
			movepushside = (side * FRACUNIT/128) + FixedDiv(player->speed, K_GetKartSpeed(player, true));
		else
			movepushside = (side * FRACUNIT/128) - FixedDiv(player->speed, K_GetKartSpeed(player, true));

		player->mo->momx += P_ReturnThrustX(player->mo, movepushsideangle, movepushside);
		player->mo->momy += P_ReturnThrustY(player->mo, movepushsideangle, movepushside);
	}

}

void K_LegacyStart(player_t *player)
{

	if (leveltime <= starttime)
		player->powers[pw_nocontrol] = 2;

	// Start charging once you're given the opportunity.
	if (leveltime >= starttime-(2*TICRATE) && leveltime <= starttime)
	{
		if (player->cmd.buttons & BT_ACCELERATE)
		{
			if (player->kartstuff[k_boostcharge] == 0)
				player->kartstuff[k_boostcharge] = player->cmd.latency;

			player->kartstuff[k_boostcharge]++;
		}
		else
			player->kartstuff[k_boostcharge] = 0;
	}

	// Increase your size while charging your engine.
	if (leveltime < starttime+10)
	{
		player->mo->scalespeed = mapobjectscale/12;
		player->mo->destscale = mapobjectscale + (FixedMul(mapobjectscale, player->kartstuff[k_boostcharge]*131));
		if (cv_kartdebugshrink.value && !modeattacking && !player->bot)
			player->mo->destscale = (6*player->mo->destscale)/8;
	}

	// Determine the outcome of your charge.
	if (leveltime > starttime && player->kartstuff[k_boostcharge])
	{
		// Not even trying?
		if (player->kartstuff[k_boostcharge] < 35)
		{
			if (player->kartstuff[k_boostcharge] > 17)
				S_StartSound(player->mo, sfx_cdfm00); // chosen instead of a conventional skid because it's more engine-like
		}
		// Get an instant boost!
		else if (player->kartstuff[k_boostcharge] <= 50)
		{
			player->kartstuff[k_startboost] = (50-player->kartstuff[k_boostcharge])+20;

			if (player->kartstuff[k_boostcharge] <= 36)
			{
				player->kartstuff[k_startboost] = 0;
				K_DoSneaker(player, 0);
				player->kartstuff[k_sneakertimer] = 70; // PERFECT BOOST!!

				if (!player->kartstuff[k_floorboost] || player->kartstuff[k_floorboost] == 3) // Let everyone hear this one
					S_StartSound(player->mo, sfx_s25f);
			}
			else
			{
				K_SpawnDashDustRelease(player); // already handled for perfect boosts by K_DoSneaker
				if ((!player->kartstuff[k_floorboost] || player->kartstuff[k_floorboost] == 3) && P_IsLocalPlayer(player))
				{
					if (player->kartstuff[k_boostcharge] <= 40)
						S_StartSound(player->mo, sfx_cdfm01); // You were almost there!
					else
						S_StartSound(player->mo, sfx_s23c); // Nope, better luck next time.
				}
			}
		}
		// You overcharged your engine? Those things are expensive!!!
		else if (player->kartstuff[k_boostcharge] > 50)
		{
			//player->spinouttimer = 40;
			player->powers[pw_nocontrol] = 40;
			//S_StartSound(player->mo, sfx_kc34);
			S_StartSound(player->mo, sfx_s3k83);
			player->pflags |= PF_FAULT; // cheeky pflag reuse
		}

		player->kartstuff[k_boostcharge] = 0;
	}

}

/**	\brief	Decreases various kart timers and powers per frame. Called in P_PlayerThink in p_user.c

	\param	player	player object passed from P_PlayerThink
	\param	cmd		control input from player

	\return	void
*/
void K_KartPlayerThink(player_t *player, ticcmd_t *cmd)
{
	K_UpdateOffroad(player);
	K_UpdateDraft(player);
	K_UpdateEngineSounds(player, cmd); // Thanks, VAda!

	// update boost angle if not spun out
	if (!player->kartstuff[k_spinouttimer] && !player->kartstuff[k_wipeoutslow])
		player->kartstuff[k_boostangle] = (INT32)player->mo->angle;

	K_GetKartBoostPower(player);

	// Special effect objects!
	if (player->mo && !player->spectator)
	{
		if (player->kartstuff[k_dashpadcooldown]) // Twinkle Circuit afterimages
		{
			mobj_t *ghost;
			ghost = P_SpawnGhostMobj(player->mo);
			ghost->fuse = player->kartstuff[k_dashpadcooldown]+1;
			ghost->momx = player->mo->momx / (player->kartstuff[k_dashpadcooldown]+1);
			ghost->momy = player->mo->momy / (player->kartstuff[k_dashpadcooldown]+1);
			ghost->momz = player->mo->momz / (player->kartstuff[k_dashpadcooldown]+1);
			player->kartstuff[k_dashpadcooldown]--;
		}

		if (player->speed > 0)
		{
			// Speed lines
			if (player->kartstuff[k_sneakertimer] || player->kartstuff[k_ringboost]
				|| player->kartstuff[k_driftboost] || player->kartstuff[k_startboost]
				|| player->kartstuff[k_eggmanexplode])
			{
				mobj_t *fast = P_SpawnMobj(player->mo->x + (P_RandomRange(-36,36) * player->mo->scale),
					player->mo->y + (P_RandomRange(-36,36) * player->mo->scale),
					player->mo->z + (player->mo->height/2) + (P_RandomRange(-20,20) * player->mo->scale),
					MT_FASTLINE);

				P_SetTarget(&fast->target, player->mo);
				fast->angle = K_MomentumAngle(player->mo);
				fast->momx = 3*player->mo->momx/4;
				fast->momy = 3*player->mo->momy/4;
				fast->momz = 3*player->mo->momz/4;

				K_MatchGenericExtraFlags(fast, player->mo);

				// Make it red when you have the eggman speed boost
				if (player->kartstuff[k_eggmanexplode])
				{
					fast->color = SKINCOLOR_RED;
					fast->colorized = true;
				}
			}

			if (player->kartstuff[k_numboosts] > 0) // Boosting after images
			{
				mobj_t *ghost;
				ghost = P_SpawnGhostMobj(player->mo);
				ghost->extravalue1 = player->kartstuff[k_numboosts]+1;
				ghost->extravalue2 = (leveltime % ghost->extravalue1);
				ghost->fuse = ghost->extravalue1;
				ghost->frame |= FF_FULLBRIGHT;
				ghost->colorized = true;
				//ghost->color = player->skincolor;
				//ghost->momx = (3*player->mo->momx)/4;
				//ghost->momy = (3*player->mo->momy)/4;
				//ghost->momz = (3*player->mo->momz)/4;
				if (leveltime & 1)
					ghost->drawflags |= MFD_DONTDRAW;
			}

			if (P_IsObjectOnGround(player->mo))
			{
				// Offroad dust
				if (player->kartstuff[k_boostpower] < FRACUNIT)
				{
					K_SpawnWipeoutTrail(player->mo, true);
					if (leveltime % 6 == 0)
						S_StartSound(player->mo, sfx_cdfm70);
				}

				// Draft dust
				if (player->kartstuff[k_draftpower] >= FRACUNIT)
				{
					K_SpawnDraftDust(player->mo);
					/*if (leveltime % 23 == 0 || !S_SoundPlaying(player->mo, sfx_s265))
						S_StartSound(player->mo, sfx_s265);*/
				}
			}
		}

		if (gametype == GT_RACE && player->rings <= 0) // spawn ring debt indicator
		{
			mobj_t *debtflag = P_SpawnMobj(player->mo->x + player->mo->momx, player->mo->y + player->mo->momy,
				player->mo->z + player->mo->momz + player->mo->height + (24*player->mo->scale), MT_THOK);

			P_SetMobjState(debtflag, S_RINGDEBT);
			P_SetScale(debtflag, (debtflag->destscale = player->mo->scale));

			K_MatchGenericExtraFlags(debtflag, player->mo);
			debtflag->frame += (leveltime % 4);

			if ((leveltime/12) & 1)
				debtflag->frame += 4;

			debtflag->color = player->skincolor;
			debtflag->fuse = 2;

			debtflag->drawflags = K_GetPlayerDontDrawFlag(player);
		}

		if (player->kartstuff[k_springstars] && (leveltime & 1))
		{
			fixed_t randx = P_RandomRange(-40, 40) * player->mo->scale;
			fixed_t randy = P_RandomRange(-40, 40) * player->mo->scale;
			fixed_t randz = P_RandomRange(0, player->mo->height >> FRACBITS) << FRACBITS;
			mobj_t *star = P_SpawnMobj(
				player->mo->x + randx,
				player->mo->y + randy,
				player->mo->z + randz,
				MT_KARMAFIREWORK);

			star->color = player->kartstuff[k_springcolor];
			star->flags |= MF_NOGRAVITY;
			star->momx = player->mo->momx / 2;
			star->momy = player->mo->momy / 2;
			star->momz = player->mo->momz / 2;
			star->fuse = 12;
			star->scale = player->mo->scale;
			star->destscale = star->scale / 2;

			player->kartstuff[k_springstars]--;
		}
	}

	if (player->playerstate == PST_DEAD || (player->respawn.state == RESPAWNST_MOVE)) // Ensure these are set correctly here
	{
		player->mo->colorized = false;
		player->mo->color = player->skincolor;
	}
	else if (player->kartstuff[k_eggmanexplode]) // You're gonna diiiiie
	{
		const INT32 flashtime = 4<<(player->kartstuff[k_eggmanexplode]/TICRATE);
		if (player->kartstuff[k_eggmanexplode] == 1 || (player->kartstuff[k_eggmanexplode] % (flashtime/2) != 0))
		{
			player->mo->colorized = false;
			player->mo->color = player->skincolor;
		}
		else if (player->kartstuff[k_eggmanexplode] % flashtime == 0)
		{
			player->mo->colorized = true;
			player->mo->color = SKINCOLOR_BLACK;
		}
		else
		{
			player->mo->colorized = true;
			player->mo->color = SKINCOLOR_CRIMSON;
		}
	}
	else if (player->kartstuff[k_invincibilitytimer]) // setting players to use the star colormap and spawning afterimages
	{
		player->mo->colorized = true;
	}
	else if (player->kartstuff[k_growshrinktimer]) // Ditto, for grow/shrink
	{
		if (player->kartstuff[k_growshrinktimer] % 5 == 0)
		{
			player->mo->colorized = true;
			player->mo->color = (player->kartstuff[k_growshrinktimer] < 0 ? SKINCOLOR_CREAMSICLE : SKINCOLOR_PERIWINKLE);
		}
		else
		{
			player->mo->colorized = false;
			player->mo->color = player->skincolor;
		}
	}
	else if (player->kartstuff[k_killfield]) // You're gonna REALLY diiiiie
	{
		const INT32 flashtime = 4<<(4-(player->kartstuff[k_killfield]/TICRATE));
		if (player->kartstuff[k_killfield] == 1 || (player->kartstuff[k_killfield] % (flashtime/2) != 0))
		{
			player->mo->colorized = false;
			player->mo->color = player->skincolor;
		}
		else if (player->kartstuff[k_killfield] % flashtime == 0)
		{
			player->mo->colorized = true;
			player->mo->color = SKINCOLOR_BYZANTIUM;
		}
		else
		{
			player->mo->colorized = true;
			player->mo->color = SKINCOLOR_RUBY;
		}
	}
	else if (player->kartstuff[k_ringboost] && (leveltime & 1)) // ring boosting
	{
		player->mo->colorized = true;
	}
	else
	{
		player->mo->colorized = false;
	}

	if (player->kartstuff[k_itemtype] == KITEM_NONE)
		player->kartstuff[k_holdready] = 0;

	// DKR style camera for boosting
	if (player->karthud[khud_boostcam] != 0 || player->karthud[khud_destboostcam] != 0)
	{
		if (player->karthud[khud_boostcam] < player->karthud[khud_destboostcam]
			&& player->karthud[khud_destboostcam] != 0)
		{
			player->karthud[khud_boostcam] += FRACUNIT/(TICRATE/4);
			if (player->karthud[khud_boostcam] >= player->karthud[khud_destboostcam])
				player->karthud[khud_destboostcam] = 0;
		}
		else
		{
			player->karthud[khud_boostcam] -= FRACUNIT/TICRATE;
			if (player->karthud[khud_boostcam] < player->karthud[khud_destboostcam])
				player->karthud[khud_boostcam] = player->karthud[khud_destboostcam] = 0;
		}
		//CONS_Printf("cam: %d, dest: %d\n", player->karthud[khud_boostcam], player->karthud[khud_destboostcam]);
	}

	player->karthud[khud_timeovercam] = 0;

	// Specific hack because it insists on setting flashing tics during this anyway...
	if (( player->kartstuff[k_spinouttype] & KSPIN_IFRAMES ) == 0)
	{
		player->powers[pw_flashing] = 0;
	}
	// Make ABSOLUTELY SURE that your flashing tics don't get set WHILE you're still in hit animations.
	else if (player->kartstuff[k_spinouttimer] != 0 || player->kartstuff[k_wipeoutslow] != 0)
	{
		player->powers[pw_flashing] = K_GetKartFlashing(player);
	}

	if (player->kartstuff[k_spinouttimer])
	{
		if ((P_IsObjectOnGround(player->mo)
			|| ( player->kartstuff[k_spinouttype] & KSPIN_AIRTIMER ))
			&& (!player->kartstuff[k_sneakertimer]))
		{
			player->kartstuff[k_spinouttimer]--;
			if (player->kartstuff[k_wipeoutslow] > 1)
				player->kartstuff[k_wipeoutslow]--;
		}
	}
	else
	{
		if (player->kartstuff[k_wipeoutslow] >= 1)
			player->mo->friction = ORIG_FRICTION;
		player->kartstuff[k_wipeoutslow] = 0;
	}

	if (player->rings > 20)
		player->rings = 20;
	else if (player->rings < -20)
		player->rings = -20;

	if ((gametyperules & GTR_BUMPERS) && (player->bumpers <= 0))
	{
		// Deplete 1 every tic when removed from the game.
		player->spheres--;
	}
	else
	{
		// Deplete 1 every second when playing.
		if ((leveltime % TICRATE) == 0)
			player->spheres--;
	}

	if (player->spheres > 40)
		player->spheres = 40;
	else if (player->spheres < 0)
		player->spheres = 0;

	if (comeback == false || !(gametyperules & GTR_KARMA) || player->eliminated == true)
	{
		player->karmadelay = comebacktime;
	}
	else if (player->karmadelay > 0 && !P_PlayerInPain(player))
	{
		player->karmadelay--;
		if (P_IsDisplayPlayer(player) && player->bumpers <= 0 && player->karmadelay <= 0)
			comebackshowninfo = true; // client has already seen the message
	}

	if (player->kartstuff[k_ringdelay])
		player->kartstuff[k_ringdelay]--;

	if (P_PlayerInPain(player))
		player->kartstuff[k_ringboost] = 0;
	else if (player->kartstuff[k_ringboost])
		player->kartstuff[k_ringboost]--;

	if (player->kartstuff[k_sneakertimer])
	{
		player->kartstuff[k_sneakertimer]--;

		if (player->kartstuff[k_sneakertimer] <= 0)
		{
			player->kartstuff[k_numsneakers] = 0;
		}
	}

	if (player->kartstuff[k_flamedash])
		player->kartstuff[k_flamedash]--;

	if (player->kartstuff[k_sneakertimer] && player->kartstuff[k_wipeoutslow] > 0 && player->kartstuff[k_wipeoutslow] < wipeoutslowtime+1)
		player->kartstuff[k_wipeoutslow] = wipeoutslowtime+1;

	if (player->kartstuff[k_floorboost])
		player->kartstuff[k_floorboost]--;

	if (player->kartstuff[k_driftboost])
		player->kartstuff[k_driftboost]--;

	if (player->kartstuff[k_startboost])
		player->kartstuff[k_startboost]--;

	if (player->kartstuff[k_spindashboost])
	{
		player->kartstuff[k_spindashboost]--;

		if (player->kartstuff[k_spindashboost] <= 0)
		{
			player->kartstuff[k_spindashspeed] = player->kartstuff[k_spindashboost] = 0;
		}
	}

	if (player->kartstuff[k_invincibilitytimer])
		player->kartstuff[k_invincibilitytimer]--;

	if ((player->respawn.state == RESPAWNST_NONE) && player->kartstuff[k_growshrinktimer] != 0)
	{
		if (player->kartstuff[k_growshrinktimer] > 0)
			player->kartstuff[k_growshrinktimer]--;
		if (player->kartstuff[k_growshrinktimer] < 0)
			player->kartstuff[k_growshrinktimer]++;

		// Back to normal
		if (player->kartstuff[k_growshrinktimer] == 0)
			K_RemoveGrowShrink(player);
	}

	if (player->kartstuff[k_superring])
	{
		if (player->kartstuff[k_superring] % 3 == 0)
		{
			mobj_t *ring = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_RING);
			ring->extravalue1 = 1; // Ring collect animation timer
			ring->angle = player->mo->angle; // animation angle
			P_SetTarget(&ring->target, player->mo); // toucher for thinker
			player->kartstuff[k_pickuprings]++;
			if (player->kartstuff[k_superring] <= 3)
				ring->cvmem = 1; // play caching when collected
		}
		player->kartstuff[k_superring]--;
	}

	if (player->kartstuff[k_stealingtimer] == 0 && player->kartstuff[k_stolentimer] == 0
		&& player->kartstuff[k_rocketsneakertimer])
		player->kartstuff[k_rocketsneakertimer]--;

	if (player->kartstuff[k_hyudorotimer])
		player->kartstuff[k_hyudorotimer]--;

	if (player->kartstuff[k_sadtimer])
		player->kartstuff[k_sadtimer]--;

	if (player->kartstuff[k_stealingtimer])
		player->kartstuff[k_stealingtimer]--;

	if (player->kartstuff[k_stolentimer])
		player->kartstuff[k_stolentimer]--;
	
	if (player->kartstuff[k_squishedtimer])
		player->kartstuff[k_squishedtimer]--;

	if (player->kartstuff[k_justbumped] > 0)
		player->kartstuff[k_justbumped]--;

	// This doesn't go in HUD update because it has potential gameplay ramifications
	if (player->karthud[khud_itemblink] && player->karthud[khud_itemblink]-- <= 0)
	{
		player->karthud[khud_itemblinkmode] = 0;
		player->karthud[khud_itemblink] = 0;
	}

	K_KartPlayerHUDUpdate(player);

	if ((gametyperules & GTR_WANTED) && player->bumpers > 0 && !P_PlayerInPain(player) && !player->powers[pw_flashing])
	{
		player->kartstuff[k_wanted]++;
	}

	if ((battleovertime.enabled >= 10*TICRATE) && (player->eliminated == false))
	{
		fixed_t distanceToBarrier = 0;

		if (battleovertime.radius > 0)
		{
			distanceToBarrier = R_PointToDist2(player->mo->x, player->mo->y, battleovertime.x, battleovertime.y) - (player->mo->radius * 2);
		}

		if (distanceToBarrier > battleovertime.radius)
		{
			P_DamageMobj(player->mo, NULL, NULL, 1, DMG_TIMEOVER);
		}
	}

	if (P_IsObjectOnGround(player->mo))
		player->kartstuff[k_waterskip] = 0;

	if (player->kartstuff[k_instashield])
		player->kartstuff[k_instashield]--;

	if (player->kartstuff[k_eggmanexplode])
	{
		if (player->spectator || (gametype == GT_BATTLE && !player->bumpers))
			player->kartstuff[k_eggmanexplode] = 0;
		else
		{
			player->kartstuff[k_eggmanexplode]--;
			if (player->kartstuff[k_eggmanexplode] <= 0)
			{
				mobj_t *eggsexplode;
				//player->powers[pw_flashing] = 0;
				eggsexplode = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_SPBEXPLOSION);
				if (player->kartstuff[k_eggmanblame] >= 0
				&& player->kartstuff[k_eggmanblame] < MAXPLAYERS
				&& playeringame[player->kartstuff[k_eggmanblame]]
				&& !players[player->kartstuff[k_eggmanblame]].spectator
				&& players[player->kartstuff[k_eggmanblame]].mo)
					P_SetTarget(&eggsexplode->target, players[player->kartstuff[k_eggmanblame]].mo);
			}
		}
	}

	if (player->kartstuff[k_itemtype] == KITEM_THUNDERSHIELD)
	{
		if (RINGTOTAL(player) < 20 && !player->kartstuff[k_ringlock])
			K_LookForRings(player->mo);
	}

	if (player->kartstuff[k_itemtype] == KITEM_BUBBLESHIELD)
	{
		if (player->kartstuff[k_bubblecool])
			player->kartstuff[k_bubblecool]--;
	}
	else
	{
		player->kartstuff[k_bubbleblowup] = 0;
		player->kartstuff[k_bubblecool] = 0;
	}

	if (player->kartstuff[k_itemtype] != KITEM_FLAMESHIELD)
	{
		if (player->kartstuff[k_flamedash])
			K_FlameDashLeftoverSmoke(player->mo);
	}

	if (player->karmadelay)
		player->kartstuff[k_comebackmode] = 0;

	if (P_IsObjectOnGround(player->mo) && player->kartstuff[k_pogospring])
	{
		if (P_MobjFlip(player->mo)*player->mo->momz <= 0)
			player->kartstuff[k_pogospring] = 0;
	}
	
	K_PogoSidemove(player);

	if (cmd->buttons & BT_DRIFT)
		player->kartstuff[k_jmp] = 1;
	else
		player->kartstuff[k_jmp] = 0;

	// Roulette Code
	K_KartItemRoulette(player, cmd);

	// Handle invincibility sfx
	K_UpdateInvincibilitySounds(player); // Also thanks, VAda!

	// Plays the music after the starting countdown.
	if (P_IsLocalPlayer(player) && leveltime == (starttime + (TICRATE/2)))
	{
		S_ChangeMusic(mapmusname, mapmusflags, true);
		S_ShowMusicCredit();
	}
}

void K_KartPlayerAfterThink(player_t *player)
{
	if (player->kartstuff[k_curshield]
		|| player->kartstuff[k_invincibilitytimer]
		|| (player->kartstuff[k_growshrinktimer] != 0 && player->kartstuff[k_growshrinktimer] % 5 == 4)) // 4 instead of 0 because this is afterthink!
	{
		player->mo->frame |= FF_FULLBRIGHT;
	}
	else
	{
		if (!(player->mo->state->frame & FF_FULLBRIGHT))
			player->mo->frame &= ~FF_FULLBRIGHT;
	}

	// Move held objects (Bananas, Orbinaut, etc)
	K_MoveHeldObjects(player);

	// Jawz reticule (seeking)
	if (player->kartstuff[k_itemtype] == KITEM_JAWZ && player->kartstuff[k_itemheld])
	{
		INT32 lasttarg = player->kartstuff[k_lastjawztarget];
		player_t *targ;
		mobj_t *ret;

		if (player->kartstuff[k_jawztargetdelay] && playeringame[lasttarg] && !players[lasttarg].spectator)
		{
			targ = &players[lasttarg];
			player->kartstuff[k_jawztargetdelay]--;
		}
		else
			targ = K_FindJawzTarget(player->mo, player);

		if (!targ || !targ->mo || P_MobjWasRemoved(targ->mo))
		{
			player->kartstuff[k_lastjawztarget] = -1;
			player->kartstuff[k_jawztargetdelay] = 0;
			return;
		}

		ret = P_SpawnMobj(targ->mo->x, targ->mo->y, targ->mo->z, MT_PLAYERRETICULE);
		P_SetTarget(&ret->target, targ->mo);
		ret->frame |= ((leveltime % 10) / 2);
		ret->tics = 1;
		ret->color = player->skincolor;

		if (targ-players != lasttarg)
		{
			if (P_IsDisplayPlayer(player) || P_IsDisplayPlayer(targ))
				S_StartSound(NULL, sfx_s3k89);
			else
				S_StartSound(targ->mo, sfx_s3k89);

			player->kartstuff[k_lastjawztarget] = targ-players;
			player->kartstuff[k_jawztargetdelay] = 5;
		}
	}
	else
	{
		player->kartstuff[k_lastjawztarget] = -1;
		player->kartstuff[k_jawztargetdelay] = 0;
	}
}

/*--------------------------------------------------
	static waypoint_t *K_GetPlayerNextWaypoint(player_t *player)

		Gets the next waypoint of a player, by finding their closest waypoint, then checking which of itself and next or
		previous waypoints are infront of the player.

	Input Arguments:-
		player - The player the next waypoint is being found for

	Return:-
		The waypoint that is the player's next waypoint
--------------------------------------------------*/
static waypoint_t *K_GetPlayerNextWaypoint(player_t *player)
{
	waypoint_t *bestwaypoint = NULL;

	if ((player != NULL) && (player->mo != NULL) && (P_MobjWasRemoved(player->mo) == false))
	{
		waypoint_t *waypoint     = K_GetBestWaypointForMobj(player->mo);
		boolean    updaterespawn = false;

		bestwaypoint = waypoint;

		// check the waypoint's location in relation to the player
		// If it's generally in front, it's fine, otherwise, use the best next/previous waypoint.
		// EXCEPTION: If our best waypoint is the finishline AND we're facing towards it, don't do this.
		// Otherwise it breaks the distance calculations.
		if (waypoint != NULL)
		{
			boolean finishlinehack  = false;
			angle_t playerangle     = player->mo->angle;
			angle_t momangle        = K_MomentumAngle(player->mo);
			angle_t angletowaypoint =
				R_PointToAngle2(player->mo->x, player->mo->y, waypoint->mobj->x, waypoint->mobj->y);
			angle_t angledelta      = ANGLE_MAX;
			angle_t momdelta        = ANGLE_MAX;

			angledelta = playerangle - angletowaypoint;
			if (angledelta > ANGLE_180)
			{
				angledelta = InvAngle(angledelta);
			}

			momdelta = momangle - angletowaypoint;
			if (momdelta > ANGLE_180)
			{
				momdelta = InvAngle(momdelta);
			}

			if (bestwaypoint == K_GetFinishLineWaypoint())
			{
				// facing towards the finishline
				if (angledelta <= ANGLE_90)
				{
					finishlinehack = true;
				}
			}

			// We're using a lot of angle calculations here, because only using facing angle or only using momentum angle both have downsides.
			// nextwaypoints will be picked if you're facing OR moving forward.
			// prevwaypoints will be picked if you're facing AND moving backward.
			if ((angledelta > ANGLE_45 || momdelta > ANGLE_45)
			&& (finishlinehack == false))
			{
				angle_t nextbestdelta = angledelta;
				angle_t nextbestmomdelta = momdelta;
				size_t i = 0U;

				if (K_PlayerUsesBotMovement(player))
				{
					// Try to force bots to use a next waypoint
					nextbestdelta = ANGLE_MAX;
					nextbestmomdelta = ANGLE_MAX;
				}

				if ((waypoint->nextwaypoints != NULL) && (waypoint->numnextwaypoints > 0U))
				{
					for (i = 0U; i < waypoint->numnextwaypoints; i++)
					{
						if (K_PlayerUsesBotMovement(player) == true
						&& K_GetWaypointIsShortcut(waypoint->nextwaypoints[i]) == true
						&& K_BotCanTakeCut(player) == false)
						{
							// Bots that aren't able to take a shortcut will ignore shortcut waypoints.
							// (However, if they're already on a shortcut, then we want them to keep going.)

							if (player->nextwaypoint == NULL
							|| K_GetWaypointIsShortcut(player->nextwaypoint) == false)
							{
								continue;
							}
						}

						angletowaypoint = R_PointToAngle2(
							player->mo->x, player->mo->y,
							waypoint->nextwaypoints[i]->mobj->x, waypoint->nextwaypoints[i]->mobj->y);

						angledelta = playerangle - angletowaypoint;
						if (angledelta > ANGLE_180)
						{
							angledelta = InvAngle(angledelta);
						}

						momdelta = momangle - angletowaypoint;
						if (momdelta > ANGLE_180)
						{
							momdelta = InvAngle(momdelta);
						}

						if (angledelta < nextbestdelta || momdelta < nextbestmomdelta)
						{
							bestwaypoint = waypoint->nextwaypoints[i];

							if (angledelta < nextbestdelta)
							{
								nextbestdelta = angledelta;
							}
							if (momdelta < nextbestmomdelta)
							{
								nextbestmomdelta = momdelta;
							}

							// Remove wrong way flag if we're using nextwaypoints
							player->kartstuff[k_wrongway] = 0;
							updaterespawn = true;
						}
					}
				}

				if ((waypoint->prevwaypoints != NULL) && (waypoint->numprevwaypoints > 0U)
				&& !(K_PlayerUsesBotMovement(player))) // Bots do not need prev waypoints
				{
					for (i = 0U; i < waypoint->numprevwaypoints; i++)
					{
						angletowaypoint = R_PointToAngle2(
							player->mo->x, player->mo->y,
							waypoint->prevwaypoints[i]->mobj->x, waypoint->prevwaypoints[i]->mobj->y);

						angledelta = playerangle - angletowaypoint;
						if (angledelta > ANGLE_180)
						{
							angledelta = InvAngle(angledelta);
						}

						momdelta = momangle - angletowaypoint;
						if (momdelta > ANGLE_180)
						{
							momdelta = InvAngle(momdelta);
						}

						if (angledelta < nextbestdelta && momdelta < nextbestmomdelta)
						{
							bestwaypoint = waypoint->prevwaypoints[i];

							nextbestdelta = angledelta;
							nextbestmomdelta = momdelta;

							// Set wrong way flag if we're using prevwaypoints
							player->kartstuff[k_wrongway] = 1;
							updaterespawn = false;
						}
					}
				}
			}
		}

		if (!P_IsObjectOnGround(player->mo))
		{
			updaterespawn = false;
		}

		// Respawn point should only be updated when we're going to a nextwaypoint
		if ((updaterespawn) &&
		(player->respawn.state == RESPAWNST_NONE) &&
		(bestwaypoint != NULL) &&
		(bestwaypoint != player->nextwaypoint) &&
		(K_GetWaypointIsSpawnpoint(bestwaypoint)) &&
		(K_GetWaypointIsEnabled(bestwaypoint) == true))
		{
			player->respawn.wp = bestwaypoint;
		}
	}

	return bestwaypoint;
}

static boolean K_PlayerCloserToNextWaypoints(waypoint_t *const waypoint, player_t *const player)
{
	boolean nextiscloser = true;

	if ((waypoint != NULL) && (player != NULL) && (player->mo != NULL))
	{
		size_t     i                   = 0U;
		waypoint_t *currentwpcheck     = NULL;
		angle_t    angletoplayer       = ANGLE_MAX;
		angle_t    currentanglecheck   = ANGLE_MAX;
		angle_t    bestangle           = ANGLE_MAX;

		angletoplayer = R_PointToAngle2(waypoint->mobj->x, waypoint->mobj->y,
			player->mo->x, player->mo->y);

		for (i = 0U; i < waypoint->numnextwaypoints; i++)
		{
			currentwpcheck = waypoint->nextwaypoints[i];
			currentanglecheck = R_PointToAngle2(
				waypoint->mobj->x, waypoint->mobj->y, currentwpcheck->mobj->x, currentwpcheck->mobj->y);

			// Get delta angle
			currentanglecheck = currentanglecheck - angletoplayer;

			if (currentanglecheck > ANGLE_180)
			{
				currentanglecheck = InvAngle(currentanglecheck);
			}

			if (currentanglecheck < bestangle)
			{
				bestangle = currentanglecheck;
			}
		}

		for (i = 0U; i < waypoint->numprevwaypoints; i++)
		{
			currentwpcheck = waypoint->prevwaypoints[i];
			currentanglecheck = R_PointToAngle2(
				waypoint->mobj->x, waypoint->mobj->y, currentwpcheck->mobj->x, currentwpcheck->mobj->y);

			// Get delta angle
			currentanglecheck = currentanglecheck - angletoplayer;

			if (currentanglecheck > ANGLE_180)
			{
				currentanglecheck = InvAngle(currentanglecheck);
			}

			if (currentanglecheck < bestangle)
			{
				bestangle = currentanglecheck;
				nextiscloser = false;
				break;
			}
		}
	}

	return nextiscloser;
}

/*--------------------------------------------------
	void K_UpdateDistanceFromFinishLine(player_t *const player)

		Updates the distance a player has to the finish line.

	Input Arguments:-
		player - The player the distance is being updated for

	Return:-
		None
--------------------------------------------------*/
void K_UpdateDistanceFromFinishLine(player_t *const player)
{
	if ((player != NULL) && (player->mo != NULL))
	{
		waypoint_t *finishline   = K_GetFinishLineWaypoint();
		waypoint_t *nextwaypoint = NULL;

		if (player->spectator)
		{
			// Don't update waypoints while spectating
			nextwaypoint = finishline;
		}
		else
		{
			nextwaypoint = K_GetPlayerNextWaypoint(player);
		}

		if (nextwaypoint != NULL)
		{
			// If nextwaypoint is NULL, it means we don't want to update the waypoint until we touch another one.
			// player->nextwaypoint will keep its previous value in this case.
			player->nextwaypoint = nextwaypoint;
		}

		// nextwaypoint is now the waypoint that is in front of us
		if (player->exiting || player->spectator)
		{
			// Player has finished, we don't need to calculate this
			player->distancetofinish = 0U;
		}
		else if ((player->nextwaypoint != NULL) && (finishline != NULL))
		{
			const boolean useshortcuts = false;
			const boolean huntbackwards = false;
			boolean pathfindsuccess = false;
			path_t pathtofinish = {};

			pathfindsuccess =
				K_PathfindToWaypoint(player->nextwaypoint, finishline, &pathtofinish, useshortcuts, huntbackwards);

			// Update the player's distance to the finish line if a path was found.
			// Using shortcuts won't find a path, so distance won't be updated until the player gets back on track
			if (pathfindsuccess == true)
			{
				// Add euclidean distance to the next waypoint to the distancetofinish
				UINT32 adddist;
				fixed_t disttowaypoint =
					P_AproxDistance(
						(player->mo->x >> FRACBITS) - (player->nextwaypoint->mobj->x >> FRACBITS),
						(player->mo->y >> FRACBITS) - (player->nextwaypoint->mobj->y >> FRACBITS));
				disttowaypoint = P_AproxDistance(disttowaypoint, (player->mo->z >> FRACBITS) - (player->nextwaypoint->mobj->z >> FRACBITS));

				adddist = (UINT32)disttowaypoint;

				player->distancetofinish = pathtofinish.totaldist + adddist;
				Z_Free(pathtofinish.array);

				// distancetofinish is currently a flat distance to the finish line, but in order to be fully
				// correct we need to add to it the length of the entire circuit multiplied by the number of laps
				// left after this one. This will give us the total distance to the finish line, and allow item
				// distance calculation to work easily
				if ((mapheaderinfo[gamemap - 1]->levelflags & LF_SECTIONRACE) == 0U)
				{
					const UINT8 numfulllapsleft = ((UINT8)cv_numlaps.value - player->laps);

					player->distancetofinish += numfulllapsleft * K_GetCircuitLength();

					// An additional HACK, to fix looking backwards towards the finish line
					// If the player's next waypoint is the finishline and the angle distance from player to
					// connectin waypoints implies they're closer to a next waypoint, add a full track distance
					if (player->nextwaypoint == finishline)
					{
						if (K_PlayerCloserToNextWaypoints(player->nextwaypoint, player) == true)
						{
							player->distancetofinish += K_GetCircuitLength();
						}
					}
				}
			}
		}
	}
}

INT32 K_GetKartRingPower(player_t *player)
{
	return (((9 - player->kartspeed) + (9 - player->kartweight)) / 2);
}

// Returns false if this player being placed here causes them to collide with any other player
// Used in g_game.c for match etc. respawning
// This does not check along the z because the z is not correctly set for the spawnee at this point
boolean K_CheckPlayersRespawnColliding(INT32 playernum, fixed_t x, fixed_t y)
{
	INT32 i;
	fixed_t p1radius = players[playernum].mo->radius;
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playernum == i || !playeringame[i] || players[i].spectator || !players[i].mo || players[i].mo->health <= 0
			|| players[i].playerstate != PST_LIVE || (players[i].mo->flags & MF_NOCLIP) || (players[i].mo->flags & MF_NOCLIPTHING))
			continue;

		if (abs(x - players[i].mo->x) < (p1radius + players[i].mo->radius)
			&& abs(y - players[i].mo->y) < (p1radius + players[i].mo->radius))
		{
			return false;
		}
	}
	return true;
}

// countersteer is how strong the controls are telling us we are turning
// turndir is the direction the controls are telling us to turn, -1 if turning right and 1 if turning left
static INT16 K_GetKartDriftValue(player_t *player, fixed_t countersteer)
{
	INT16 basedrift, driftadjust;
	fixed_t driftweight = player->kartweight*14; // 12

	if (player->kartstuff[k_drift] == 0 || !P_IsObjectOnGround(player->mo))
	{
		// If they aren't drifting or on the ground, this doesn't apply
		return 0;
	}

	if (player->kartstuff[k_driftend] != 0)
	{
		// Drift has ended and we are tweaking their angle back a bit
		return -266*player->kartstuff[k_drift];
	}

	basedrift = (83 * player->kartstuff[k_drift]) - (((driftweight - 14) * player->kartstuff[k_drift]) / 5); // 415 - 303
	driftadjust = abs((252 - driftweight) * player->kartstuff[k_drift] / 5);

	return basedrift + (FixedMul(driftadjust * FRACUNIT, countersteer) / FRACUNIT);
}

INT16 K_GetKartTurnValue(player_t *player, INT16 turnvalue)
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

	currentSpeed = R_PointToDist2(0, 0, player->mo->momx, player->mo->momy);

	if ((currentSpeed <= 0) // Not moving
	&& ((player->cmd.buttons & BT_EBRAKEMASK) != BT_EBRAKEMASK) // not e-braking
	&& (player->respawn.state == RESPAWNST_NONE)) // Not respawning
	{
		return 0;
	}

	p_maxspeed = K_GetKartSpeed(player, false);
	p_speed = min(FixedHypot(player->mo->momx, player->mo->momy), (p_maxspeed * 2));
	weightadjust = FixedDiv((p_maxspeed * 3) - p_speed, (p_maxspeed * 3) + (player->kartweight * FRACUNIT));

	if (K_PlayerUsesBotMovement(player))
	{
		turnfixed = FixedMul(turnfixed, 5*FRACUNIT/4); // Base increase to turning
		turnfixed = FixedMul(turnfixed, K_BotRubberband(player));
	}

	if (player->kartstuff[k_drift] != 0 && P_IsObjectOnGround(player->mo))
	{
		if (player->kartstuff[k_driftend] != 0)
		{
			// Sal: K_GetKartDriftValue is short-circuited to give a weird additive magic number,
			// instead of an entirely replaced turn value. This gaslit me years ago when I was doing a
			// code readability pass, where I missed that fact because it also returned early.
			turnfixed += K_GetKartDriftValue(player, FRACUNIT) * FRACUNIT;
			return (turnfixed / FRACUNIT);

		}
		else
		{
			// If we're drifting we have a completely different turning value
			fixed_t countersteer = FixedDiv(turnfixed, KART_FULLTURN * FRACUNIT);
			return K_GetKartDriftValue(player, countersteer);

		}

	}

	if (player->kartstuff[k_handleboost] > 0)
	{
		turnfixed = FixedMul(turnfixed, FRACUNIT + player->kartstuff[k_handleboost]);
	}

	// Weight has a small effect on turning
	turnfixed = FixedMul(turnfixed, weightadjust);

	return (turnfixed / FRACUNIT);
}

INT32 K_GetKartDriftSparkValue(player_t *player)
{
	return (26*4 + player->kartspeed*2 + (9 - player->kartweight))*8;
}

/*
Stage 1: red sparks
Stage 2: blue sparks
Stage 3: big large rainbow sparks
*/
void K_SpawnDriftBoostExplosion(player_t *player, int stage)
{
	mobj_t *overlay = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_DRIFTEXPLODE);

	P_SetTarget(&overlay->target, player->mo);
	P_SetScale(overlay, (overlay->destscale = player->mo->scale));
	K_FlipFromObject(overlay, player->mo);

	switch (stage)
	{
		case 1:
			overlay->color = SKINCOLOR_KETCHUP;
			overlay->fuse = 16;
			break;

		case 2:
			overlay->color = SKINCOLOR_SAPPHIRE;
			overlay->fuse = 32;

			S_StartSound(player->mo, sfx_kc5b);
			break;

		case 3:
			overlay->color = SKINCOLOR_SILVER;
			overlay->fuse = 120;

			S_StartSound(player->mo, sfx_kc5b);
			S_StartSound(player->mo, sfx_s3kc4l);
			break;
	}

	overlay->extravalue1 = stage;
}

static void K_KartDrift(player_t *player, boolean onground)
{
	fixed_t minspeed = (10 * player->mo->scale);
	INT32 dsone = K_GetKartDriftSparkValue(player);
	INT32 dstwo = dsone*2;
	INT32 dsthree = dstwo*2;

	// Drifting is actually straffing + automatic turning.
	// Holding the Jump button will enable drifting.

	// Drift Release (Moved here so you can't "chain" drifts)
	if (player->kartstuff[k_drift] != -5 && player->kartstuff[k_drift] != 5)
	{
		if (player->kartstuff[k_driftcharge] < 0 || player->kartstuff[k_driftcharge] >= dsone)
		{
			angle_t pushdir = K_MomentumAngle(player->mo);

			S_StartSound(player->mo, sfx_s23c);
			//K_SpawnDashDustRelease(player);

			if (player->kartstuff[k_driftcharge] < 0)
			{
				// Stage 0: Yellow sparks
				if (!onground)
					P_Thrust(player->mo, pushdir, player->speed / 8);

				if (player->kartstuff[k_driftboost] < 15)
					player->kartstuff[k_driftboost] = 15;
			}
			else if (player->kartstuff[k_driftcharge] >= dsone && player->kartstuff[k_driftcharge] < dstwo)
			{
				// Stage 1: Red sparks
				if (!onground)
					P_Thrust(player->mo, pushdir, player->speed / 4);

				if (player->kartstuff[k_driftboost] < 20)
					player->kartstuff[k_driftboost] = 20;

				K_SpawnDriftBoostExplosion(player, 1);
			}
			else if (player->kartstuff[k_driftcharge] < dsthree)
			{
				// Stage 2: Blue sparks
				if (!onground)
					P_Thrust(player->mo, pushdir, player->speed / 3);

				if (player->kartstuff[k_driftboost] < 50)
					player->kartstuff[k_driftboost] = 50;

				K_SpawnDriftBoostExplosion(player, 2);
			}
			else if (player->kartstuff[k_driftcharge] >= dsthree)
			{
				// Stage 3: Rainbow sparks
				if (!onground)
					P_Thrust(player->mo, pushdir, player->speed / 2);

				if (player->kartstuff[k_driftboost] < 125)
					player->kartstuff[k_driftboost] = 125;

				K_SpawnDriftBoostExplosion(player, 3);
			}
		}

		// Remove charge
		player->kartstuff[k_driftcharge] = 0;
	}

	// Drifting: left or right?
	if ((player->cmd.turning > 0) && player->speed > minspeed && player->kartstuff[k_jmp] == 1
		&& (player->kartstuff[k_drift] == 0 || player->kartstuff[k_driftend] == 1)) // && player->kartstuff[k_drift] != 1)
	{
		// Starting left drift
		player->kartstuff[k_drift] = 1;
		player->kartstuff[k_driftend] = player->kartstuff[k_driftcharge] = 0;
	}
	else if ((player->cmd.turning < 0) && player->speed > minspeed && player->kartstuff[k_jmp] == 1
		&& (player->kartstuff[k_drift] == 0 || player->kartstuff[k_driftend] == 1)) // && player->kartstuff[k_drift] != -1)
	{
		// Starting right drift
		player->kartstuff[k_drift] = -1;
		player->kartstuff[k_driftend] = player->kartstuff[k_driftcharge] = 0;
	}
	else if (player->kartstuff[k_jmp] == 0) // || player->kartstuff[k_turndir] == 0)
	{
		// drift is not being performed so if we're just finishing set driftend and decrement counters
		if (player->kartstuff[k_drift] > 0)
		{
			player->kartstuff[k_drift]--;
			player->kartstuff[k_driftend] = 1;
		}
		else if (player->kartstuff[k_drift] < 0)
		{
			player->kartstuff[k_drift]++;
			player->kartstuff[k_driftend] = 1;
		}
		else
			player->kartstuff[k_driftend] = 0;
	}

	if (P_PlayerInPain(player) || player->speed <= 0)
	{
		// Stop drifting
		player->kartstuff[k_drift] = player->kartstuff[k_driftcharge] = 0;
		player->kartstuff[k_aizdriftstrat] = player->kartstuff[k_brakedrift] = 0;
		player->kartstuff[k_getsparks] = 0;
	}
	else if (player->kartstuff[k_jmp] == 1 && player->kartstuff[k_drift] != 0)
	{
		// Incease/decrease the drift value to continue drifting in that direction
		fixed_t driftadditive = 24;
		boolean playsound = false;

		if (onground)
		{
			if (player->kartstuff[k_drift] >= 1) // Drifting to the left
			{
				player->kartstuff[k_drift]++;
				if (player->kartstuff[k_drift] > 5)
					player->kartstuff[k_drift] = 5;

				if (player->cmd.turning > 0) // Inward
					driftadditive += abs(player->cmd.turning)/100;
				if (player->cmd.turning < 0) // Outward
					driftadditive -= abs(player->cmd.turning)/75;
			}
			else if (player->kartstuff[k_drift] <= -1) // Drifting to the right
			{
				player->kartstuff[k_drift]--;
				if (player->kartstuff[k_drift] < -5)
					player->kartstuff[k_drift] = -5;

				if (player->cmd.turning < 0) // Inward
					driftadditive += abs(player->cmd.turning)/100;
				if (player->cmd.turning > 0) // Outward
					driftadditive -= abs(player->cmd.turning)/75;
			}

			// Disable drift-sparks until you're going fast enough
			if (player->kartstuff[k_getsparks] == 0
				|| (player->kartstuff[k_offroad] && K_ApplyOffroad(player)))
				driftadditive = 0;

			// Inbetween minspeed and minspeed*2, it'll keep your previous drift-spark state.
			if (player->speed > minspeed*2)
			{
				player->kartstuff[k_getsparks] = 1;

				if (player->kartstuff[k_driftcharge] <= -1)
				{
					player->kartstuff[k_driftcharge] = dsone; // Back to red
					playsound = true;
				}
			}
			else if (player->speed <= minspeed)
			{
				player->kartstuff[k_getsparks] = 0;
				driftadditive = 0;

				if (player->kartstuff[k_driftcharge] >= dsone)
				{
					player->kartstuff[k_driftcharge] = -1; // Set yellow sparks
					playsound = true;
				}
			}
		}
		else
		{
			driftadditive = 0;
		}

		// This spawns the drift sparks
		if ((player->kartstuff[k_driftcharge] + driftadditive >= dsone)
			|| (player->kartstuff[k_driftcharge] < 0))
		{
			K_SpawnDriftSparks(player);
		}

		if ((player->kartstuff[k_driftcharge] < dsone && player->kartstuff[k_driftcharge]+driftadditive >= dsone)
			|| (player->kartstuff[k_driftcharge] < dstwo && player->kartstuff[k_driftcharge]+driftadditive >= dstwo)
			|| (player->kartstuff[k_driftcharge] < dsthree && player->kartstuff[k_driftcharge]+driftadditive >= dsthree))
		{
			playsound = true;
		}

		// Sound whenever you get a different tier of sparks
		if (playsound && P_IsDisplayPlayer(player))
		{
			if (player->kartstuff[k_driftcharge] == -1)
				S_StartSoundAtVolume(player->mo, sfx_sploss, 192); // Yellow spark sound
			else
				S_StartSoundAtVolume(player->mo, sfx_s3ka2, 192);
		}

		player->kartstuff[k_driftcharge] += driftadditive;
		player->kartstuff[k_driftend] = 0;
	}

	if ((player->kartstuff[k_handleboost] == 0)
	|| (!player->cmd.turning)
	|| (!player->kartstuff[k_aizdriftstrat])
	|| (player->cmd.turning > 0) != (player->kartstuff[k_aizdriftstrat] > 0))
	{
		if (!player->kartstuff[k_drift])
			player->kartstuff[k_aizdriftstrat] = 0;
		else
			player->kartstuff[k_aizdriftstrat] = ((player->kartstuff[k_drift] > 0) ? 1 : -1);
	}
	else if (player->kartstuff[k_aizdriftstrat] && !player->kartstuff[k_drift])
		K_SpawnAIZDust(player);

	if (player->kartstuff[k_drift]
		&& ((player->cmd.buttons & BT_BRAKE)
		|| !(player->cmd.buttons & BT_ACCELERATE))
		&& P_IsObjectOnGround(player->mo))
	{
		if (!player->kartstuff[k_brakedrift])
			K_SpawnBrakeDriftSparks(player);
		player->kartstuff[k_brakedrift] = 1;
	}
	else
		player->kartstuff[k_brakedrift] = 0;
}
//
// K_KartUpdatePosition
//
void K_KartLegacyUpdatePosition(player_t *player)
{
	fixed_t position = 1;
	fixed_t oldposition = player->kartstuff[k_position];
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
			if ((((players[i].starpostnum) + (numstarposts + 1) * players[i].laps) >
				((player->starpostnum) + (numstarposts + 1) * player->laps)))
				position++;
			else if (((players[i].starpostnum) + (numstarposts+1)*players[i].laps) ==
				((player->starpostnum) + (numstarposts+1)*player->laps))
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

					if (mo->health == player->starpostnum && (!mo->movecount || mo->movecount == player->laps+1))
					{
						player->prevcheck += pmo;
						ppcd++;
					}
					if (mo->health == (player->starpostnum + 1) && (!mo->movecount || mo->movecount == player->laps+1))
					{
						player->nextcheck += pmo;
						pncd++;
					}
					if (mo->health == players[i].starpostnum && (!mo->movecount || mo->movecount == players[i].laps+1))
					{
						players[i].prevcheck += imo;
						ipcd++;
					}
					if (mo->health == (players[i].starpostnum + 1) && (!mo->movecount || mo->movecount == players[i].laps+1))
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
					if (players[i].starposttime < player->starposttime)
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
		player->karthud[k_positiondelay]= 10; // Position number growth

	player->kartstuff[k_position] = position;
}

void K_KartUpdatePosition(player_t *player)
{
	fixed_t position = 1;
	fixed_t oldposition = player->kartstuff[k_position];
	fixed_t i;
	if (numbosswaypoints == 0)
	{
		if (player->spectator || !player->mo)
		{
			// Ensure these are reset for spectators
			player->kartstuff[k_position] = 0;
			player->kartstuff[k_positiondelay] = 0;
			return;
		}

		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i] || players[i].spectator || !players[i].mo)
				continue;

			if (gametyperules & GTR_CIRCUIT)
			{
				if (player->exiting) // End of match standings
				{
					// Only time matters
					if (players[i].realtime < player->realtime)
						position++;
				}
				else
				{
					// I'm a lap behind this player OR
					// My distance to the finish line is higher, so I'm behind
					if ((players[i].laps > player->laps)
						|| (players[i].distancetofinish < player->distancetofinish))
					{
						position++;
					}
				}
			}
			else
			{
				if (player->exiting) // End of match standings
				{
					// Only score matters
					if (players[i].marescore > player->marescore)
						position++;
				}
				else
				{
					UINT8 myEmeralds = K_NumEmeralds(player);
					UINT8 yourEmeralds = K_NumEmeralds(&players[i]);

					if (yourEmeralds > myEmeralds)
					{
						// Emeralds matter above all
						position++;
					}
					else if (yourEmeralds == myEmeralds)
					{
						// Bumpers are a tie breaker
						if (players[i].bumpers > player->bumpers)
						{
							position++;
						}
						else if (players[i].bumpers == player->bumpers)
						{
							// Score is the second tier tie breaker
							if (players[i].marescore > player->marescore)
							{
								position++;
							}
						}
					}
				}
			}
		}

		if (leveltime < starttime || oldposition == 0)
			oldposition = position;

		if (oldposition != position) // Changed places?
			player->kartstuff[k_positiondelay] = 10; // Position number growth

		player->kartstuff[k_position] = position;
	}
	else
		K_KartLegacyUpdatePosition(player);
}

//
// K_StripItems
//
void K_StripItems(player_t *player)
{
	K_DropRocketSneaker(player);
	K_DropKitchenSink(player);
	player->kartstuff[k_itemtype] = KITEM_NONE;
	player->kartstuff[k_itemamount] = 0;
	player->kartstuff[k_itemheld] = 0;

	if (!player->kartstuff[k_itemroulette] || player->kartstuff[k_roulettetype] != 2)
	{
		player->kartstuff[k_itemroulette] = 0;
		player->kartstuff[k_roulettetype] = 0;
	}
	player->kartstuff[k_eggmanheld] = 0;

	player->kartstuff[k_hyudorotimer] = 0;
	player->kartstuff[k_stealingtimer] = 0;
	player->kartstuff[k_stolentimer] = 0;

	player->kartstuff[k_curshield] = KSHIELD_NONE;
	player->kartstuff[k_bananadrag] = 0;

	player->kartstuff[k_sadtimer] = 0;

	K_UpdateHnextList(player, true);
}

void K_StripOther(player_t *player)
{
	player->kartstuff[k_itemroulette] = 0;
	player->kartstuff[k_roulettetype] = 0;

	player->kartstuff[k_invincibilitytimer] = 0;
	K_RemoveGrowShrink(player);

	if (player->kartstuff[k_eggmanexplode])
	{
		player->kartstuff[k_eggmanexplode] = 0;
		player->kartstuff[k_eggmanblame] = -1;
	}
}

static INT32 K_FlameShieldMax(player_t *player)
{
	UINT32 disttofinish = 0;
	UINT32 distv = DISTVAR;
	UINT8 numplayers = 0;
	UINT8 i;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && !players[i].spectator)
			numplayers++;
		if (players[i].kartstuff[k_position] == 1)
			disttofinish = players[i].distancetofinish;
	}

	if (numplayers <= 1)
	{
		return 16; // max when alone, for testing
	}
	else if (player->kartstuff[k_position] == 1)
	{
		return 0; // minimum for first
	}

	disttofinish = player->distancetofinish - disttofinish;
	distv = FixedMul(distv * FRACUNIT, mapobjectscale) / FRACUNIT;
	return min(16, 1 + (disttofinish / distv));
}

boolean K_PlayerEBrake(player_t *player)
{
	return (player->cmd.buttons & BT_EBRAKEMASK) == BT_EBRAKEMASK
	&& P_IsObjectOnGround(player->mo) == true
	&& player->kartstuff[k_drift] == 0
	&& player->kartstuff[k_spinouttimer] == 0
	&& player->kartstuff[k_justbumped] == 0
	&& player->kartstuff[k_spindash] >= 0
	&& player->kartstuff[k_spindashboost] == 0
	&& player->powers[pw_nocontrol] == 0;
}

static void K_KartSpindashDust(mobj_t *parent)
{
	fixed_t rad = FixedDiv(FixedHypot(parent->radius, parent->radius), parent->scale);
	INT32 i;

	for (i = 0; i < 2; i++)
	{
		fixed_t hmomentum = P_RandomRange(6, 12) * parent->scale;
		fixed_t vmomentum = P_RandomRange(2, 6) * parent->scale;

		angle_t ang = parent->player->drawangle + ANGLE_180;
		SINT8 flip = 1;

		mobj_t *dust;

		if (i & 1)
			ang -= ANGLE_45;
		else
			ang += ANGLE_45;

		dust = P_SpawnMobjFromMobj(parent,
			FixedMul(rad, FINECOSINE(ang >> ANGLETOFINESHIFT)),
			FixedMul(rad, FINESINE(ang >> ANGLETOFINESHIFT)),
			0, MT_SPINDASHDUST
		);
		flip = P_MobjFlip(dust);

		dust->momx = FixedMul(hmomentum, FINECOSINE(ang >> ANGLETOFINESHIFT));
		dust->momy = FixedMul(hmomentum, FINESINE(ang >> ANGLETOFINESHIFT));
		dust->momz = vmomentum * flip;
	}
}

static void K_KartSpindashWind(mobj_t *parent)
{
	mobj_t *wind = P_SpawnMobjFromMobj(parent,
		P_RandomRange(-36,36) * FRACUNIT,
		P_RandomRange(-36,36) * FRACUNIT,
		FixedDiv(parent->height / 2, parent->scale) + (P_RandomRange(-20,20) * FRACUNIT),
		MT_SPINDASHWIND
	);

	P_SetTarget(&wind->target, parent);

	if (parent->momx || parent->momy)
		wind->angle = R_PointToAngle2(0, 0, parent->momx, parent->momy);
	else
		wind->angle = parent->player->drawangle;

	wind->momx = 3 * parent->momx / 4;
	wind->momy = 3 * parent->momy / 4;
	wind->momz = 3 * parent->momz / 4;

	K_MatchGenericExtraFlags(wind, parent);
}

static void K_KartSpindash(player_t *player)
{
	const INT16 MAXCHARGETIME = K_GetSpindashChargeTime(player);
	ticcmd_t *cmd = &player->cmd;
	boolean spawnWind = (leveltime % 2 == 0);

	if (player->kartstuff[k_spindash] > 0 && (cmd->buttons & (BT_DRIFT|BT_BRAKE)) != (BT_DRIFT|BT_BRAKE))
	{
		player->kartstuff[k_spindashspeed] = (player->kartstuff[k_spindash] * FRACUNIT) / MAXCHARGETIME;
		player->kartstuff[k_spindashboost] = TICRATE;

		player->kartstuff[k_spindash] = 0;
		S_StartSound(player->mo, sfx_s23c);
	}


	if ((player->kartstuff[k_spindashboost] > 0) && (spawnWind == true))
	{
		K_KartSpindashWind(player->mo);
	}

	if (player->kartstuff[k_spindashboost] > (TICRATE/2))
	{
		K_KartSpindashDust(player->mo);
	}

	if (K_PlayerEBrake(player) == false)
	{
		player->kartstuff[k_spindash] = 0;
		return;
	}

	if (player->speed < 6*mapobjectscale && player->powers[pw_flashing] == 0)
	{
		if (cmd->turning != 0 && leveltime % 8 == 0)
			S_StartSound(player->mo, sfx_ruburn);

		if ((cmd->buttons & (BT_DRIFT|BT_BRAKE)) == (BT_DRIFT|BT_BRAKE))
		{
			INT16 chargetime = MAXCHARGETIME - ++player->kartstuff[k_spindash];
			boolean spawnOldEffect = true;

			if (chargetime <= (MAXCHARGETIME / 2))
			{
				K_KartSpindashDust(player->mo);
				spawnOldEffect = false;
			}

			if (chargetime <= (MAXCHARGETIME / 4) && spawnWind == true)
			{
				K_KartSpindashWind(player->mo);
			}

			if (chargetime > 0)
			{
				UINT16 soundcharge = 0;
				UINT8 add = 0;

				while ((soundcharge += ++add) < chargetime);

				if (soundcharge == chargetime)
				{
					if (spawnOldEffect == true)
						K_SpawnDashDustRelease(player);
					S_StartSound(player->mo, sfx_s3kab);
				}
			}
			else if (chargetime < -TICRATE)
			{
				P_DamageMobj(player->mo, NULL, NULL, 1, DMG_NORMAL);
			}
		}
	}
	else
	{
		if (leveltime % 4 == 0)
			S_StartSound(player->mo, sfx_kc2b);
	}
}

//
// K_AdjustPlayerFriction
//
void K_AdjustPlayerFriction(player_t *player)
{
	fixed_t prevfriction = player->mo->friction;

	if (P_IsObjectOnGround(player->mo) == false)
	{
		return;
	}
	
	if (player->kartstuff[k_pogospring])
	{
		return;
	}
	
	if (K_PlayerEBrake(player) == true)
	{
		player->mo->friction -= 2048;
	}
	else if (player->speed > 0 && player->cmd.forwardmove < 0)
	{
		player->mo->friction -= 2048;
	}

	// Wipeout slowdown
	if (player->kartstuff[k_spinouttimer] && player->kartstuff[k_wipeoutslow])
	{
		if (player->kartstuff[k_offroad])
			player->mo->friction -= 4912;
		if (player->kartstuff[k_wipeoutslow] == 1)
			player->mo->friction -= 9824;
	}

	// Cap between intended values
	if (player->mo->friction > FRACUNIT)
		player->mo->friction = FRACUNIT;
	if (player->mo->friction < 0)
		player->mo->friction = 0;

	// Friction was changed, so we must recalculate movefactor
	if (player->mo->friction != prevfriction)
	{
		player->mo->movefactor = FixedDiv(ORIG_FRICTION, player->mo->friction);

		if (player->mo->movefactor < FRACUNIT)
			player->mo->movefactor = 19*player->mo->movefactor - 18*FRACUNIT;
		else
			player->mo->movefactor = FRACUNIT;
	}

	// Don't go too far above your top speed when rubberbanding
	// Down here, because we do NOT want to modify movefactor
	if (K_PlayerUsesBotMovement(player))
	{
		player->mo->friction = K_BotFrictionRubberband(player, player->mo->friction);
	}
}

//
// K_MoveKartPlayer
//
void K_MoveKartPlayer(player_t *player, boolean onground)
{
	ticcmd_t *cmd = &player->cmd;
	boolean ATTACK_IS_DOWN = ((cmd->buttons & BT_ATTACK) && !(player->pflags & PF_ATTACKDOWN));
	boolean HOLDING_ITEM = (player->kartstuff[k_itemheld] || player->kartstuff[k_eggmanheld]);
	boolean NO_HYUDORO = (player->kartstuff[k_stolentimer] == 0 && player->kartstuff[k_stealingtimer] == 0);

	player->pflags &= ~PF_HITFINISHLINE;

	if (!player->exiting)
	{
		if (player->kartstuff[k_oldposition] < player->kartstuff[k_position]) // But first, if you lost a place,
		{
			player->kartstuff[k_oldposition] = player->kartstuff[k_position]; // then the other player taunts.
			K_RegularVoiceTimers(player); // and you can't for a bit
		}
		else if (player->kartstuff[k_oldposition] > player->kartstuff[k_position]) // Otherwise,
		{
			K_PlayOvertakeSound(player->mo); // Say "YOU'RE TOO SLOW!"
			player->kartstuff[k_oldposition] = player->kartstuff[k_position]; // Restore the old position,
		}
	}

	if (player->kartstuff[k_positiondelay])
		player->kartstuff[k_positiondelay]--;

	// Prevent ring misfire
	if (!(cmd->buttons & BT_ATTACK))
	{
		if (player->kartstuff[k_itemtype] == KITEM_NONE
			&& NO_HYUDORO && !(HOLDING_ITEM
			|| player->kartstuff[k_itemamount]
			|| player->kartstuff[k_itemroulette]
			|| player->kartstuff[k_rocketsneakertimer]
			|| player->kartstuff[k_eggmanexplode]))
			player->kartstuff[k_userings] = 1;
		else
			player->kartstuff[k_userings] = 0;
	}

	if ((player->pflags & PF_ATTACKDOWN) && !(cmd->buttons & BT_ATTACK))
		player->pflags &= ~PF_ATTACKDOWN;
	else if (cmd->buttons & BT_ATTACK)
		player->pflags |= PF_ATTACKDOWN;

	if (player && player->mo && player->mo->health > 0 && !player->spectator && !P_PlayerInPain(player) && !mapreset && leveltime > introtime)
	{
		// First, the really specific, finicky items that function without the item being directly in your item slot.
		// Karma item dropping
		if (player->kartstuff[k_comebackmode])
		{
			if (ATTACK_IS_DOWN)
			{
				mobj_t *newitem;

				if (player->kartstuff[k_comebackmode] == 1)
				{
					newitem = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_RANDOMITEM);
					newitem->threshold = 69; // selected "randomly".
				}
				else
				{
					newitem = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_EGGMANITEM);
					if (player->kartstuff[k_eggmanblame] >= 0
					&& player->kartstuff[k_eggmanblame] < MAXPLAYERS
					&& playeringame[player->kartstuff[k_eggmanblame]]
					&& !players[player->kartstuff[k_eggmanblame]].spectator
					&& players[player->kartstuff[k_eggmanblame]].mo)
						P_SetTarget(&newitem->target, players[player->kartstuff[k_eggmanblame]].mo);
					player->kartstuff[k_eggmanblame] = -1;
				}

				newitem->flags2 = (player->mo->flags2 & MF2_OBJECTFLIP);
				newitem->fuse = 15*TICRATE; // selected randomly.

				player->kartstuff[k_comebackmode] = 0;
				player->karmadelay = comebacktime;
				S_StartSound(player->mo, sfx_s254);
			}
		}
		else
		{
			// Ring boosting
			if (player->kartstuff[k_userings])
			{
				if ((player->pflags & PF_ATTACKDOWN) && !player->kartstuff[k_ringdelay] && player->rings > 0)
				{
					mobj_t *ring = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_RING);
					P_SetMobjState(ring, S_FASTRING1);
					ring->extravalue1 = 1; // Ring use animation timer
					ring->extravalue2 = 1; // Ring use animation flag
					ring->shadowscale = 0;
					P_SetTarget(&ring->target, player->mo); // user
					player->rings--;
					player->kartstuff[k_ringdelay] = 3;
				}
			}
			// Other items
			else
			{
				// Eggman Monitor exploding
				if (player->kartstuff[k_eggmanexplode])
				{
					if (ATTACK_IS_DOWN && player->kartstuff[k_eggmanexplode] <= 3*TICRATE && player->kartstuff[k_eggmanexplode] > 1)
						player->kartstuff[k_eggmanexplode] = 1;
				}
				// Eggman Monitor throwing
				else if (player->kartstuff[k_eggmanheld])
				{
					if (ATTACK_IS_DOWN)
					{
						K_ThrowKartItem(player, false, MT_EGGMANITEM, -1, 0);
						K_PlayAttackTaunt(player->mo);
						player->kartstuff[k_eggmanheld] = 0;
						K_UpdateHnextList(player, true);
					}
				}
				// Rocket Sneaker usage
				else if (player->kartstuff[k_rocketsneakertimer] > 1)
				{
					if (ATTACK_IS_DOWN && !HOLDING_ITEM && onground && NO_HYUDORO)
					{
						K_DoSneaker(player, 2);
						K_PlayBoostTaunt(player->mo);
						player->kartstuff[k_rocketsneakertimer] -= 3*TICRATE;
						if (player->kartstuff[k_rocketsneakertimer] < 1)
							player->kartstuff[k_rocketsneakertimer] = 1;
					}
				}
				else if (player->kartstuff[k_itemamount] <= 0)
				{
					player->kartstuff[k_itemamount] = player->kartstuff[k_itemheld] = 0;
				}
				else
				{
					switch (player->kartstuff[k_itemtype])
					{
						case KITEM_SNEAKER:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && onground && NO_HYUDORO)
							{
								K_DoSneaker(player, 1);
								K_PlayBoostTaunt(player->mo);
								player->kartstuff[k_itemamount]--;
							}
							break;
						case KITEM_ROCKETSNEAKER:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && onground && NO_HYUDORO
								&& player->kartstuff[k_rocketsneakertimer] == 0)
							{
								INT32 moloop;
								mobj_t *mo = NULL;
								mobj_t *prev = player->mo;

								K_PlayBoostTaunt(player->mo);
								//player->kartstuff[k_itemheld] = 1;
								S_StartSound(player->mo, sfx_s3k3a);

								//K_DoSneaker(player, 2);

								player->kartstuff[k_rocketsneakertimer] = (itemtime*3);
								player->kartstuff[k_itemamount]--;
								K_UpdateHnextList(player, true);

								for (moloop = 0; moloop < 2; moloop++)
								{
									mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_ROCKETSNEAKER);
									K_MatchGenericExtraFlags(mo, player->mo);
									mo->flags |= MF_NOCLIPTHING;
									mo->angle = player->mo->angle;
									mo->threshold = 10;
									mo->movecount = moloop%2;
									mo->movedir = mo->lastlook = moloop+1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&mo->hprev, prev);
									P_SetTarget(&prev->hnext, mo);
									prev = mo;
								}
							}
							break;
						case KITEM_INVINCIBILITY:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO) // Doesn't hold your item slot hostage normally, so you're free to waste it if you have multiple
							{
								if (!player->kartstuff[k_invincibilitytimer])
								{
									mobj_t *overlay = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_INVULNFLASH);
									P_SetTarget(&overlay->target, player->mo);
									overlay->destscale = player->mo->scale;
									P_SetScale(overlay, player->mo->scale);
								}
								player->kartstuff[k_invincibilitytimer] = itemtime+(2*TICRATE); // 10 seconds
								if (P_IsLocalPlayer(player))
									S_ChangeMusicSpecial("kinvnc");
								if (! P_IsDisplayPlayer(player))
									S_StartSound(player->mo, (cv_kartinvinsfx.value ? sfx_alarmg : sfx_kinvnc));
								P_RestoreMusic(player);
								K_PlayPowerGloatSound(player->mo);
								player->kartstuff[k_itemamount]--;
							}
							break;
						case KITEM_BANANA:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								INT32 moloop;
								mobj_t *mo;
								mobj_t *prev = player->mo;

								//K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemheld] = 1;
								S_StartSound(player->mo, sfx_s254);

								for (moloop = 0; moloop < player->kartstuff[k_itemamount]; moloop++)
								{
									mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_BANANA_SHIELD);
									if (!mo)
									{
										player->kartstuff[k_itemamount] = moloop;
										break;
									}
									mo->flags |= MF_NOCLIPTHING;
									mo->threshold = 10;
									mo->movecount = player->kartstuff[k_itemamount];
									mo->movedir = moloop+1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&mo->hprev, prev);
									P_SetTarget(&prev->hnext, mo);
									prev = mo;
								}
							}
							else if (ATTACK_IS_DOWN && player->kartstuff[k_itemheld]) // Banana x3 thrown
							{
								K_ThrowKartItem(player, false, MT_BANANA, -1, 0);
								K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemamount]--;
								K_UpdateHnextList(player, false);
							}
							break;
						case KITEM_EGGMAN:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								mobj_t *mo;
								player->kartstuff[k_itemamount]--;
								player->kartstuff[k_eggmanheld] = 1;
								S_StartSound(player->mo, sfx_s254);
								mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_EGGMANITEM_SHIELD);
								if (mo)
								{
									K_FlipFromObject(mo, player->mo);
									mo->flags |= MF_NOCLIPTHING;
									mo->threshold = 10;
									mo->movecount = 1;
									mo->movedir = 1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&player->mo->hnext, mo);
								}
							}
							break;
						case KITEM_ORBINAUT:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								angle_t newangle;
								INT32 moloop;
								mobj_t *mo = NULL;
								mobj_t *prev = player->mo;

								//K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemheld] = 1;
								S_StartSound(player->mo, sfx_s3k3a);

								for (moloop = 0; moloop < player->kartstuff[k_itemamount]; moloop++)
								{
									newangle = (player->mo->angle + ANGLE_157h) + FixedAngle(((360 / player->kartstuff[k_itemamount]) * moloop) << FRACBITS) + ANGLE_90;
									mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_ORBINAUT_SHIELD);
									if (!mo)
									{
										player->kartstuff[k_itemamount] = moloop;
										break;
									}
									mo->flags |= MF_NOCLIPTHING;
									mo->angle = newangle;
									mo->threshold = 10;
									mo->movecount = player->kartstuff[k_itemamount];
									mo->movedir = mo->lastlook = moloop+1;
									mo->color = player->skincolor;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&mo->hprev, prev);
									P_SetTarget(&prev->hnext, mo);
									prev = mo;
								}
							}
							else if (ATTACK_IS_DOWN && player->kartstuff[k_itemheld]) // Orbinaut x3 thrown
							{
								K_ThrowKartItem(player, true, MT_ORBINAUT, 1, 0);
								K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemamount]--;
								K_UpdateHnextList(player, false);
							}
							break;
						case KITEM_JAWZ:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								angle_t newangle;
								INT32 moloop;
								mobj_t *mo = NULL;
								mobj_t *prev = player->mo;

								//K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemheld] = 1;
								S_StartSound(player->mo, sfx_s3k3a);

								for (moloop = 0; moloop < player->kartstuff[k_itemamount]; moloop++)
								{
									newangle = (player->mo->angle + ANGLE_157h) + FixedAngle(((360 / player->kartstuff[k_itemamount]) * moloop) << FRACBITS) + ANGLE_90;
									mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_JAWZ_SHIELD);
									if (!mo)
									{
										player->kartstuff[k_itemamount] = moloop;
										break;
									}
									mo->flags |= MF_NOCLIPTHING;
									mo->angle = newangle;
									mo->threshold = 10;
									mo->movecount = player->kartstuff[k_itemamount];
									mo->movedir = mo->lastlook = moloop+1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&mo->hprev, prev);
									P_SetTarget(&prev->hnext, mo);
									prev = mo;
								}
							}
							else if (ATTACK_IS_DOWN && HOLDING_ITEM && player->kartstuff[k_itemheld]) // Jawz thrown
							{
								if (player->kartstuff[k_throwdir] == 1 || player->kartstuff[k_throwdir] == 0)
									K_ThrowKartItem(player, true, MT_JAWZ, 1, 0);
								else if (player->kartstuff[k_throwdir] == -1) // Throwing backward gives you a dud that doesn't home in
									K_ThrowKartItem(player, true, MT_JAWZ_DUD, -1, 0);
								K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemamount]--;
								K_UpdateHnextList(player, false);
							}
							break;
						case KITEM_MINE:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								mobj_t *mo;
								player->kartstuff[k_itemheld] = 1;
								S_StartSound(player->mo, sfx_s254);
								mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_SSMINE_SHIELD);
								if (mo)
								{
									mo->flags |= MF_NOCLIPTHING;
									mo->threshold = 10;
									mo->movecount = 1;
									mo->movedir = 1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&player->mo->hnext, mo);
								}
							}
							else if (ATTACK_IS_DOWN && player->kartstuff[k_itemheld])
							{
								K_ThrowKartItem(player, false, MT_SSMINE, 1, 1);
								K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemamount]--;
								player->kartstuff[k_itemheld] = 0;
								K_UpdateHnextList(player, true);
							}
							break;
						case KITEM_LANDMINE:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->kartstuff[k_itemamount]--;
								K_ThrowLandMine(player);
								K_PlayAttackTaunt(player->mo);
							}
							break;
						case KITEM_BALLHOG:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->kartstuff[k_itemamount]--;
								K_ThrowKartItem(player, true, MT_BALLHOG, 1, 0);
								K_PlayAttackTaunt(player->mo);
							}
							break;
						case KITEM_SPB:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->kartstuff[k_itemamount]--;
								K_ThrowKartItem(player, true, MT_SPB, 1, 0);
								K_PlayAttackTaunt(player->mo);
							}
							break;
						case KITEM_GROW:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								if (player->kartstuff[k_growshrinktimer] < 0) // If you're shrunk, then "grow" will just make you normal again.
									K_RemoveGrowShrink(player);
								else
								{
									K_PlayPowerGloatSound(player->mo);
									player->mo->scalespeed = mapobjectscale/TICRATE;
									player->mo->destscale = (3*mapobjectscale)/2;
									if (cv_kartdebugshrink.value && !modeattacking && !player->bot)
										player->mo->destscale = (6*player->mo->destscale)/8;
									player->kartstuff[k_growshrinktimer] = itemtime+(4*TICRATE); // 12 seconds
									if (P_IsLocalPlayer(player))
										S_ChangeMusicSpecial("kgrow");
									if (! P_IsDisplayPlayer(player))
										S_StartSound(player->mo, (cv_kartinvinsfx.value ? sfx_alarmg : sfx_kgrow));
									P_RestoreMusic(player);
									S_StartSound(player->mo, sfx_kc5a);
								}
								player->kartstuff[k_itemamount]--;
							}
							break;
						case KITEM_SHRINK:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								K_DoShrink(player);
								player->kartstuff[k_itemamount]--;
								K_PlayPowerGloatSound(player->mo);
							}
							break;
						case KITEM_THUNDERSHIELD:
							if (player->kartstuff[k_curshield] != KSHIELD_THUNDER)
							{
								mobj_t *shield = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_THUNDERSHIELD);
								P_SetScale(shield, (shield->destscale = (5*shield->destscale)>>2));
								P_SetTarget(&shield->target, player->mo);
								S_StartSound(player->mo, sfx_s3k41);
								player->kartstuff[k_curshield] = KSHIELD_THUNDER;
							}

							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								K_DoThunderShield(player);
								player->kartstuff[k_itemamount]--;
								K_PlayAttackTaunt(player->mo);
							}
							break;
						case KITEM_BUBBLESHIELD:
							if (player->kartstuff[k_curshield] != KSHIELD_BUBBLE)
							{
								mobj_t *shield = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_BUBBLESHIELD);
								P_SetScale(shield, (shield->destscale = (5*shield->destscale)>>2));
								P_SetTarget(&shield->target, player->mo);
								S_StartSound(player->mo, sfx_s3k3f);
								player->kartstuff[k_curshield] = KSHIELD_BUBBLE;
							}

							if (!HOLDING_ITEM && NO_HYUDORO)
							{
								if ((cmd->buttons & BT_ATTACK) && player->kartstuff[k_holdready])
								{
									if (player->kartstuff[k_bubbleblowup] == 0)
										S_StartSound(player->mo, sfx_s3k75);

									player->kartstuff[k_bubbleblowup]++;
									player->kartstuff[k_bubblecool] = player->kartstuff[k_bubbleblowup]*4;

									if (player->kartstuff[k_bubbleblowup] > bubbletime*2)
									{
										K_ThrowKartItem(player, (player->kartstuff[k_throwdir] > 0), MT_BUBBLESHIELDTRAP, -1, 0);
										K_PlayAttackTaunt(player->mo);
										player->kartstuff[k_bubbleblowup] = 0;
										player->kartstuff[k_bubblecool] = 0;
										player->kartstuff[k_holdready] = 0;
										player->kartstuff[k_itemamount]--;
									}
								}
								else
								{
									if (player->kartstuff[k_bubbleblowup] > bubbletime)
										player->kartstuff[k_bubbleblowup] = bubbletime;

									if (player->kartstuff[k_bubbleblowup])
										player->kartstuff[k_bubbleblowup]--;

									player->kartstuff[k_holdready] = (player->kartstuff[k_bubblecool] ? 0 : 1);
								}
							}
							break;
						case KITEM_FLAMESHIELD:
							if (player->kartstuff[k_curshield] != KSHIELD_FLAME)
							{
								mobj_t *shield = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_FLAMESHIELD);
								P_SetScale(shield, (shield->destscale = (5*shield->destscale)>>2));
								P_SetTarget(&shield->target, player->mo);
								S_StartSound(player->mo, sfx_s3k3e);
								player->kartstuff[k_curshield] = KSHIELD_FLAME;
							}

							if (!HOLDING_ITEM && NO_HYUDORO)
							{
								INT32 destlen = K_FlameShieldMax(player);
								INT32 flamemax = 0;

								if (player->kartstuff[k_flamelength] < destlen)
									player->kartstuff[k_flamelength]++; // Can always go up!

								flamemax = player->kartstuff[k_flamelength] * flameseg;
								if (flamemax > 0)
									flamemax += TICRATE; // leniency period

								if ((cmd->buttons & BT_ATTACK) && player->kartstuff[k_holdready])
								{
									if (player->kartstuff[k_flamemeter] < 0)
										player->kartstuff[k_flamemeter] = 0;

									if (player->kartstuff[k_flamedash] == 0)
									{
										S_StartSound(player->mo, sfx_s3k43);
										K_PlayBoostTaunt(player->mo);
									}

									player->kartstuff[k_flamedash] += 2;
									player->kartstuff[k_flamemeter] += 2;

									if (!onground)
									{
										P_Thrust(
											player->mo, K_MomentumAngle(player->mo),
											FixedMul(player->mo->scale, K_GetKartGameSpeedScalar(gamespeed))
										);
									}

									if (player->kartstuff[k_flamemeter] > flamemax)
									{
										P_Thrust(
											player->mo, player->mo->angle,
											FixedMul((50*player->mo->scale), K_GetKartGameSpeedScalar(gamespeed))
										);

										player->kartstuff[k_flamemeter] = 0;
										player->kartstuff[k_flamelength] = 0;
										player->kartstuff[k_holdready] = 0;
										player->kartstuff[k_itemamount]--;
									}
								}
								else
								{
									player->kartstuff[k_holdready] = 1;

									if (player->kartstuff[k_flamemeter] > 0)
										player->kartstuff[k_flamemeter]--;

									if (player->kartstuff[k_flamelength] > destlen)
									{
										player->kartstuff[k_flamelength]--; // Can ONLY go down if you're not using it

										flamemax = player->kartstuff[k_flamelength] * flameseg;
										if (flamemax > 0)
											flamemax += TICRATE; // leniency period
									}

									if (player->kartstuff[k_flamemeter] > flamemax)
										player->kartstuff[k_flamemeter] = flamemax;
								}
							}
							break;
						case KITEM_HYUDORO:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->kartstuff[k_itemamount]--;
								K_DoHyudoroSteal(player); // yes. yes they do.
							}
							break;
						case KITEM_POGOSPRING:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && onground && NO_HYUDORO
								&& !player->kartstuff[k_pogospring])
							{
								K_PlayBoostTaunt(player->mo);
								K_DoPogoSpring(player->mo, 32<<FRACBITS, 2);
								player->kartstuff[k_pogospring] = 1;
								player->kartstuff[k_itemamount]--;
							}
							break;
						case KITEM_SUPERRING:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								player->kartstuff[k_superring] += (10*3);
								player->kartstuff[k_itemamount]--;
							}
							break;
						case KITEM_KITCHENSINK:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO)
							{
								mobj_t *mo;
								player->kartstuff[k_itemheld] = 1;
								S_StartSound(player->mo, sfx_s254);
								mo = P_SpawnMobj(player->mo->x, player->mo->y, player->mo->z, MT_SINK_SHIELD);
								if (mo)
								{
									mo->flags |= MF_NOCLIPTHING;
									mo->threshold = 10;
									mo->movecount = 1;
									mo->movedir = 1;
									P_SetTarget(&mo->target, player->mo);
									P_SetTarget(&player->mo->hnext, mo);
								}
							}
							else if (ATTACK_IS_DOWN && HOLDING_ITEM && player->kartstuff[k_itemheld]) // Sink thrown
							{
								K_ThrowKartItem(player, false, MT_SINK, 1, 2);
								K_PlayAttackTaunt(player->mo);
								player->kartstuff[k_itemamount]--;
								player->kartstuff[k_itemheld] = 0;
								K_UpdateHnextList(player, true);
							}
							break;
						case KITEM_SAD:
							if (ATTACK_IS_DOWN && !HOLDING_ITEM && NO_HYUDORO
								&& !player->kartstuff[k_sadtimer])
							{
								player->kartstuff[k_sadtimer] = stealtime;
								player->kartstuff[k_itemamount]--;
							}
							break;
						default:
							break;
					}
				}
			}
		}

		// No more!
		if (!player->kartstuff[k_itemamount])
		{
			player->kartstuff[k_itemheld] = 0;
			player->kartstuff[k_itemtype] = KITEM_NONE;
		}

		if (K_GetShieldFromItem(player->kartstuff[k_itemtype]) == KSHIELD_NONE)
		{
			player->kartstuff[k_curshield] = KSHIELD_NONE; // RESET shield type
			player->kartstuff[k_bubbleblowup] = 0;
			player->kartstuff[k_bubblecool] = 0;
			player->kartstuff[k_flamelength] = 0;
			player->kartstuff[k_flamemeter] = 0;
		}

		if (spbplace == -1 || player->kartstuff[k_position] != spbplace)
			player->kartstuff[k_ringlock] = 0; // reset ring lock

		if (player->kartstuff[k_itemtype] == KITEM_SPB
			|| player->kartstuff[k_itemtype] == KITEM_SHRINK
			|| player->kartstuff[k_growshrinktimer] < 0)
			indirectitemcooldown = 20*TICRATE;

		if (player->kartstuff[k_hyudorotimer] > 0)
		{
			INT32 hyu = hyudorotime;

			if (gametype == GT_RACE)
				hyu *= 2; // double in race

			if (leveltime & 1)
			{
				player->mo->drawflags |= MFD_DONTDRAW;
			}
			else
			{
				if (player->kartstuff[k_hyudorotimer] >= (TICRATE/2) && player->kartstuff[k_hyudorotimer] <= hyu-(TICRATE/2))
					player->mo->drawflags &= ~K_GetPlayerDontDrawFlag(player);
				else
					player->mo->drawflags &= ~MFD_DONTDRAW;
			}

			player->powers[pw_flashing] = player->kartstuff[k_hyudorotimer]; // We'll do this for now, let's people know about the invisible people through subtle hints
		}
		else if (player->kartstuff[k_hyudorotimer] == 0)
		{
			player->mo->drawflags &= ~MFD_DONTDRAW;
		}

		if (gametype == GT_BATTLE && player->bumpers <= 0) // dead in match? you da bomb
		{
			K_DropItems(player); //K_StripItems(player);
			K_StripOther(player);
			player->mo->drawflags |= MFD_SHADOW;
			player->powers[pw_flashing] = player->karmadelay;
		}
		else if (gametype == GT_RACE || player->bumpers > 0)
		{
			player->mo->drawflags &= ~(MFD_TRANSMASK|MFD_BRIGHTMASK);
		}
	}

	K_KartDrift(player, onground); // Not using onground, since we don't want this affected by spring pads
	K_KartSpindash(player);
	
	// Squishing
	// If a Grow player or a sector crushes you, get flattened instead of being killed.
	if (player->kartstuff[k_squishedtimer] <= 0)
	{
		player->mo->flags &= ~MF_NOCLIP;
	}
	else
	{
		player->mo->flags |= MF_NOCLIP;
		player->mo->momx = 0;
		player->mo->momy = 0;
		//player->mo->spriteyscale = FRACUNIT/2
	}

	// Play the starting countdown sounds
	if (player == &players[g_localplayers[0]]) // Don't play louder in splitscreen
	{
		if ((leveltime == starttime-(3*TICRATE)) || (leveltime == starttime-(2*TICRATE)) || (leveltime == starttime-TICRATE))
			S_StartSound(NULL, sfx_s3ka7);
		if (leveltime == starttime)
		{
			S_StartSound(NULL, sfx_s3kad);
			S_StopMusic(); // The GO! sound stops the level start ambience
		}
	}
	
	
	K_LegacyStart(player);
	
	// salty hop! i wanna die
	if (cv_saltyhop.value)
		K_QuiteSaltyHop(player);
	else {
		player->mo->sprzoff = 0;
		player->mo->salty_jump = false;
		player->mo->salty_zoffset = 0;
		player->mo->salty_momz = 0;
	}
	
}

void K_CheckSpectateStatus(void)
{
	UINT8 respawnlist[MAXPLAYERS];
	UINT8 i, j, numingame = 0, numjoiners = 0;

	// Maintain spectate wait timer
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;
		if (players[i].spectator && (players[i].pflags & PF_WANTSTOJOIN))
			players[i].kartstuff[k_spectatewait]++;
		else
			players[i].kartstuff[k_spectatewait] = 0;
	}

	// No one's allowed to join
	if (!cv_allowteamchange.value)
		return;

	// Get the number of players in game, and the players to be de-spectated.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		if (!players[i].spectator)
		{
			numingame++;
			if (cv_ingamecap.value && numingame >= cv_ingamecap.value) // DON'T allow if you've hit the in-game player cap
				return;
			if (gamestate != GS_LEVEL) // Allow if you're not in a level
				continue;
			if (players[i].exiting) // DON'T allow if anyone's exiting
				return;
			if (numingame < 2 || leveltime < starttime || mapreset) // Allow if the match hasn't started yet
				continue;
			if (leveltime > (starttime + 20*TICRATE)) // DON'T allow if the match is 20 seconds in
				return;
			if (gametype == GT_RACE && players[i].laps >= 2) // DON'T allow if the race is at 2 laps
				return;
			continue;
		}
		else if (!(players[i].pflags & PF_WANTSTOJOIN))
			continue;

		respawnlist[numjoiners++] = i;
	}

	// literally zero point in going any further if nobody is joining
	if (!numjoiners)
		return;

	// Organize by spectate wait timer
	if (cv_ingamecap.value)
	{
		UINT8 oldrespawnlist[MAXPLAYERS];
		memcpy(oldrespawnlist, respawnlist, numjoiners);
		for (i = 0; i < numjoiners; i++)
		{
			UINT8 pos = 0;
			INT32 ispecwait = players[oldrespawnlist[i]].kartstuff[k_spectatewait];

			for (j = 0; j < numjoiners; j++)
			{
				INT32 jspecwait = players[oldrespawnlist[j]].kartstuff[k_spectatewait];
				if (j == i)
					continue;
				if (jspecwait > ispecwait)
					pos++;
				else if (jspecwait == ispecwait && j < i)
					pos++;
			}

			respawnlist[pos] = oldrespawnlist[i];
		}
	}

	// Finally, we can de-spectate everyone!
	for (i = 0; i < numjoiners; i++)
	{
		if (cv_ingamecap.value && numingame+i >= cv_ingamecap.value) // Hit the in-game player cap while adding people?
			break;
		P_SpectatorJoinGame(&players[respawnlist[i]]);
	}

	// Reset the match if you're in an empty server
	if (!mapreset && gamestate == GS_LEVEL && leveltime >= starttime && (numingame < 2 && numingame+i >= 2)) // use previous i value
	{
		S_ChangeMusicInternal("chalng", false); // COME ON
		mapreset = 3*TICRATE; // Even though only the server uses this for game logic, set for everyone for HUD
	}
}

//}
