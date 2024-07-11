// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2024 by Indev
// Copyright (C) 2024 by Alufolie91
// Copyright (C) 2024 by GenericHeroGuy
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../n_hud.h"


// NOIRE: Colorized hud
patch_t *kc_timesticker;
patch_t *kc_timestickerwide;
patch_t *kc_lapsticker;
patch_t *kc_lapstickerwide;
patch_t *kc_lapstickernarrow;
patch_t *kc_bumpersticker;
patch_t *kc_bumperstickerwide;
patch_t *kc_capsulesticker;
patch_t *kc_capsulestickerwide;
patch_t *kc_karmasticker;
patch_t *kc_spheresticker;
patch_t *kc_splitspheresticker;
patch_t *kc_splitkarmabomb;
patch_t *kc_timeoutsticker;
patch_t *kc_ringsticker[2];
patch_t *kc_ringstickersplit[4];
patch_t *kc_speedometersticker;
patch_t *kc_itemmulsticker[2];
patch_t *kc_itembg[4];
patch_t *kc_ringbg[2];
patch_t *kc_trickcool[2];

extern patch_t *kp_itembg[6];
extern patch_t *kp_ringbg[6];
extern patch_t *kp_itemmulsticker[2];

CV_PossibleValue_t HudColor_cons_t[MAXSKINCOLORS+1];

boolean K_UseColorHud(void)
{
	return (cv_colorizedhud.value && clr_hud);
}

UINT16 K_GetHudColor(void)
{
	if (cv_colorizedhud.value){
		if (cv_colorizedhudcolor.value) return cv_colorizedhudcolor.value;
	}
	if (stplyr && P_IsMachineLocalPlayer(stplyr) && gamestate == GS_LEVEL) return stplyr->skincolor;
	return ((stplyr && gamestate == GS_LEVEL) ? stplyr->skincolor : cv_playercolor->value);
}

patch_t *K_getItemBoxPatch(boolean small, boolean dark)
{
	UINT8 ofs = (cv_darkitembox.value && dark ? 1 : 0) + (small ? 2 : 0);
	return (cv_colorizeditembox.value && K_UseColorHud()) ? kc_itembg[ofs] : kp_itembg[ofs];
}

patch_t *K_getSlotMachinePatch(boolean small)
{
	UINT8 ofs = small ? 1 : 0;
	return (cv_colorizeditembox.value && K_UseColorHud()) ? kc_ringbg[ofs] : kp_ringbg[ofs];
}

patch_t *K_getItemMulPatch(boolean small)
{
	UINT8 ofs = small ? 1 : 0;
	return K_UseColorHud() ? kc_itemmulsticker[ofs] : kp_itemmulsticker[ofs];
}

void N_LoadColorizedHud(void)
{
	// Stickers
	HU_UpdatePatch(&kc_timesticker, "K_SCTIME");
	HU_UpdatePatch(&kc_timestickerwide, "K_SCTIMW");
	HU_UpdatePatch(&kc_lapsticker, "K_SCLAPS");
	HU_UpdatePatch(&kc_lapstickerwide, "K_SCLAPW");
	HU_UpdatePatch(&kc_lapstickernarrow, "K_SCLAPN");
	HU_UpdatePatch(&kc_bumpersticker, "K_SCBALN");
	HU_UpdatePatch(&kc_bumperstickerwide, "K_SCBALW");
	HU_UpdatePatch(&kc_capsulesticker, "K_SCCAPN");
	HU_UpdatePatch(&kc_capsulestickerwide, "K_SCCAPW");
	HU_UpdatePatch(&kc_karmasticker, "K_SCKARM");
	HU_UpdatePatch(&kc_spheresticker, "K_SCBSMT");
	HU_UpdatePatch(&kc_splitspheresticker, "K_SCBSMC");
	HU_UpdatePatch(&kc_splitkarmabomb, "K_SPTKRM");
	HU_UpdatePatch(&kc_timeoutsticker, "K_SCTOUT");
	HU_UpdatePatch(&kc_ringsticker[0], "RNCBACKA");
	HU_UpdatePatch(&kc_ringsticker[1], "RNCBACKB");
	HU_UpdatePatch(&kc_ringstickersplit[0], "SMRNCBGA");
	HU_UpdatePatch(&kc_ringstickersplit[1], "SMRNCBGB");
	HU_UpdatePatch(&kc_speedometersticker, "K_SPDMBC");
	HU_UpdatePatch(&kc_itemmulsticker[0], "K_ITMULC");
	HU_UpdatePatch(&kc_itemmulsticker[1], "K_ISMULC");

	HU_UpdatePatch(&kc_itembg[0], "K_ITBC");
	HU_UpdatePatch(&kc_itembg[1], "K_ITBCD");
	HU_UpdatePatch(&kc_ringbg[0], "K_RBBC");
	HU_UpdatePatch(&kc_ringbg[1], "K_SBBC");

	HU_UpdatePatch(&kc_itembg[2], "K_ISBC");
	HU_UpdatePatch(&kc_itembg[3], "K_ISBCD");

	HU_UpdatePatch(&kc_trickcool[0], "K_COOLC1");
	HU_UpdatePatch(&kc_trickcool[1], "K_COOLC2");

}

void N_ReloadHUDColorCvar(void)
{
	HudColor_cons_t[0].value = 0;
	HudColor_cons_t[0].strvalue = "Skin Color";

	for (INT32 i = 1; i < MAXSKINCOLORS; i++)
	{
		HudColor_cons_t[i].value = i;
		HudColor_cons_t[i].strvalue = skincolors[i].name;				// SRB2kart
	}

	HudColor_cons_t[MAXSKINCOLORS].value = 0;
	HudColor_cons_t[MAXSKINCOLORS].strvalue = NULL;
}
