// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2020 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_SOC__
#define __N_SOC__

#include "../typedef.h"
#include "../doomstat.h"
#include "../deh_soc.h"
#include "n_cvar.h"

#ifdef __cplusplus
extern "C" {
#endif

boolean N_UseLegacyStart(void);
boolean N_UseLegacyItemboxScale(void);
boolean N_UseTerrain(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
