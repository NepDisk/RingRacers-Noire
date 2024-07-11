// Haya's Expansion Pack II for Dr. Robotnik's Ring Racers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by hayaUnderscore/haya

// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  hep2/h_newintro.cpp
/// \brief Original custom devmode screen

#include "h_general.h"
#include "../v_draw.hpp"
#include "../v_video.h"

#include "../d_main.h"
#include "../d_netcmd.h"
#include "../m_cond.h"
#include "../m_misc.h"
#include "../k_menu.h"
#include "../s_sound.h"
#include "../m_random.h"
#include "../music.h"
#include "../g_game.h"
#include "../r_fps.h"
#include "../r_main.h"

// ooooough std
#include <string>
#include <array>
#include <vector>

static menuitem_t MISC_DevmodeMenu[] =
{
	{IT_NOTHING, NULL, NULL, NULL, {NULL}, 0, 0},
};

static UINT8 skidrow_rainbow_fill[] = {
	37, 55, 53, 73, 188, 112, 96, 121, 255, 134, 152, 166, 164, 183, 182, 215
};

// aaaaaaaa
int star_x[BASEVIDWIDTH/2][BASEVIDHEIGHT/2] = {0};
int star_y[BASEVIDWIDTH/2][BASEVIDHEIGHT/2] = {0};

static void M_InitDevmode(void)
{
	skidrow.bottomtext_line = BASEVIDWIDTH << FRACBITS;
	
	// Generate the star bg here ahead of time.
	int x, y = 0;
	
	// Clear them first.
	for (x = 0; x < BASEVIDWIDTH/2; x++)
		for (y = 0; y < BASEVIDHEIGHT/2; y++)
			star_x[x][y] = 0;
		
	for (x = 0; x < BASEVIDWIDTH/2; x++)
		for (y = 0; y < BASEVIDHEIGHT/2; y++)
			star_y[x][y] = 0;
	
	// Yes this is random
	for (x = 0; x < BASEVIDWIDTH/2; x++)
		for (y = 0; y < BASEVIDHEIGHT/2; y++)
			if (M_RandomKey(400) <= 1)
			{
				star_x[x][y] = 1;
				
				x += 3; // add an offset
				y += BASEVIDHEIGHT/4; // erm....
			}
			
	for (x = 0; x < BASEVIDWIDTH/2; x++)
		for (y = 0; y < BASEVIDHEIGHT/2; y++)
			if (M_RandomKey(400) <= 1)
			{
				star_y[x][y] = 1;
				
				x += 3; // add an offset
				y += BASEVIDHEIGHT/4; // erm....
			}
}

static void M_DrawDevmodeBack(void)
{
	INT32 x = 0, y = 0, i = 0;
	
	// black bg
	V_DrawFill(0, 0, BASEVIDWIDTH, BASEVIDHEIGHT, 31);
	
	V_SetClipRect(
		4 * FRACUNIT,
		5 * FRACUNIT,
		(BASEVIDWIDTH-8) * FRACUNIT,
		(BASEVIDHEIGHT-20) * FRACUNIT,
		0
	);
	
	// HAYA: TODO Rework this from it's initial impl to allow the damn thing to interpolate
	skidrow_tick_frac += R_UsingFrameInterpolation() ? renderdeltatics : FRACUNIT;
	
#define VIDWIDTH (BASEVIDWIDTH << FRACBITS)
#define VIDHEIGHT (BASEVIDHEIGHT << FRACBITS)
#define V_DrawStar(x, y, col) V_DrawStretchyFixedPatch(x, y, FRACUNIT/64, FRACUNIT/64, 0, col, NULL)

	int tickoffs = (skidrow_tick_frac * 3) % VIDWIDTH;
	
	for (x = 0; x < VIDWIDTH/2; x += FRACUNIT)
		for (y = 0; y < VIDHEIGHT/2; y += FRACUNIT)
			if (star_y[x>>FRACBITS][y>>FRACBITS])
				V_DrawStar(VIDWIDTH + x*2 - tickoffs, y*2, checkerpatches[3]);
			
	for (x = 0; x < VIDWIDTH/2; x += FRACUNIT)
		for (y = 0; y < VIDHEIGHT/2; y += FRACUNIT)
			if (star_y[x>>FRACBITS][y>>FRACBITS])
				V_DrawStar(x*2 - tickoffs, y*2, checkerpatches[3]);
			
	tickoffs = (skidrow_tick_frac * 5) % VIDWIDTH;
	
	for (x = 0; x < VIDWIDTH/2; x += FRACUNIT)
		for (y = 0; y < VIDHEIGHT/2; y += FRACUNIT)
			if (star_x[x>>FRACBITS][y>>FRACBITS])
				V_DrawStar(VIDWIDTH + x*2 - tickoffs, y*2, checkerpatches[2]);
			
	for (x = 0; x < VIDWIDTH/2; x += FRACUNIT)
		for (y = 0; y < VIDHEIGHT/2; y += FRACUNIT)
			if (star_x[x>>FRACBITS][y>>FRACBITS])
				V_DrawStar(x*2 - tickoffs, y*2, checkerpatches[2]);
			
#undef VIDWIDTH
#undef VIDHEIGHT
#undef V_DrawStar
	
	V_ClearClipRect();
	
	// Rainbow strips
	x = 0; y = 2; i = 0;
	
	// top
	while (x < BASEVIDWIDTH)
	{
		V_DrawFill(x, y, 10, 1, skidrow_rainbow_fill[(i+(skidrow.ticker))%16]);
		x += 10; i++;
	}
	
	x = BASEVIDWIDTH; i = 0; y = BASEVIDHEIGHT-13;
	
	// bottom
	while (x > 0)
	{
		V_DrawFill(x-10, y, 10, 1, skidrow_rainbow_fill[(i+(skidrow.ticker))%16]);
		x -= 10; i++;
	}
	
	// bottom text
	V_DrawThinString(4, BASEVIDHEIGHT-10, V_GRAYMAP, "Press \xBB \xBD to exit");
}

// CREDITS N SUCH
// SCROLLING TEXT GALLORE

std::array SkidrowTextColors = 
{
	V_REDMAP,
	V_ORANGEMAP,
	V_YELLOWMAP,
	0,
	V_GREENMAP,
	0,
	V_SKYMAP,
	V_BLUEMAP,
	V_LAVENDERMAP,
	V_MAGENTAMAP,
	0,
	V_GRAYMAP,
	V_TANMAP,
};

std::vector<std::string> DevmodeTopCredits = {
	"* KART ROW *",
	"PROUDLY PRESENTS",
	"ANOTHER LIGHTSPEED RELEASE",
	"                          ",
	"RING RACERS DEV MODE FINAL",
	"                          ",
	"PATCHED BY KANYX AND ACHIIRO OF GG",
	"INTRO PROGRAMMED BY HAYASAKA",
	"MUSIC BY RANDOM VOICE",
	"DISTRIBUTED BY KART ROW IN 2024",
	"    ",
	"PLEASE VISIT OUR BOARDS FOR MORE STUFF.",
	"SEE YOU IN ANOTHER INTRO !!!",
};

// One long strip.....
std::string DevmodeSinescroll = 
	"* KART ROW * AND OTHERS PROUDLY PRESENT    "
	"DR ROBOTNIK'S RING RACERS FROM KART KREW.              "
	"PATCHED BY KANYX AND ACHIIRO OF GG "
	"AND DISTRIBUTED BY KART ROW. "
	"INTRO GRACIOUSLY PROGRAMMED BY HAYASAKA "
	"WITH AMAZING MUSIC BY RANDOM VOICE. "
	"SPECIAL REGARDS TO KOMA AND CO FOR HELP "
	"               REGARDS TO: "
	"SUNFLOWER / COSMO / STAR / MAYO / AC / DIGGLE / "
	"HYOGS / RHOPHASOFA (HAPPY BIRTHDAY!) / BLADE / "
	"MIKU FROM PT / DGPLEX / APOLLO / BEPIS / SALMON / "
	"SATURN / SUPERSANIC / JOSEPH.... WE DIDN'T FORGET YOU THIS TIME "
	"                      THAT'S IT BYE......           ";
	
static void M_DevmodeTick(void)
{
	skidrow.ticker++; // This is obvious.
	
	if (skidrow.ticker < TICRATE/2)
		return;
	
	if (skidrow.linetime < DevmodeTopCredits[skidrow.curline].length())
		skidrow.linetime++;
	else
	{
		skidrow.waitline++;
		if (skidrow.waitline > TICRATE/2)
		{
			if (skidrow.curline > DevmodeTopCredits.size() - 2) return;
			skidrow.curline++;
			skidrow.linetime = 0;
			skidrow.waitline = 0;
		}
	}
}

static void M_DrawDevmode(void)
{
	int i;
	int yshift = 0;
	
#define V_BigAmigaStringWidth( string,option ) \
	V__IntegerStringWidth ( FRACUNIT,option,BIGAMIGA_FONT,string )
	
	fixed_t frac = R_UsingFrameInterpolation() ? renderdeltatics : FRACUNIT;
	
	if (skidrow.bottomtext_line < -(V_BigAmigaStringWidth(DevmodeSinescroll.c_str(), V_MONOSPACE) << FRACBITS))
		skidrow.bottomtext_line = BASEVIDWIDTH << FRACBITS;
	else
		skidrow.bottomtext_line -= 2 * frac;
	
#undef V_BigAmigaStringWidth
	
#define V_DrawBigAmigaString( x,y,option,string ) \
	V__DrawOneScaleString (x,y,FRACUNIT,option,NULL,BIGAMIGA_FONT,string)
	
	V_SetAmigaEffect(ANG2, 4, -7);
	V_DrawBigAmigaString(skidrow.bottomtext_line, ((BASEVIDHEIGHT-38)-7) << FRACBITS, V_TRANSLUCENT|V_GRAYMAP|V_MONOSPACE, DevmodeSinescroll.c_str());
	V_ClearAmigaEffect();
	
	V_SetAmigaEffect(ANG2, 4, 28);
	V_DrawBigAmigaString(skidrow.bottomtext_line, ((BASEVIDHEIGHT-70)-7) << FRACBITS, V_MONOSPACE, DevmodeSinescroll.c_str());
	V_ClearAmigaEffect();
	
#undef V_DrawBigAmigaString
	
	// draw all original text
	for (i = 0; i < skidrow.curline; i++)
	{
		using srb2::Draw;

		Draw::TextElement text(DevmodeTopCredits[i]);
		text.font(Draw::Font::kSkidrow);
		
		Draw(160, 10+yshift).flags(V_MONOSPACE|SkidrowTextColors[i]).align(Draw::Align::kCenter).text(text);
		
		yshift += 6;
	}
	
	// draw current text
	using srb2::Draw;
	
	std::string msg = DevmodeTopCredits[skidrow.curline];
	UINT8 sublen = skidrow.linetime;
	std::string submsg = msg.substr(0, sublen);
	
	Draw::TextElement text(submsg);
	text.font(Draw::Font::kSkidrow);
	
	// We don't exactly want auto alignment here. Do alignment ourselves!
#define V_MediumStringWidth( string,option ) \
	V__IntegerStringWidth ( FRACUNIT,option,MED_FONT,string )
	
	Draw(160-(V_MediumStringWidth(msg.c_str(), V_MONOSPACE)/2)-0.5, 10+yshift).flags(V_MONOSPACE|SkidrowTextColors[skidrow.curline]).text(text);
	
#undef V_MediumStringWidth
}

menu_t MISC_DevmodeDef = {
	sizeof (MISC_DevmodeMenu)/sizeof (menuitem_t),
	NULL,
	0,
	MISC_DevmodeMenu,
	0, 0,
	0, 0,
	MBF_SOUNDLESS|MBF_DRAWBGWHILEPLAYING,
	".",
	98, 0,
	M_DrawDevmode,
	M_DrawDevmodeBack,
	M_DevmodeTick,
	M_InitDevmode,
	NULL,
	NULL,
};
