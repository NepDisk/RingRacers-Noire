// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2024 by Indev
// Copyright (C) 2024 by Alufolie91
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_HUD__
#define __N_HUD__

#include "../k_hud.h"
#include "../d_main.h"
#include "../p_local.h"
#include "../k_kart.h"
#include "../g_game.h"
#include "../r_main.h"
#include "../s_sound.h"
#include "../d_netcmd.h"

#include "n_cvar.h"

#ifdef __cplusplus
extern "C" {
#endif

// Colorized Hud
extern patch_t *kc_timesticker;
extern patch_t *kc_timestickerwide;
extern patch_t *kc_lapsticker;
extern patch_t *kc_lapstickerwide;
extern patch_t *kc_lapstickernarrow;
extern patch_t *kc_bumpersticker;
extern patch_t *kc_bumperstickerwide;
extern patch_t *kc_capsulesticker;
extern patch_t *kc_capsulestickerwide;
extern patch_t *kc_karmasticker;
extern patch_t *kc_spheresticker;
extern patch_t *kc_splitspheresticker;
extern patch_t *kc_splitkarmabomb;
extern patch_t *kc_timeoutsticker;
extern patch_t *kc_ringsticker[2];
extern patch_t *kc_ringstickersplit[4];
extern patch_t *kc_speedometersticker;
extern patch_t *kc_itemmulsticker[2];
extern patch_t *kc_itembg[4];
extern patch_t *kc_ringbg[2];
extern patch_t *kc_trickcool[2];

boolean K_UseColorHud(void);
UINT16 K_GetHudColor(void);

patch_t *K_getItemBoxPatch(boolean small, boolean dark);
patch_t *K_getSlotMachinePatch(boolean small);
patch_t *K_getItemMulPatch(boolean small);

void N_LoadColorizedHud(void);
void N_ReloadHUDColorCvar(void);
extern CV_PossibleValue_t HudColor_cons_t[MAXSKINCOLORS+1];

#ifdef __cplusplus
} // extern "C"
#endif

#endif

