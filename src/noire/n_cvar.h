// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by hayaUnderscore/haya
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_CVAR__
#define __N_CVAR__

#include "../command.h"

#ifdef __cplusplus
extern "C" {
#endif

//Player
extern consvar_t cv_colorizedhud;
extern consvar_t cv_colorizeditembox;
extern consvar_t cv_darkitembox;
extern consvar_t cv_colorizedhudcolor;
extern consvar_t cv_oldinputdisplay;
extern consvar_t cv_oldpositiondisplay;
extern consvar_t cv_extendedspeedometer;
extern consvar_t cv_highresportrait;
extern consvar_t cv_spectatormusic;
extern consvar_t cv_spectatormusiclump;

extern consvar_t cv_ng_trick_reversion[MAXSPLITSCREENPLAYERS];
extern consvar_t cv_ng_dummyprofileoldtricks;

//Rings
extern consvar_t cv_ng_rings;
extern consvar_t cv_ng_ringcap;
extern consvar_t cv_ng_spillcap;
extern consvar_t cv_ng_ringdebt;
extern consvar_t cv_ng_ringsting;
extern consvar_t cv_ng_ringdeathmark;
extern consvar_t cv_ng_durationcap;
extern consvar_t cv_ng_maprings;
extern consvar_t cv_ng_mapringcapsules;
extern consvar_t cv_ng_mapringboxes;
extern consvar_t cv_ng_ringboxtransform;
extern consvar_t cv_ng_trickrings;

//Items
extern consvar_t cv_ng_forceoldboxscale;
extern consvar_t cv_ng_capsules;
extern consvar_t cv_ng_oldorbinaut;
extern consvar_t cv_ng_oldjawz;
extern consvar_t cv_ng_oldhyudoro;
extern consvar_t cv_ng_oldballhog;
extern consvar_t cv_ng_oldspb;
extern consvar_t cv_ng_oldinvincibility;
extern consvar_t cv_ng_oldgrow;
extern consvar_t cv_ng_oldshrink;
extern consvar_t cv_ng_oldeggman;
extern consvar_t cv_ng_eggboxinvinpickup;
extern consvar_t cv_ng_nerfdroptarget;
extern consvar_t cv_ng_nerfflameshield;
extern consvar_t cv_ng_nerfflameshielddiminish;
extern consvar_t cv_ng_noireodds;
extern consvar_t cv_ng_forceautoroulette;
extern consvar_t cv_ng_airtimerdrain;

extern consvar_t cv_distvar;
// Itemtable hell
extern consvar_t cv_sitbl[8]; extern consvar_t cv_rsitbl[8];
extern consvar_t cv_initbl[8]; extern consvar_t cv_banitbl[8];
extern consvar_t cv_eggitbl[8]; extern consvar_t cv_orbitbl[8];
extern consvar_t cv_jawitbl[8]; extern consvar_t cv_minitbl[8];
extern consvar_t cv_lminitbl[8]; extern consvar_t cv_balitbl[8];
extern consvar_t cv_spbitbl[8]; extern consvar_t cv_groitbl[8];
extern consvar_t cv_shritbl[8]; extern consvar_t cv_ligitbl[8];
extern consvar_t cv_bubitbl[8]; extern consvar_t cv_flmitbl[8];
extern consvar_t cv_hyuitbl[8]; extern consvar_t cv_pogitbl[8];
extern consvar_t cv_supitbl[8]; extern consvar_t cv_kititbl[8];
extern consvar_t cv_dtgitbl[8]; extern consvar_t cv_gtpitbl[8];
extern consvar_t cv_gacitbl[8]; extern consvar_t cv_dsitbl[8];
extern consvar_t cv_tsitbl[8]; extern consvar_t cv_tbaitbl[8];
extern consvar_t cv_toritbl[8]; extern consvar_t cv_quoitbl[8];
extern consvar_t cv_djaitbl[8]; extern consvar_t cv_tgaitbl[8];

//Mechanics
extern consvar_t cv_ng_fastfall;
extern consvar_t cv_ng_fastfallbounce;
extern consvar_t cv_ng_draft;
extern consvar_t cv_ng_wavedash;
extern consvar_t cv_ng_tumble;
extern consvar_t cv_ng_stumble;
extern consvar_t cv_ng_safelanding;
extern consvar_t cv_ng_hitlag;
extern consvar_t cv_ng_combo;
extern consvar_t cv_ng_invincibilitydamage;
extern consvar_t cv_ng_growdamage;
extern consvar_t cv_ng_triangledash;
extern consvar_t cv_ng_triangledashdownthrust;
extern consvar_t cv_ng_mapanger;
extern consvar_t cv_ng_tripwires;
extern consvar_t cv_ng_lives;
extern consvar_t cv_ng_continuesrank;
extern consvar_t cv_ng_dospecialstage;
extern consvar_t cv_ng_forcenoposition;


//Instawhip
extern consvar_t cv_ng_instawhip;
extern consvar_t cv_ng_instawhipcharge;
extern consvar_t cv_ng_instawhiplockout;
extern consvar_t cv_ng_instawhipdrain;

//Spindash
extern consvar_t cv_ng_spindash;
extern consvar_t cv_ng_spindashthreshold;
extern consvar_t cv_ng_spindashcharge;
extern consvar_t cv_ng_spindashoverheat;
extern consvar_t cv_ng_desperationforce;

//Driving
extern consvar_t cv_ng_butteredslopes;
extern consvar_t cv_ng_slopeclimb;
extern consvar_t cv_ng_slopehelper;
extern consvar_t cv_ng_slopehelperspeedboost;
extern consvar_t cv_ng_slopehelperaccelboost;
extern consvar_t cv_ng_stairjank;
extern consvar_t cv_ng_turnstyle;
extern consvar_t cv_ng_oldpogooverride;
extern consvar_t cv_ng_underwaterhandling;
extern consvar_t cv_ng_nophysicsflag;
extern consvar_t cv_ng_oldboostpower;
extern consvar_t cv_ng_oldspeedcalc;
extern consvar_t cv_ng_airspeedcap;

//Bots
extern consvar_t cv_ng_botrubberbandboost;
extern consvar_t cv_ng_rivals;
extern consvar_t cv_ng_rivaltopspeed;
extern consvar_t cv_ng_rivalringpower;
extern consvar_t cv_ng_rivalfrantic;
extern consvar_t cv_ng_rivaldraft;
extern consvar_t cv_ng_charsetrivals;

void NG_Generic_OnChange(void);
void NG_Rings_OnChange(void);
void NG_Instawhip_OnChange(void);
void NG_Spindash_OnChange(void);
void NG_Olditem_OnChange(void);
void NG_Lives_OnChange(void);
void NG_OldPogoOverride_OnChange(void);
void NG_Rivals_OnChange(void);
void ColorHUD_OnChange(void);
void NG_ForceNoPosition_OnChange(void);
void NG_ForceSmallBoxScale_OnChange(void);
void NG_SRB2KartMovement_OnChange(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
