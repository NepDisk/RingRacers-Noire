// RadioRacers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by $HOME
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file radioracers/menu/rr_hud.c

#include <math.h>

#include "../rr_hud.h"
#include "../rr_cvar.h"
#include "../../k_roulette.h" // ROULETTE_SPACING, SLOT_SPACING
#include "../../k_hud.h" // trackingResult_t
#include "../../p_local.h" // player, P_MobjFlip()
#include "../../m_fixed.h" // FixedToFloat(), FixedMul(), FixedDiv()
#include "../../r_fps.h" // R_InterpolateFixed()
#include "../../console.h"
#include "../../d_player.h"

#define ITEM_BOX_WIDTH 46 // 50 - 4
#define ITEM_BOX_HEIGHT 50

#define RING_BOX_WIDTH 55 // 56 - 1
#define RING_BOX_HEIGHT 48

typedef struct 
{
    fixed_t scale; // HUD scale for either Ring Box cvar or Item Roulette cvar
    float scale_f; // HUD scale (float)

    int GRAPHIC_WIDTH; // Width of item/ringbox graphic
    int GRAPHIC_HEIGHT; // Height of item/ringbox graphic 

    itemboxposition_e position; // Preferred position of item/ringbox graphic (cvar)
} roulette_type_info_t;


// ================ HELPER FUNCTIONS ===================
static trackingResult_t _getBaseRouletteCoordinatesForTrackingPlayer(void)
{
	trackingResult_t result;

	// No player object? Not bothering.
	const boolean doesPlayerHaveMo = !((stplyr->mo == NULL || P_MobjWasRemoved(stplyr->mo)));
	if (!doesPlayerHaveMo)
		return result;

    // Functionality copied from k_hud_track.cpp
	vector3_t v = {
		R_InterpolateFixed(stplyr->mo->old_x, stplyr->mo->x) + stplyr->mo->sprxoff,
		R_InterpolateFixed(stplyr->mo->old_y, stplyr->mo->y) + stplyr->mo->spryoff,
		R_InterpolateFixed(stplyr->mo->old_z, stplyr->mo->z) + stplyr->mo->sprzoff + 
        (stplyr->mo->height >> 1),
	};

	vector3_t vertical_offset_vector = {
		0, 
		0, 
		64 * stplyr->mo->scale * P_MobjFlip(stplyr->mo)
	};

	FV3_Add(&v, &vertical_offset_vector);
	
	K_ObjectTracking(&result, &v, false);

	return result;
}

static itembox_tracking_coordinates_t _getBaseRouletteCoordinates(int width, int height)
{
    // Initialize the coordinates..
    itembox_tracking_coordinates_t coords = { .x=0, .y=0 };

    // Firstly, get the tracking coordinates
    const trackingResult_t _tracking_coords = _getBaseRouletteCoordinatesForTrackingPlayer();

    // If we're out of range, no point in drawing anything, set valid_coords to false and return
    if(_tracking_coords.x == 0 && _tracking_coords.y == 0)
    {
        coords.valid_coords = false;
        return coords;
    }

    // Otherwise, let's start calculating.

    // Set some base variables to work off of.
    int16_t x = (_tracking_coords.x / FRACUNIT);
    int16_t y = (_tracking_coords.y / FRACUNIT);

    /**
     * Firstly, we need to center the graphic so that it's smack in the middle of the player.
     * Then, from there, we have three directions to choose from. LEFT, UP, RIGHT.
     * 
     * This is where the position CVARs will come into play.
     */
    x -= ((width) / 2);
    y += ((height) / 2);
    
    coords.x = x;
    coords.y = y;
    coords.valid_coords = true;
    
    return coords;
}

static int _clampRouletteCoordinates(fixed_t clamp_scale, int base)
{
    int NEW_BASE = (int)(base * FixedToFloat(clamp_scale));
    return (NEW_BASE);
}

static vector2_t _getRouletteCropping(vector2_t rouletteCrop, float hud_scale)
{
    const int base_x = rouletteCrop.x;
    const int base_y = rouletteCrop.y;
    const float base_x_scale_f = round(base_x * hud_scale);
    const float base_y_scale_f = round(base_y * hud_scale);


    /**
     * 100%
     *  7 - (7 - (7*1.0))
     *  7 - (7 - 7)
     *  7 - (0)
     *  7
     * 
     * 90%
     *  7 - (7 - (7*0.9))
     *  7 - (7 - 6)
     *  7 - (1)
     *  6
     * 
     * etc...
     */
    rouletteCrop.x = base_x - (base_x - (int)(base_x_scale_f));
    rouletteCrop.y = base_y - (base_y - (int)(base_y_scale_f));

    return rouletteCrop;
}

static roulette_offset_spacing_t _getRouletteSpacingOffset(fixed_t space, fixed_t offset, float hud_scale_float)
{
    const int base_roulette_spacing = (space) >> FRACBITS;
    
    const int new_roulette_spacing = ((int)((base_roulette_spacing * hud_scale_float)) << FRACBITS);
    const int new_roulette_offset = FixedMul(offset, FixedDiv(new_roulette_spacing, space));

    const roulette_offset_spacing_t roulette_offset_spacing = {
        .space=new_roulette_spacing, 
        .offset=new_roulette_offset
    };
    return roulette_offset_spacing;
}

static itembox_tracking_coordinates_t _getRouletteCoordinates(roulette_type_info_t info)
{
    const float _scale = info.scale_f;
    const fixed_t _HUD_SCALE = info.scale;
    const int _GRAPHIC_WIDTH = info.GRAPHIC_WIDTH;
    const int _GRAPHIC_HEIGHT = info.GRAPHIC_HEIGHT;

    const float width_f = round(_GRAPHIC_WIDTH * _scale);
    const int width = (int) (width_f);
    const int height = (int) (_GRAPHIC_HEIGHT - (int)(_GRAPHIC_HEIGHT * _scale));

    itembox_tracking_coordinates_t _base_coords = _getBaseRouletteCoordinates(width, height);
    const itemboxposition_e _position = info.position;

    if(!_base_coords.valid_coords)
        return _base_coords;

    // Used for calculating LEFT and RIGHT coordinates.
    const int BASE_WIDTH_OFFSET = _GRAPHIC_WIDTH;
    const int BASE_ITEM_BOX_WIDTH = (int)(BASE_WIDTH_OFFSET * _scale);

    // Used for calculating ABOVE coordinates.
    const int BASE_HEIGHT_OFFSET = _GRAPHIC_HEIGHT;
    const int BASE_ITEM_BOX_HEIGHT = (int)(BASE_HEIGHT_OFFSET * _scale);

    /**
     * Assume X = 0
     * So, what these calculations are doing are how much to subtract X by depending on the HUD scale.
     * 
     * For example, if drawing the graphics at 100% scale:
     * Subtract X by the width of the graphic. 
     * 
     * Drawing graphics at 100% scale:
     * X = X - (100% of the graphic width)
     * 
     * 90% scale:
     * X = X - (100% of the graphic width - 90% of the graphic width).
     * 
     * 80% scale:
     * X = X - (100% of the graphic width - 80% of the graphic width).
     * 
     * And so on. The idea behind this is that the bigger the scale, the further out the item roulette is to the side.
     * And as that scale decreases, it gets closer to the player, so it's easier to keep a track of.
     * 
     * However, past a certain scale, it can get a bit too close to the player and cause clipping.
     * So, there's a conditional that at 60% scale and below, draw the roulette graphic in the same
     * position where it'd be drawn at 60%. This prevents the clipping. 
     * 
     * I think it's called 'clamping'?
     * 
     * Same logic applies to ABOVE positioning, just happening vertically.
     */
    const fixed_t MAX_SCALE_X = (3*FRACUNIT)/5;  // 60%
    int x_offset = BASE_WIDTH_OFFSET - (BASE_WIDTH_OFFSET - BASE_ITEM_BOX_WIDTH);
    if (_HUD_SCALE <= MAX_SCALE_X) {
        x_offset = _clampRouletteCoordinates(MAX_SCALE_X, BASE_WIDTH_OFFSET);
    }

    // ABOVE
    const fixed_t MAX_SCALE_Y = (3*FRACUNIT)/5;  // 60%
    int y_offset = BASE_HEIGHT_OFFSET - (BASE_HEIGHT_OFFSET - BASE_ITEM_BOX_HEIGHT);
    if (_HUD_SCALE <= MAX_SCALE_Y) {
        y_offset = _clampRouletteCoordinates(MAX_SCALE_Y, BASE_HEIGHT_OFFSET);
    }

    const boolean UPSIDE_DOWN = (stplyr->mo->eflags & MFE_VERTICALFLIP);

    switch(_position)
    {
        case LEFT: // To the left of the player.
            _base_coords.x -= x_offset;
            break;
        case ABOVE: // Directly above the player.
            _base_coords.y = (UPSIDE_DOWN) ? 
                    _base_coords.y + y_offset : 
                    _base_coords.y - y_offset;
            break;
        case RIGHT: // To the right of the player.
            _base_coords.x += x_offset;
            break;
    }
    return _base_coords;
}
// ================ HELPER FUNCTIONS ===================

// ================ CVARS ===================
fixed_t RR_getItemBoxHudScale(void)
{
    return cv_item_roulette_player_scale.value;
}

fixed_t RR_getRingBoxHudScale(void)
{
    return cv_ringbox_roulette_player_scale.value;
}

static itemboxposition_e _getItemBoxHudPosition(void)
{
    return cv_item_roulette_player_position.value;
}

static itemboxposition_e _getRingBoxHudPosition(void)
{
    return cv_ringbox_roulette_player_position.value;
}

float RR_getItemBoxHudScaleFloat(void)
{
    return FixedToFloat(RR_getItemBoxHudScale());
}

float RR_getRingBoxHudScaleFloat(void)
{
    return FixedToFloat(RR_getRingBoxHudScale());
}
// ================ CVARS ===================


// ================ MAIN ===================
itembox_tracking_coordinates_t RR_getRouletteCoordinatesForKartItem(void)
{
    const roulette_type_info_t info = {
        .scale = RR_getItemBoxHudScale(),
        .scale_f = RR_getItemBoxHudScaleFloat(),
        .GRAPHIC_WIDTH = ITEM_BOX_WIDTH,
        .GRAPHIC_HEIGHT = ITEM_BOX_HEIGHT,
        .position = _getItemBoxHudPosition()
    };

    return _getRouletteCoordinates(info);
}

itembox_tracking_coordinates_t RR_getRouletteCoordinatesForRingBox(void)
{
    const roulette_type_info_t info = {
        .scale = RR_getRingBoxHudScale(),
        .scale_f = RR_getRingBoxHudScaleFloat(),
        .GRAPHIC_WIDTH = RING_BOX_WIDTH,
        .GRAPHIC_HEIGHT = RING_BOX_HEIGHT,
        .position = _getRingBoxHudPosition()
    };

    return _getRouletteCoordinates(info);
}

vector2_t RR_getRouletteCroppingForKartItem(vector2_t rouletteCrop)
{
    // Usually is {7, 7}
    return _getRouletteCropping(rouletteCrop, RR_getItemBoxHudScaleFloat());
}

vector2_t RR_getRouletteCroppingForRingBox(vector2_t rouletteCrop)
{
    // Usually is {10, 10}
    return _getRouletteCropping(rouletteCrop, RR_getRingBoxHudScaleFloat());
}

roulette_offset_spacing_t RR_getRouletteSpacingOffsetForKartItem(fixed_t offset)
{
    return _getRouletteSpacingOffset(
        ROULETTE_SPACING, // Usually is 36 << FRACBITS
        offset,
        RR_getItemBoxHudScaleFloat()
    );
}

roulette_offset_spacing_t RR_getRouletteSpacingOffsetForRingBox(fixed_t offset)
{
    return _getRouletteSpacingOffset(
        SLOT_SPACING, // Usually is 40 << FRACBITS
        offset,
        RR_getRingBoxHudScaleFloat()
    );
}