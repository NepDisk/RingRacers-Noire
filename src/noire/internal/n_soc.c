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


