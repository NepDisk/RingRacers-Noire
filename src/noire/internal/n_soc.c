// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../n_soc.h"

boolean N_UseLegacyStart(void)
{
    const mapheader_t *mapheader = mapheaderinfo[gamemap - 1];

	if (cv_ng_forcenoposition.value)
		return true;

	if (mapheader->legacystart == true)
		return true;

	if (mapheader->legacystart == false)
		return false;

	return false;
}

boolean N_UseLegacyItemboxScale(void)
{
    const mapheader_t *mapheader = mapheaderinfo[gamemap - 1];

	if (cv_ng_forceoldboxscale.value)
		return true;

	if (mapheader->legacyboxscale == true)
		return true;

	if (mapheader->legacyboxscale == false)
		return false;

	return false;
}

boolean N_UseTerrain(void)
{
    const mapheader_t *mapheader = mapheaderinfo[gamemap - 1];

	if (mapheader->terrain == true)
		return true;

	if (mapheader->terrain == false)
		return false;

	return true;
}

boolean N_LegacyNoPhysics(void)
{
    const mapheader_t *mapheader = mapheaderinfo[gamemap - 1];

	if (mapheader->legacynophysics == true)
		return true;

	if (cv_ng_nophysicsflag.value)
		return true;

	if (mapheader->legacynophysics == false)
		return false;

	return false;
}
