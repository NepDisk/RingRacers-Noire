// Haya's Expansion Pack II for Dr. Robotnik's Ring Racers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by hayaUnderscore/haya
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  hep2/h_cvars.h
/// \brief HEP2 related consvars

#ifndef __H_CVARS__
#define __H_CVARS__

#ifdef __cplusplus
extern "C" {
#endif

#include "h_general.h"

//Player
extern consvar_t cv_saltyhop;

// Music related stuff
extern consvar_t cv_menumusic;
extern consvar_t cv_postracemusic;
extern consvar_t cv_postracejingles;
extern consvar_t cv_postracevoices;

// Warning
extern consvar_t cv_cheatywarning;

// Flipcam stuff
extern consvar_t cv_flipcam[MAXSPLITSCREENPLAYERS];
extern consvar_t cv_dummyprofileflipcam;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
