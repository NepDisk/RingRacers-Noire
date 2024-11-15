// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "presets.h"
#include "../n_menu.h"
#include "../../s_sound.h"


void N_RRVanillaPreset(INT32 choice)
{
	(void)choice;
	S_StartSound(NULL, sfx_shield);

	// Rings
	CV_Set(&cv_ng_rings, "On");
	CV_SetValue(&cv_ng_ringcap, 20);
	CV_SetValue(&cv_ng_spillcap, 20);
	CV_Set(&cv_ng_durationcap, "Off");
	CV_Set(&cv_ng_ringdebt, "On");
	CV_Set(&cv_ng_ringsting, "On");
	CV_SetValue(&cv_ng_ringdeathmark, -20);
	CV_Set(&cv_ng_maprings, "On");
	CV_Set(&cv_ng_mapringboxes, "On");
	CV_Set(&cv_ng_ringboxtransform, "On");
	CV_Set(&cv_ng_trickrings, "On");
	CV_Set(&cv_ng_nerfrings, "Off");
	CV_Set(&cv_ng_nerfringsdiminish, "1.0");

	// Items
	CV_Set(&cv_ng_forceoldboxscale, "Off");
	CV_Set(&cv_ng_capsules, "On");
	CV_StealthSet(&cv_ng_oldorbinaut, "Ring Racers");
	CV_StealthSet(&cv_ng_oldjawz, "Ring Racers");
	CV_StealthSet(&cv_ng_oldhyudoro, "Ring Racers");
	CV_StealthSet(&cv_ng_oldballhog, "Ring Racers");
	CV_StealthSet(&cv_ng_oldspb, "Ring Racers");
	CV_StealthSet(&cv_ng_oldinvincibility, "Ring Racers");
	CV_StealthSet(&cv_ng_oldgrow, "Ring Racers");
	CV_StealthSet(&cv_ng_oldshrink, "Ring Racers");
	CV_StealthSet(&cv_ng_oldeggman, "Ring Racers");
	CV_Set(&cv_ng_eggboxinvinpickup, "On");
	CV_Set(&cv_ng_nerfdroptarget, "Off");
	CV_Set(&cv_ng_nerfflameshield, "Off");
	CV_Set(&cv_ng_nerfflameshielddiminish, "1.2");
	CV_Set(&cv_ng_airtimerdrain, "Off");
	CV_StealthSet(&cv_ng_noireodds, "Ring Racers");
	CV_Set(&cv_ng_forceautoroulette, "Off");
	CV_SetValue(&cv_distvar, 2048);

	for (int i = 0; i < NUMKARTITEMS-1; i++)
	{
		CV_Set(&cv_capsuleitems[i], "On");
	}

	CV_Set(&cv_capsuleitems[KITEM_SUPERRING-1], "On");

	// Mechanics
	CV_Set(&cv_ng_fastfall, "On");
	CV_Set(&cv_ng_fastfallbounce, "On");
	CV_Set(&cv_ng_draft, "On");
	CV_Set(&cv_ng_wavedash, "On");
	CV_Set(&cv_ng_tumble, "On");
	CV_Set(&cv_ng_stumble, "On");
	CV_Set(&cv_ng_invincibilitydamage, "Tumble");
	CV_Set(&cv_ng_growdamage, "Stumble");
	CV_Set(&cv_ng_safelanding, "On");
	CV_Set(&cv_ng_hitlag, "On");
	CV_Set(&cv_ng_combo, "On");
	CV_Set(&cv_ng_triangledash, "On");
	CV_Set(&cv_ng_olddrift, "Ring Racers");
	CV_Set(&cv_ng_firstblood, "On");
	CV_Set(&cv_ng_firstbloodrb, "Off");
	CV_SetValue(&cv_ng_mapanger, 2);
	CV_Set(&cv_ng_tripwires, "On");
	CV_Set(&cv_ng_forcenoposition, "Off");
	CV_Set(&cv_ng_dospecialstage, "On");

	CV_Set(&cv_ng_lives, "On");
	CV_Set(&cv_ng_continuesrank, "On");

	CV_Set(&cv_ng_instawhip, "On");
	CV_SetValue(&cv_ng_instawhipcharge, 75);
	CV_SetValue(&cv_ng_instawhiplockout, 70);
	CV_Set(&cv_ng_instawhipdrain, "On");

	CV_Set(&cv_ng_spindash, "On");
	CV_SetValue(&cv_ng_spindashthreshold, 6);
	CV_SetValue(&cv_ng_spindashcharge, 0);
	CV_Set(&cv_ng_spindashoverheat, "On");
	CV_Set(&cv_ng_desperationforce, "Off");

	CV_Set(&cv_ng_butteredslopes, "On");
	CV_Set(&cv_ng_slopeclimb, "On");
	CV_Set(&cv_ng_slopehelper, "Off");
	CV_Set(&cv_ng_slopehelperspeedboostg1, "1.8");
	CV_Set(&cv_ng_slopehelperspeedboostg2, "1.5");
	CV_Set(&cv_ng_slopehelperspeedboostg3, "1.2");
	CV_Set(&cv_ng_slopehelperaccelboost, "1.25");
	CV_Set(&cv_ng_stairjank, "All");
	CV_Set(&cv_ng_turnstyle, "Vanilla");
	CV_Set(&cv_ng_underwaterhandling, "On");
	CV_Set(&cv_ng_oldpogooverride, "Off");
	CV_Set(&cv_ng_nophysicsflag, "Off");
	CV_Set(&cv_ng_oldboostpower, "Off");
	CV_Set(&cv_ng_oldspeedcalc, "Off");
	CV_SetValue(&cv_ng_airspeedcap, 50);
	CV_Set(&cv_ng_botrubberbandboost, "On");
	CV_Set(&cv_ng_charsetrivals, "On");
	CV_Set(&cv_ng_rivals, "On");
	CV_SetValue(&cv_ng_rivaltopspeed, 11);
	CV_SetValue(&cv_ng_rivalringpower, 20);
	CV_Set(&cv_ng_rivalfrantic, "On");
	CV_Set(&cv_ng_rivaldraft, "On");
}

void N_NoirePreset(INT32 choice)
{
	(void)choice;
	S_StartSound(NULL, sfx_wirlsg);

	// Rings
	CV_Set(&cv_ng_rings, "Off");
	CV_SetValue(&cv_ng_ringcap, 0);
	CV_SetValue(&cv_ng_spillcap, 0);
	CV_Set(&cv_ng_durationcap, "Off");
	CV_Set(&cv_ng_ringdebt, "Off");
	CV_Set(&cv_ng_ringsting, "Off");
	CV_SetValue(&cv_ng_ringdeathmark, -20);
	CV_Set(&cv_ng_maprings, "Off");
	CV_Set(&cv_ng_mapringboxes, "Off");
	CV_Set(&cv_ng_ringboxtransform, "Off");
	CV_Set(&cv_ng_trickrings, "Off");
	CV_Set(&cv_ng_nerfrings, "Off");
	CV_Set(&cv_ng_nerfringsdiminish, "1.0");

	// Items
	CV_Set(&cv_ng_forceoldboxscale, "Off");
	CV_Set(&cv_ng_capsules, "On");
	CV_StealthSet(&cv_ng_oldorbinaut, "Kart");
	CV_StealthSet(&cv_ng_oldjawz, "Kart");
	CV_StealthSet(&cv_ng_oldhyudoro, "Kart");
	CV_StealthSet(&cv_ng_oldballhog, "Kart");
	CV_StealthSet(&cv_ng_oldspb, "Kart");
	CV_StealthSet(&cv_ng_oldinvincibility, "Kart");
	CV_StealthSet(&cv_ng_oldgrow, "Kart");
	CV_StealthSet(&cv_ng_oldshrink, "Kart");
	CV_StealthSet(&cv_ng_oldeggman, "Kart");
	CV_Set(&cv_ng_eggboxinvinpickup, "Off");
	CV_Set(&cv_ng_nerfdroptarget, "On");
	CV_Set(&cv_ng_nerfflameshield, "On");
	CV_Set(&cv_ng_nerfflameshielddiminish, "1.2");
	CV_Set(&cv_ng_airtimerdrain, "On");
	CV_StealthSet(&cv_ng_noireodds, "Noire");
	CV_Set(&cv_ng_forceautoroulette, "On");
	CV_SetValue(&cv_distvar, 2048);

	for (int i = 0; i < NUMKARTITEMS-1; i++)
	{
		CV_Set(&cv_capsuleitems[i], "On");
	}

	CV_Set(&cv_capsuleitems[KITEM_SUPERRING-1], "Off");

	// Mechanics
	CV_Set(&cv_ng_fastfall, "On");
	CV_Set(&cv_ng_fastfallbounce, "Bubble Shield");
	CV_Set(&cv_ng_draft, "Off");
	CV_Set(&cv_ng_wavedash, "On");
	CV_Set(&cv_ng_tumble, "Off");
	CV_Set(&cv_ng_stumble, "Off");
	CV_Set(&cv_ng_invincibilitydamage, "Spinout");
	CV_Set(&cv_ng_growdamage, "Spinout");
	CV_Set(&cv_ng_safelanding, "Off");
	CV_Set(&cv_ng_hitlag, "Off");
	CV_Set(&cv_ng_combo, "Off");
	CV_Set(&cv_ng_triangledash, "Forward Thrust Only");
	CV_Set(&cv_ng_olddrift, "Ring Racers");
	CV_Set(&cv_ng_firstblood, "On");
	CV_Set(&cv_ng_firstbloodrb, "Off");
	CV_SetValue(&cv_ng_mapanger, -1);
	CV_Set(&cv_ng_tripwires, "On");
	CV_Set(&cv_ng_forcenoposition, "Off");
	CV_Set(&cv_ng_dospecialstage, "On");

	CV_Set(&cv_ng_lives, "On");
	CV_Set(&cv_ng_continuesrank, "On");

	CV_Set(&cv_ng_instawhip, "Off");
	CV_SetValue(&cv_ng_instawhipcharge, 0);
	CV_SetValue(&cv_ng_instawhiplockout, 0);
	CV_Set(&cv_ng_instawhipdrain, "Off");

	CV_Set(&cv_ng_spindash, "On");
	CV_SetValue(&cv_ng_spindashthreshold, 6);
	CV_SetValue(&cv_ng_spindashcharge, 0);
	CV_Set(&cv_ng_spindashoverheat, "On");
	CV_Set(&cv_ng_desperationforce, "On");

	CV_Set(&cv_ng_butteredslopes, "On");
	CV_Set(&cv_ng_slopeclimb, "Off");
	CV_Set(&cv_ng_slopehelper, "On");
	CV_Set(&cv_ng_slopehelperspeedboostg1, "1.8");
	CV_Set(&cv_ng_slopehelperspeedboostg2, "1.5");
	CV_Set(&cv_ng_slopehelperspeedboostg3, "1.2");
	CV_Set(&cv_ng_slopehelperaccelboost, "1.25");
	CV_Set(&cv_ng_stairjank, "None");
	CV_Set(&cv_ng_turnstyle, "V1");
	CV_Set(&cv_ng_underwaterhandling, "Off");
	CV_Set(&cv_ng_oldpogooverride, "Off");
	CV_Set(&cv_ng_nophysicsflag, "On");
	CV_Set(&cv_ng_oldboostpower, "Off");
	CV_Set(&cv_ng_oldspeedcalc, "Off");
	CV_SetValue(&cv_ng_airspeedcap, 50);
	CV_Set(&cv_ng_botrubberbandboost, "On");
	CV_Set(&cv_ng_charsetrivals, "Off");
	CV_Set(&cv_ng_rivals, "Off");
	CV_SetValue(&cv_ng_rivaltopspeed, 8);
	CV_SetValue(&cv_ng_rivalringpower, 0);
	CV_Set(&cv_ng_rivalfrantic, "On");
	CV_Set(&cv_ng_rivaldraft, "Off");
}

void N_SRB2KPreset(INT32 choice)
{
	(void)choice;
	S_StartSound(NULL, sfx_forcsg);
	// Rings
	CV_Set(&cv_ng_rings, "Off");
	CV_SetValue(&cv_ng_ringcap, 0);
	CV_SetValue(&cv_ng_spillcap, 0);
	CV_Set(&cv_ng_durationcap, "Off");
	CV_Set(&cv_ng_ringdebt, "Off");
	CV_Set(&cv_ng_ringsting, "Off");
	CV_SetValue(&cv_ng_ringdeathmark, -20);
	CV_Set(&cv_ng_maprings, "Off");
	CV_Set(&cv_ng_mapringboxes, "Off");
	CV_Set(&cv_ng_ringboxtransform, "Off");
	CV_Set(&cv_ng_trickrings, "Off");
	CV_Set(&cv_ng_nerfrings, "Off");
	CV_Set(&cv_ng_nerfringsdiminish, "1.0");

	// Items
	CV_Set(&cv_ng_forceoldboxscale, "Off");
	CV_Set(&cv_ng_capsules, "Off");
	CV_StealthSet(&cv_ng_oldorbinaut, "Kart");
	CV_StealthSet(&cv_ng_oldjawz, "Kart");
	CV_StealthSet(&cv_ng_oldhyudoro, "Kart");
	CV_StealthSet(&cv_ng_oldballhog, "Kart");
	CV_StealthSet(&cv_ng_oldspb, "Kart");
	CV_StealthSet(&cv_ng_oldinvincibility, "Kart");
	CV_StealthSet(&cv_ng_oldgrow, "Kart");
	CV_StealthSet(&cv_ng_oldshrink, "Kart");
	CV_StealthSet(&cv_ng_oldeggman, "Kart");
	CV_Set(&cv_ng_eggboxinvinpickup, "Off");
	CV_Set(&cv_ng_nerfdroptarget, "On");
	CV_Set(&cv_ng_nerfflameshield, "On");
	CV_Set(&cv_ng_nerfflameshielddiminish, "1.2");
	CV_Set(&cv_ng_airtimerdrain, "On");
	CV_StealthSet(&cv_ng_noireodds, "Noire");
	CV_Set(&cv_ng_forceautoroulette, "On");
	CV_SetValue(&cv_distvar, 2048);

	for (int i = 0; i < NUMKARTITEMS-1; i++)
	{
		CV_Set(&cv_capsuleitems[i], "On");
	}

	CV_Set(&cv_capsuleitems[KITEM_SUPERRING-1], "Off");

	// Mechanics
	CV_Set(&cv_ng_fastfall, "Off");
	CV_Set(&cv_ng_fastfallbounce, "Off");
	CV_Set(&cv_ng_draft, "Off");
	CV_Set(&cv_ng_wavedash, "Off");
	CV_Set(&cv_ng_tumble, "Off");
	CV_Set(&cv_ng_stumble, "Off");
	CV_Set(&cv_ng_invincibilitydamage, "Spinout");
	CV_Set(&cv_ng_growdamage, "Spinout");
	CV_Set(&cv_ng_safelanding, "Off");
	CV_Set(&cv_ng_hitlag, "Off");
	CV_Set(&cv_ng_combo, "Off");
	CV_Set(&cv_ng_triangledash, "Off");
	CV_Set(&cv_ng_olddrift, "Kart");
	CV_Set(&cv_ng_firstblood, "On");
	CV_Set(&cv_ng_firstbloodrb, "Off");
	CV_SetValue(&cv_ng_mapanger, -1);
	CV_Set(&cv_ng_tripwires, "On");
	CV_Set(&cv_ng_forcenoposition, "Off");
	CV_Set(&cv_ng_dospecialstage, "On");

	CV_Set(&cv_ng_lives, "On");
	CV_Set(&cv_ng_continuesrank, "On");

	CV_Set(&cv_ng_instawhip, "Off");
	CV_SetValue(&cv_ng_instawhipcharge, 0);
	CV_SetValue(&cv_ng_instawhiplockout, 0);
	CV_Set(&cv_ng_instawhipdrain, "Off");

	CV_Set(&cv_ng_spindash, "Off");
	CV_SetValue(&cv_ng_spindashthreshold, 6);
	CV_SetValue(&cv_ng_spindashcharge, 0);
	CV_Set(&cv_ng_spindashoverheat, "On");
	CV_Set(&cv_ng_desperationforce, "On");

	CV_Set(&cv_ng_butteredslopes, "On");
	CV_Set(&cv_ng_slopeclimb, "Off");
	CV_Set(&cv_ng_slopehelper, "On");
	CV_Set(&cv_ng_slopehelperspeedboostg1, "1.7");
	CV_Set(&cv_ng_slopehelperspeedboostg2, "1.5");
	CV_Set(&cv_ng_slopehelperspeedboostg3, "1.2");
	CV_Set(&cv_ng_slopehelperaccelboost, "1.25");
	CV_Set(&cv_ng_stairjank, "None");
	CV_Set(&cv_ng_turnstyle, "V1");
	CV_Set(&cv_ng_underwaterhandling, "Off");
	CV_Set(&cv_ng_oldpogooverride, "Off");
	CV_Set(&cv_ng_nophysicsflag, "On");
	CV_Set(&cv_ng_oldboostpower, "On");
	CV_Set(&cv_ng_oldspeedcalc, "On");
	CV_SetValue(&cv_ng_airspeedcap, 75);
	CV_Set(&cv_ng_botrubberbandboost, "On");
	CV_Set(&cv_ng_charsetrivals, "Off");
	CV_Set(&cv_ng_rivals, "Off");
	CV_SetValue(&cv_ng_rivaltopspeed, 8);
	CV_SetValue(&cv_ng_rivalringpower, 0);
	CV_Set(&cv_ng_rivalfrantic, "On");
	CV_Set(&cv_ng_rivaldraft, "Off");
}
