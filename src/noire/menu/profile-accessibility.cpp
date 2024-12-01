// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2024 by James Robert Roman.
// Copyright (C) 2024 by Kart Krew.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../../v_draw.hpp"

#include "../../command.h"
#include "../../k_menu.h"
#include "../../m_easing.h"
#include "../../p_local.h" // cv_tilting
#include "../../r_main.h"	// cv_flipcam

#include "../n_menu.h"
#include "../../hep2/h_cvars.h"

using srb2::Draw;

namespace
{

void draw_routine()
{
	Draw row = Draw(M_EaseWithTransition(Easing_InSine, 5 * 48), currentMenu->y).font(Draw::Font::kMenu);

	M_DrawEditProfileTooltips();

	if (optionsmenu.profile != NULL)
	{
		M_DrawProfileCard(optionsmenu.optx, optionsmenu.opty, false, optionsmenu.profile);
	}

	for (int i = 0; i < currentMenu->numitems; ++i)
	{
		const menuitem_t& it = currentMenu->menuitems[i];

		if (it.status & IT_DISPLAY)
		{
			bool selected = i == itemOn;

			Draw h = row.x(currentMenu->x);

			if (selected)
			{
				M_DrawUnderline(h.x(), BASEVIDWIDTH - 18, h.y(), 0);
				M_DrawCursorHand(h.x(), h.y(), 0);
			}

			if ((it.status & IT_HEADERTEXT) == IT_HEADERTEXT)
			{
				h
					.x(-4)
					.flags(V_GRAYMAP)
					.text(it.text);
			}
			else
			{
				h
					.flags(selected ? highlightflags : 0)
					.text(it.text);
			}

			if ((it.status & IT_TYPE) == IT_CVAR)
			{
				bool isDefault = CV_IsSetToDefault(it.itemaction.cvar);
				auto val = Draw::TextElement(it.itemaction.cvar->string).font(Draw::Font::kMenu);

				h = row.x(BASEVIDWIDTH - 18);
				h.align(Draw::Align::kRight).flags(isDefault ? highlightflags : warningflags).text(val);

				if (selected)
				{
					Draw ar = h.flags(highlightflags);
					int ofs = skullAnimCounter / 5;
					ar.x(-val.width() - 10 - ofs).text("\x1C");
					ar.x(2 + ofs).text("\x1D");
				}

				if (!isDefault)
				{
					h.x(selected ? 12 : 5).y(-1).flags(warningflags).text(".");
				}
			}
		}

		row = row.y(11);
	}
}

}; // namespace

menuitem_t OPTIONS_NoireProfileAccessibility[] = {

	{IT_HEADER, "This Profile only:", NULL,
		NULL, {NULL}, 0, 0},

	{IT_STRING | IT_CVAR, "Old Tricks", "Directional inputs instantly trick",
		NULL, {.cvar = &cv_ng_dummyprofileoldtricks}, 0, 0},

	{IT_STRING | IT_CVAR, "Flipcam", "Flip the camera when upside down. Kinda sucks.",
		NULL, {.cvar = &cv_dummyprofileflipcam}, 0, 0},

	{IT_STRING | IT_CVAR, "Inverted Flight", "Hold UP to go down, and vice versa, on a flying mount.",
		NULL, {.cvar = &cv_dummyprofileinvertedflight}, 0, 0},
};

menu_t OPTIONS_NoireProfileAccessibilityDef = {
	sizeof (OPTIONS_NoireProfileAccessibility) / sizeof (menuitem_t),
	&OPTIONS_ProfileAccessibilityDef,
	0,
	OPTIONS_NoireProfileAccessibility,
	145, 33,
	SKINCOLOR_ULTRAMARINE, 0,
	MBF_DRAWBGWHILEPLAYING,
	"FILE",
	2, 5,
	draw_routine,
	M_DrawOptionsCogs,
	M_OptionsTick, // animate cogs
	NULL,
	NULL,
	NULL,
};
