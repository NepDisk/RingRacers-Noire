// Haya's Expansion Pack II for Dr. Robotnik's Ring Racers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by hayaUnderscore/haya

// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  hep2/h_general.h
/// \brief HEP2 global header

#ifndef __H_GENERAL_H__
#define __H_GENERAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "../byteptr.h"
#include "../doomdef.h"
#include "../r_main.h"

// :)
#define HEPVSTRING "v2.3333"

extern patch_t* checkerpatches[4];

// I should REALLY move this to skidrow_t....
extern int bottomtext_x;
extern int skidrow_tick_frac;

extern menu_t OPTIONS_CheatyDef;
extern menu_t MISC_SkidrowDef;
extern menu_t MISC_DevmodeDef;
void M_Skidrow(INT32 choice);

void add_element(void **array, UINT8 *capacity, UINT8 typeSize, void *new_elem);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // __H_GENERAL_H__