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

//Rings
extern consvar_t cv_ng_rings;
extern consvar_t cv_ng_ringcap;
extern consvar_t cv_ng_spillcap;
extern consvar_t cv_ng_ringdebt;
extern consvar_t cv_ng_ringsting;
extern consvar_t cv_ng_ringdeathmark;
extern consvar_t cv_ng_maprings;
extern consvar_t cv_ng_mapringboxes;
extern consvar_t cv_ng_ringboxtransform;

//Collectables
extern consvar_t cv_ng_capsules;

//Items
extern consvar_t cv_ng_oldorbinaut;
extern consvar_t cv_ng_oldjawz;
extern consvar_t cv_ng_oldhyudoro;
extern consvar_t cv_ng_oldballhog;
extern consvar_t cv_ng_oldspb;

//Mechanics
extern consvar_t cv_ng_fastfallbounce;
extern consvar_t cv_ng_draft;
extern consvar_t cv_ng_tumble;
extern consvar_t cv_ng_stumble;
extern consvar_t cv_ng_hitlag;
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

//Driving
extern consvar_t cv_ng_butteredslopes;
extern consvar_t cv_ng_slopeclimb;
extern consvar_t cv_ng_stairjank;
extern consvar_t cv_ng_turnstyle;
extern consvar_t cv_ng_oldpogooverride;
extern consvar_t cv_ng_underwaterhandling;
extern consvar_t cv_ng_nophysicsflag;

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

#ifdef __cplusplus
} // extern "C"
#endif

#endif
