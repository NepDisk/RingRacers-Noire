// RadioRacers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by $HOME
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file radioracers/rr_hud.h
/// \brief RadioRacers HUD Functionality

#ifndef __RR_HUD__
#define __RR_HUD__

#include "../k_menu.h"
#include "../m_cond.h"
#include "../command.h"
#include "../console.h"
#include "../d_player.h"
#include "../m_fixed.h"

#include "rr_cvar.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LEFT = 0,
    ABOVE,
    RIGHT
} itemboxposition_e;

typedef struct
{
    fixed_t x;
    fixed_t y;
    boolean valid_coords;
} itembox_tracking_coordinates_t;

typedef struct 
{
    fixed_t space;
    fixed_t offset;
} roulette_offset_spacing_t;


/**
 * Item box graphic is 50 x 50. (42 x 42 excluding any empty space).
 * Ring box graphic is 56 x 48.
 * 
 * If we're drawing the graphics with tracking without doing any extra work to the coordinates, they would be drawn right on top of the player.
 * So, the basis for any positioning (depending on what the player has chosen) is the width .. and multiplying that by the scale cvar.
 */
extern itembox_tracking_coordinates_t RR_getRouletteCoordinatesForKartItem(void);
extern itembox_tracking_coordinates_t RR_getRouletteCoordinatesForRingBox(void);

extern fixed_t RR_getItemBoxHudScale(void);
extern float RR_getItemBoxHudScaleFloat(void);
extern fixed_t RR_getRingBoxHudScale(void);
extern float RR_getRingBoxHudScaleFloat(void);

extern vector2_t RR_getRouletteCroppingForKartItem(vector2_t rouletteCrop);
extern vector2_t RR_getRouletteCroppingForRingBox(vector2_t rouletteCrop);

extern roulette_offset_spacing_t RR_getRouletteSpacingOffsetForKartItem(fixed_t offset);
extern roulette_offset_spacing_t RR_getRouletteSpacingOffsetForRingBox(fixed_t offset);

#ifdef __cplusplus
} // extern "C"
#endif

#endif
