// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2024 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../n_object.h"

#define orbinaut_speed(o) ((o)->movefactor)

#define orbinaut_flags(o) ((o)->movedir)

#define orbinaut_owner(o) ((o)->target)

enum {
	ORBI_DROPPED	= 0x01, // stationary hazard
	ORBI_TOSSED		= 0x02, // Gacha Bom tossed forward
	ORBI_TRAIL		= 0x04, // spawn afterimages
	ORBI_SPIN		= 0x08, // animate facing angle
	ORBI_WATERSKI	= 0x10, // this orbinaut can waterski
};

void Obj_OrbinautOldThrown(mobj_t *th, fixed_t finalSpeed, SINT8 dir)
{
	orbinaut_flags(th) = 0;

	if (orbinaut_owner(th) != NULL && P_MobjWasRemoved(orbinaut_owner(th)) == false
		&& orbinaut_owner(th)->player != NULL)
	{
		th->color = orbinaut_owner(th)->player->skincolor;

		const mobj_t *owner = orbinaut_owner(th);
		const ffloor_t *rover = P_IsObjectFlipped(owner) ? owner->ceilingrover : owner->floorrover;

		if (dir != -1 && rover && (rover->fofflags & FOF_SWIMMABLE))
		{
			// The owner can run on water, so we should too!
			orbinaut_flags(th) |= ORBI_WATERSKI;
		}
	}
	else
	{
		th->color = SKINCOLOR_GREY;
	}

	orbinaut_speed(th) = finalSpeed;

	orbinaut_flags(th) |= ORBI_TRAIL;
}
