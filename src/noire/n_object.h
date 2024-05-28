// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#ifndef __N_OBJECT__
#define __N_OBJECT__

#include "../../p_local.h"
#include "../../k_kart.h"
#include "../../g_game.h"
#include "../../r_main.h"
#include "../../s_sound.h"
#include "../../m_fixed.h"


#ifdef __cplusplus
extern "C" {
#endif


void OBJ_JawzOldThink(mobj_t *actor);
void Obj_JawzOldThrown(mobj_t *th, fixed_t finalSpeed, SINT8 dir);
void Obj_OrbinautOldThrown(mobj_t *th, fixed_t finalSpeed, SINT8 dir);
void Obj_SPBOldThink(mobj_t *mobj);
void Obj_SPBChase(mobj_t *spb);
void Obj_SPBOldTouch(mobj_t *spb, mobj_t *toucher);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
