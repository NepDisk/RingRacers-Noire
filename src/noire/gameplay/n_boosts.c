// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2020 by KartKrew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../n_boosts.h"
#include "../../k_podium.h"
#include "../../k_kart.h"

// Returns kart speed from a stat. Boost power and scale are NOT taken into account, no player or object is necessary.
fixed_t N_GetKartSpeedFromStat(UINT8 kartspeed)
{
	const fixed_t xspd = 3072;
	fixed_t g_cc = FRACUNIT;
	fixed_t k_speed = 150;
	fixed_t finalspeed;

	switch (gamespeed)
	{
		case 0:
			g_cc = 53248 + xspd; //  50cc =  81.25 + 4.69 =  85.94%
			break;
		case 2:
			g_cc = 77824 + xspd; // 150cc = 118.75 + 4.69 = 123.44%
			break;
		default:
			g_cc = 65536 + xspd; // 100cc = 100.00 + 4.69 = 104.69%
			break;
	}

	k_speed += kartspeed*3; // 153 - 177

	finalspeed = FixedMul(k_speed<<14, g_cc);
	return finalspeed;
}

fixed_t N_GetKartSpeed(const player_t *player, boolean doboostpower, boolean dorubberband)
{
	UINT8 kartspeed = player->kartspeed;
	fixed_t finalspeed = 0;
	const boolean mobjValid = (player->mo != NULL && P_MobjWasRemoved(player->mo) == false);
	const fixed_t physicsScale = mobjValid ? K_GrowShrinkSpeedMul(player) : FRACUNIT;

	if (cv_ng_airspeedcap.value > 0)
	{
		if (doboostpower && !player->pogospring && !P_IsObjectOnGround(player->mo))
			return (cv_ng_airspeedcap.value*mapobjectscale); // air speed cap
	}

	if (K_PodiumSequence() == true)
	{
		// Make 1st reach their podium faster!
		finalspeed = N_GetKartSpeedFromStat(max(1, 11 - (player->position * 3)));

		// Ignore other speed boosts.
		doboostpower = dorubberband = false;
	}
	else
	{

		finalspeed = N_GetKartSpeedFromStat(kartspeed);

		//if (doboostpower && !player->pogospring && !P_IsObjectOnGround(player->mo))
			//return (75*mapobjectscale); // air speed cap

		if (player->spheres > 0)
		{
			fixed_t sphereAdd = (FRACUNIT/60); // 66% at max
			finalspeed = FixedMul(finalspeed, FRACUNIT + (sphereAdd * player->spheres));
		}

		if (K_PlayerUsesBotMovement(player))
		{
			// Increase bot speed by 0-10% depending on difficulty
			const fixed_t modifier = K_BotMapModifier();
			fixed_t add = ((player->botvars.difficulty-1) * FixedMul(FRACUNIT / 10, modifier)) / (DIFFICULTBOT-1);
			finalspeed = FixedMul(finalspeed, FRACUNIT + add);

			if (player->bot && (player->botvars.rival || cv_levelskull.value))
			{
				// +10% top speed for the rival
				finalspeed = FixedMul(finalspeed, cv_ng_rivaltopspeed.value*FRACUNIT/10);
			}
		}
	}

	finalspeed = FixedMul(finalspeed, mapobjectscale);

	if (dorubberband == true && player->botvars.rubberband < FRACUNIT && K_PlayerUsesBotMovement(player) == true)
	{
		finalspeed = FixedMul(finalspeed, player->botvars.rubberband);
	}

	if (doboostpower)
	{
		// Scale with the player.
		finalspeed = FixedMul(finalspeed, physicsScale);

		finalspeed = FixedMul(finalspeed, player->boostpower+player->speedboost);

	}

	if (player->outrun != 0)
	{
		// Milky Way's roads
		finalspeed += FixedMul(player->outrun, player->mo->scale);
	}

	return finalspeed;
}

fixed_t N_GetKartAccel(const player_t *player)
{
	fixed_t k_accel = 32;
	UINT8 stat = (9 - player->kartspeed);

	if (K_PodiumSequence() == true)
	{
		// Normalize to Metal's accel
		stat = 1;
	}

	k_accel += 4 * stat; // 32 - 64

	// Marble Garden Top gets 1200% accel
	if (player->curshield == KSHIELD_TOP)
	{
		k_accel = FixedMul(k_accel, player->topAccel);
	}

	if (K_PodiumSequence() == true)
	{
		k_accel = FixedMul(k_accel, FRACUNIT);
	}
	else
	{
		k_accel =  FixedMul(k_accel, FRACUNIT+player->accelboost);
	}

	return k_accel;
}

fixed_t N_GetNewSpeed(const player_t *player)
{
	fixed_t accelmax = 4000;
	fixed_t newspeed, oldspeed, finalspeed;
	fixed_t p_speed = N_GetKartSpeed(player, true, true);
	fixed_t p_accel = N_GetKartAccel(player);
	boolean onground = (P_IsObjectOnGround(player->mo) || player->pogospring);

	if (!onground) return 0; // If the player isn't on the ground, there is no change in speed

	if (player->curshield == KSHIELD_TOP)
	{
		p_speed = 15 * p_speed / 10;
	}

	if (K_PlayerUsesBotMovement(player) == true && player->botvars.rubberband > 0)
	{
		// Acceleration is tied to top speed...
		// so if we want JUST a top speed boost, we have to do this...
		p_accel = FixedDiv(p_accel, player->botvars.rubberband);
	}

	// ACCELCODE!!!1!11!
	oldspeed = R_PointToDist2(0, 0, player->rmomx, player->rmomy); // FixedMul(P_AproxDistance(player->rmomx, player->rmomy), player->mo->scale);
	newspeed = FixedDiv(FixedDiv(FixedMul(oldspeed, accelmax - p_accel) + FixedMul(p_speed, p_accel), accelmax), ORIG_FRICTION);

	if (player->pogospring) // Pogo Spring minimum/maximum thrust
	{
		const fixed_t hscale = mapobjectscale;
		fixed_t minspeed = 24*hscale;
		fixed_t maxspeed = 28*hscale;

		if (player->mo->terrain)
		{
			minspeed = player->mo->terrain->pogoSpringMin*hscale;
			maxspeed = player->mo->terrain->pogoSpringMax*hscale;
		}

		if (newspeed > maxspeed && player->pogospring == 2)
			newspeed = maxspeed;
		if (newspeed < minspeed)
			newspeed = minspeed;
	}

	finalspeed = newspeed - oldspeed;

	return finalspeed;
}

fixed_t N_3dKartMovement(const player_t *player)
{
	fixed_t finalspeed = N_GetNewSpeed(player);
	SINT8 forwardmove = K_GetForwardMove(player);
	// forwardmove is:
	//  50 while accelerating,
	//  25 while clutching,
	//   0 with no gas, and
	// -25 when only braking.

	finalspeed *= forwardmove/25;
	finalspeed /= 2;

	if (forwardmove < 0 && finalspeed > mapobjectscale*2)
		return finalspeed/2;
	else if (forwardmove < 0)
		return -mapobjectscale/2;

	if (finalspeed < 0)
		finalspeed = 0;

	return finalspeed;
}

// v2 almost broke sliptiding when it fixed turning bugs!
// This value is fine-tuned to feel like v1 again without reverting any of those changes.
#define SLIPTIDEHANDLING 7*FRACUNIT/8

static boolean K_HasInfiniteTether(player_t *player)
{
	switch (player->curshield)
	{
		case KSHIELD_LIGHTNING:
			return true;
	}

	if (player->eggmanexplode > 0)
		return true;

	if (player->trickcharge)
		return true;

	if (player->infinitether)
		return true;

	return false;
}

static fixed_t K_RingDurationBoost(const player_t *player)
{
	fixed_t ret = FRACUNIT;

	if (K_PlayerUsesBotMovement(player))
	{
		// x2.0 for Lv. 9
		const fixed_t modifier = K_BotMapModifier();
		fixed_t add = ((player->botvars.difficulty-1) * modifier) / (DIFFICULTBOT-1);

		ret += add;

		if (player->botvars.rival == true || cv_levelskull.value)
		{
			// x2.0 for Rival
			ret *= 2;
		}
	}

	return ret;
}

//Rescale oh boy based on old version of blib
static fixed_t K_BoostRescale(fixed_t value,fixed_t oldmin,fixed_t oldmax,fixed_t newmin,fixed_t newmax)
{
		return newmin +  FixedMul(FixedDiv( value-oldmin , oldmax-oldmin), newmax-newmin);
}

static fixed_t K_FlameShieldDashVar(INT32 val)
{
	// 1 second = 75% + 50% top speed
	return (3*FRACUNIT/4) + (((val * FRACUNIT) / TICRATE));
}

#define SLIPTIDEHANDLING 7*FRACUNIT/8

void N_GetKartBoostPower(player_t *player)
{
	fixed_t boostpower = FRACUNIT;
	fixed_t speedboost = 0, accelboost = 0, handleboost = 0, nonspeedboost = 0;
	fixed_t intermediate = 0;
	fixed_t harddiminish = 0;
	fixed_t boostmult = FRACUNIT;
	UINT8 numboosts = 0;

	if (player->spinouttimer && player->wipeoutslow == 1) // Slow down after you've been bumped
	{
		player->boostpower = player->speedboost = player->accelboost = 0;
		return;
	}

	// Offroad is separate, it's difficult to factor it in with a variable value anyway.
	if (K_ApplyOffroad(player) && player->offroad >= 0)
		boostpower = FixedDiv(boostpower, player->offroad + FRACUNIT);

	if (player->bananadrag > TICRATE)
		boostpower = (4*boostpower)/5;

#define ADDBOOST(s,a,h,b,ns) { \
	numboosts++; \
	speedboost += max(s, speedboost); \
	accelboost = max(a, accelboost); \
	handleboost = max(h, handleboost); \
	nonspeedboost = max(ns, nonspeedboost); \
	boostmult += b; \
}

	if (player->sneakertimer) // Sneaker
	{
		fixed_t sneakerspeedboost = 0;
		int i;
		switch (gamespeed)
		{
			case 0:
				sneakerspeedboost = 53740+768;
				break;
			case 2:
				sneakerspeedboost = 17294+768;
				break;
			default:
				sneakerspeedboost = 32768;
				break;
		}
		ADDBOOST(sneakerspeedboost * (player->numsneakers ? player->numsneakers : 1),8*FRACUNIT,SLIPTIDEHANDLING,0,0)
	}

	if (player->invincibilitytimer) // Invincibility
	{
		ADDBOOST(3*FRACUNIT/8,3*FRACUNIT,SLIPTIDEHANDLING/2,0,0)
	}

	if (player->growshrinktimer > 0) // Grow
	{
		ADDBOOST(0,FloatToFixed(0.4),2*SLIPTIDEHANDLING/5,FloatToFixed(-0.3),FloatToFixed(0.3));
	}

	if (cv_ng_nerfflameshield.value)
	{
		if (player->flamedash) // Flame Shield dash with diminish nerf
		{
			fixed_t dash = K_FlameShieldDashVar(player->flamedash);
			fixed_t diminishvalue = cv_ng_nerfflameshielddiminish.value;
			fixed_t intermediate = 0;
			fixed_t boost = 0;

			intermediate = FixedDiv(FixedMul(diminishvalue, FRACUNIT*-1/2) - FRACUNIT/4,-diminishvalue+FRACUNIT/2);
			boost = FixedMul(diminishvalue,(FRACUNIT-FixedDiv(FRACUNIT,(dash+intermediate))));

			ADDBOOST(
				boost, // + diminished top speed
				3*FRACUNIT, // + 300% acceleration
				FixedMul(FixedDiv(boost, FRACUNIT/2), SLIPTIDEHANDLING/2) // + infinite handling
				,0  // No boostmult
				,0
			);
		}
	}
	else
	{
		if (player->flamedash) // Flame Shield dash
		{
			fixed_t dash = K_FlameShieldDashVar(player->flamedash);
			ADDBOOST(
				dash, // + infinite top speed
				3*FRACUNIT, // + 300% acceleration
				FixedMul(FixedDiv(dash, FRACUNIT/2), SLIPTIDEHANDLING/2) // + infinite handling
				,0  // No boostmult
				,0
			);
		}
	}

	if (player->counterdash) // "Fake Flame" (bubble, voltage)
	{
		fixed_t dash = K_FlameShieldDashVar(player->counterdash);
		ADDBOOST(
			dash, // + infinite top speed
			3*FRACUNIT, // + 300% acceleration
			FixedMul(FixedDiv(dash, FRACUNIT/2), SLIPTIDEHANDLING/2) // + infinite handling
			,0 // No boostmult
			,0
		);
	}

	if (player->wavedashboost)
	{
		// NB: This is intentionally under the 25% handleboost threshold required to initiate a sliptide
		ADDBOOST(
			Easing_InCubic(
				player->wavedashpower,
				0,
				8*FRACUNIT/10
			),
			Easing_InSine(
				player->wavedashpower,
				0,
				4*FRACUNIT
			),
			2*SLIPTIDEHANDLING/5
			,0
			,0
		);  // + 80% top speed (peak), +400% acceleration (peak), +20% handling, No bootmult
	}

	if (player->spindashboost) // Spindash boost
	{
		const fixed_t MAXCHARGESPEED = K_GetSpindashChargeSpeed(player);
		const fixed_t exponent = FixedMul(player->spindashspeed, player->spindashspeed);

		// character & charge dependent
		ADDBOOST(
			FixedMul(MAXCHARGESPEED, exponent), // + 0 to K_GetSpindashChargeSpeed()% top speed
			(40 * exponent), // + 0% to 4000% acceleration
			0, // + 0% handling
			0, // No Boost Mult
			0
		);
	}

	if (player->startboost) // Startup Boost
	{
		ADDBOOST(FRACUNIT, 4*FRACUNIT, SLIPTIDEHANDLING,0,0);
	}

	if (player->dropdashboost) // Drop dash
	{
		ADDBOOST(FRACUNIT/4, 6*FRACUNIT, SLIPTIDEHANDLING,0,0);
	}

	if (player->driftboost) // Drift Boost
	{
		ADDBOOST(FRACUNIT/4,4*FRACUNIT,0,0,0)
	}

	if (player->trickboost)	// Trick pannel up-boost
	{
		ADDBOOST(player->trickboostpower, 5*FRACUNIT, 0,0,0);	// <trickboostpower>% speed, 500% accel, 0% handling
	}

	if (player->gateBoost) // SPB Juicebox boost
	{
		ADDBOOST(3*FRACUNIT/4, 4*FRACUNIT, SLIPTIDEHANDLING/2,0,0); // + 75% top speed, + 400% acceleration, +25% handling
	}

	if (player->ringboost) // Ring Boost
	{
		// This one's a little special: we add extra top speed per tic of ringboost stored up, to allow for Ring Box to really rocket away.
		// (We compensate when decrementing ringboost to avoid runaway exponential scaling hell.)
		fixed_t rb = FixedDiv(player->ringboost * FRACUNIT, max(FRACUNIT, K_RingDurationBoost(player)));
		ADDBOOST(
			FRACUNIT/5 + FixedMul(FRACUNIT / 1750, rb),
			4*FRACUNIT,
			Easing_InCubic(min(FRACUNIT, rb / (TICRATE*12)), 0, 0),
			0,
			0
		); // + 20% + ???% top speed, + 400% acceleration, +???% handling, No boostmult
	}

	if (!cv_ng_oldeggman.value)
	{
		if (player->eggmanexplode) // Ready-to-explode
		{
			ADDBOOST(6*FRACUNIT/20, FRACUNIT, 0, 0,0); // + 30% top speed, + 100% acceleration, +0% handling, No Boost Mult
		}
	}

	if (player->trickcharge)
	{
		// NB: This is an acceleration-only boost.
		// If this is applied earlier in the chain, it will diminish real speed boosts.
		ADDBOOST(0, FRACUNIT,  2*SLIPTIDEHANDLING/10, 0,0); // 0% speed 100% accel 20% handle, No Boost Mult
	}

	// This should always remain the last boost stack before tethering
	if (player->botvars.rubberband > FRACUNIT && K_PlayerUsesBotMovement(player) == true && cv_ng_botrubberbandboost.value)
	{
		ADDBOOST(player->botvars.rubberband - FRACUNIT, 0, 0,0,0);
	}

	if (player->draftpower > 0) // Drafting
	{
		// 30% - 44%, each point of speed adds 1.75%
		fixed_t draftspeed = ((3*FRACUNIT)/10) + ((player->kartspeed-1) * ((7*FRACUNIT)/400));

		if (gametyperules & GTR_CLOSERPLAYERS)
		{
			draftspeed *= 2;
		}

		if (K_HasInfiniteTether(player))
		{
			// infinite tether
			draftspeed *= 2;
		}

		speedboost += FixedMul(draftspeed, player->draftpower); // (Drafting suffers no boost stack penalty.)
		numboosts++;
	}

	speedboost = max(speedboost,nonspeedboost);

	// Diminish Speed for controlability
	if (gamespeed <= 1 && speedboost > FRACUNIT/2)
	{
		intermediate = FixedDiv(FixedMul(FloatToFixed(1.25),FRACUNIT*-1/2) - FRACUNIT/4,-FloatToFixed(1.25)+FRACUNIT/2);
		speedboost = FixedMul(FloatToFixed(1.25),(FRACUNIT-FixedDiv(FRACUNIT,(speedboost+intermediate))));
	}
	else if (gamespeed == 2 && speedboost > FRACUNIT*375/1000)
	{
		harddiminish = K_BoostRescale(FloatToFixed(1.25), FRACUNIT, 2*FRACUNIT, FRACUNIT*95/100, FRACUNIT*180/100);
		intermediate = FixedDiv(FixedMul(harddiminish,FRACUNIT*-625/1000) - 9216,-harddiminish+FRACUNIT*375/1000);
		speedboost = FixedMul(harddiminish,(FRACUNIT-FixedDiv(FRACUNIT,(speedboost+intermediate))));
	}
	else if (gamespeed == 3 && speedboost > FRACUNIT*375/1000)
	{
		harddiminish = K_BoostRescale(FloatToFixed(1.25), FRACUNIT, 2*FRACUNIT, FRACUNIT*95/100, FRACUNIT*180/100);
		intermediate = FixedDiv(FixedMul(harddiminish,FRACUNIT*-625/1000) - 9216,-harddiminish+FRACUNIT*375/1000);
		speedboost = FixedMul(harddiminish,(FRACUNIT-FixedDiv(FRACUNIT,(speedboost+intermediate))));
	}

	// value smoothing
	if (speedboost > player->speedboost)
		player->speedboost = speedboost;
	//brakemod. slowdown on braking or sliptide (based on version from booststack)
	else if ((player->aizdriftstrat && abs(player->drift) < 5) || (player->cmd.buttons & BT_BRAKE))
		player->speedboost = max(speedboost - FloatToFixed(1.25), min(speedboost, 3*FRACUNIT/8));
	else
		player->speedboost += (speedboost - player->speedboost)/(TICRATE/2);

	//This here is the boostmult, its implemented as an adjustment to boostpower
	player->boostpower = boostpower + (FixedMul(player->speedboost, boostmult) - player->speedboost);

	player->accelboost = accelboost;
	player->handleboost = handleboost;

}

//
// K_AdjustPlayerFriction
//
void N_AdjustPlayerFriction(player_t *player, boolean onground)
{
	const fixed_t prevfriction = K_PlayerBaseFriction(player, player->mo->friction);
	// JugadorXEI: Do *not* calculate friction when a player is pogo'd
	// because they'll be in the air and friction will not reset!
	if (onground && !player->pogospring)
	{
		player->mo->friction = prevfriction;

		// Friction
		/*if (!player->offroad)
		{
			if (player->speed > 0 && player->cmd.forwardmove == 0 && player->mo->friction == 59392)
				player->mo->friction += 4608;
		}*/

		// Reduce friction after hitting a spring
		if (player->tiregrease)
		{
			player->mo->friction += ((FRACUNIT - prevfriction) / greasetics) * player->tiregrease;
		}

		// Less friction on Top unless grinding
		if (player->curshield == KSHIELD_TOP &&
				K_GetForwardMove(player) > 0 &&
				player->speed < 2 * K_GetKartSpeed(player, false, false))
		{
			player->mo->friction += 1024;
		}

		if (player->speed > 0 && player->cmd.forwardmove < 0)	// change friction while braking no matter what, otherwise it's not any more effective than just letting go off accel
			player->mo->friction -= 2048;

		if (cv_ng_underwaterhandling.value)
		{
			// Water gets ice physics too
			if ((player->mo->eflags & MFE_TOUCHWATER) &&
					!player->offroad)
			{
				player->mo->friction += 614;
			}
			else if ((player->mo->eflags & MFE_UNDERWATER))
			{
				if (!K_Sliptiding(player))
					player->mo->friction += 312;
			}
		}

		// Wipeout slowdown
		if (player->speed > 0 && player->spinouttimer && player->wipeoutslow)
		{
			if (player->offroad)
				player->mo->friction -= 4912;
			if (player->wipeoutslow == 1)
				player->mo->friction -= 9824;
		}

		if (player->icecube.frozen)
		{
			player->mo->friction = FRACUNIT;
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

	}
}

// Based on indevs slopehelper script, Thanks!
void N_SlopeHelper(player_t *player)
{
	fixed_t speedval = cv_ng_slopehelperspeedboost.value;
	fixed_t accelval = cv_ng_slopehelperaccelboost.value;
	pslope_t *slope = player->mo->standingslope;
	boolean flip = !(player->mo->eflags & MFE_VERTICALFLIP);
	angle_t momangle = player->mo->angle;
	angle_t hillangle = 0;
	fixed_t anglemult;
	fixed_t slopemult;
	fixed_t mult;
	fixed_t addedboost;
	fixed_t addedaccel;

	if (!slope || (player->offroad && K_ApplyOffroad(player)))
		return;

	if ((((int)slope->zangle > 0) && flip) || (((int)slope->zangle < 0) && (!flip)))
		hillangle = momangle - slope->xydirection;
	else
		hillangle = momangle - (slope->xydirection + ANGLE_180);

	hillangle = max(abs((int)hillangle) - ANG1*3, 0); // ANG1*3 somehow fixes some slopes???

	if (hillangle >= ANGLE_90)
		return;

	anglemult = FixedDiv(AngleFixed(ANGLE_90-hillangle), 90*FRACUNIT);
	slopemult = FixedDiv(AngleFixed(min(abs((int)slope->zangle)+ANG1*3, ANGLE_90)), 90*FRACUNIT);

	mult = FixedMul(anglemult, slopemult);

	addedboost = min(FixedMul(mult, speedval), FRACUNIT);
	addedaccel = FixedMul(mult, accelval);

	//CONS_Printf("addedboost: %d\naddedaccel: %d\n",addedboost,addedaccel);

	player->speedboost = max(player->speedboost, addedboost);
	player->accelboost = max(player->accelboost, addedaccel);
}
