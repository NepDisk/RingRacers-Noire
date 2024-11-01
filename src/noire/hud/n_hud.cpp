// RINGRACERS-NOIRE
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by NepDisk
// Copyright (C) 2018-2024 by Kart Krew
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------

#include "../n_hud.h"
#include "../../k_hud.h"
#include "../../r_draw.h"
#include "../../k_dialogue.h"
#include "../../z_zone.h"

patch_t *kp_oldpositionnum[10][7];
patch_t *kp_oldwinnernum[7];

void N_LoadOldPositionNumbers(void)
{
	int i,j;
	char buffer[9];
	// Position numbers
	sprintf(buffer, "K_POSNxx");
	for (i = 0; i < 10; i++)
	{
		buffer[6] = '0'+i;
		for (j = 0; j < 7; j++)
		{
			//sprintf(buffer, "K_POSN%d%d", i, j);
			buffer[7] = '0'+j;
			kp_oldpositionnum[i][j] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
		}
	}

	sprintf(buffer, "K_POSNWx");
	for (i = 0; i < 6; i++)
	{
		buffer[7] = '0'+i;
		kp_oldwinnernum[i] = (patch_t *) W_CachePatchName(buffer, PU_HUDGFX);
	}
}


// doesn't support 4p just like v1
void N_drawOldInput()
{
	static INT32 pn = 0;
	INT32 target = 0, splitflags = (V_SNAPTOBOTTOM|V_SNAPTORIGHT|V_SLIDEIN);
	INT32 x = BASEVIDWIDTH - 32, y = BASEVIDHEIGHT-24, offs, yoffs = 0, col;
	const INT32 accent1 = splitflags | skincolors[stplyr->skincolor].ramp[5];
	const INT32 accent2 = splitflags | skincolors[stplyr->skincolor].ramp[9];
	ticcmd_t *cmd = &stplyr->cmd;
	fixed_t slide = K_GetDialogueSlide(FRACUNIT);

	// Don't draw under dialouge boxes pls
	if (slide)
	{
		yoffs = -18;
		splitflags &= ~(V_SNAPTORIGHT);
	}

#define BUTTW 8
#define BUTTH 11

#define drawbutt(xoffs, butt, symb)\
	if (!stplyr->exiting && (cmd->buttons & butt))\
	{\
		offs = 2;\
		col = accent1;\
	}\
	else\
	{\
		offs = 0;\
		col = accent2;\
		V_DrawFill(x+(xoffs), y+BUTTH+yoffs, BUTTW-1, 2, splitflags|31);\
	}\
	V_DrawFill(x+(xoffs), y+offs+yoffs, BUTTW-1, BUTTH, col);\
	V_DrawFixedPatch((x+1+(xoffs))<<FRACBITS, (y+offs+1+yoffs)<<FRACBITS, FRACUNIT, splitflags, fontv[TINY_FONT].font[symb-HU_FONTSTART], NULL)

	drawbutt(-2*BUTTW, BT_ACCELERATE, 'A');
	drawbutt(  -BUTTW, BT_BRAKE,      'B');
	drawbutt(       0, BT_DRIFT,      'D');
	drawbutt(   BUTTW, BT_ATTACK,     'I');

#undef drawbutt

#undef BUTTW
#undef BUTTH

	y -= 1;

	if (stplyr->exiting || !cmd->turning) // no turn
		target = 0;
	else // turning of multiple strengths!
	{
		target = ((abs(cmd->turning) - 1)/125)+1;
		if (target > 4)
			target = 4;
		if (cmd->turning < 0)
			target = -target;
	}

	if (pn != target)
	{
		if (abs(pn - target) == 1)
			pn = target;
		else if (pn < target)
			pn += 2;
		else //if (pn > target)
			pn -= 2;
	}

	if (pn < 0)
	{
		splitflags |= V_FLIP; // right turn
		x--;
	}

	target = abs(pn);
	if (target > 4)
		target = 4;

	if (!stplyr->skincolor)
		V_DrawFixedPatch(x<<FRACBITS, (y+yoffs)<<FRACBITS, FRACUNIT, splitflags, kp_inputwheel[target], NULL);
	else
	{
		UINT8 *colormap = R_GetTranslationColormap(TC_DEFAULT, static_cast<skincolornum_t>(K_GetHudColor()), GTC_CACHE);
		V_DrawFixedPatch(x<<FRACBITS, (y+yoffs)<<FRACBITS, FRACUNIT, splitflags, kp_inputwheel[target], colormap);
	}
}

void N_DrawKartOldPositionNum(INT32 num)
{
	INT32 POSI_X = BASEVIDWIDTH  - 9;		// 268
	INT32 POSI_Y = BASEVIDHEIGHT - 9;		// 138

	INT32 POSI2_X = (BASEVIDWIDTH/2)-4;
	INT32 POSI2_Y = (BASEVIDHEIGHT/2)-26;

	boolean win = (stplyr->exiting && num == 1);
	//INT32 X = POSI_X;
	INT32 xoffs = (cv_drawinput.value) ? -48 : 0;
	INT32 W = SHORT(kp_oldpositionnum[0][0]->width);
	fixed_t scale = FRACUNIT;
	patch_t *localpatch = kp_oldpositionnum[0][0];
	INT32 fx = 0, fy = 0, fflags = 0;
	INT32 addOrSub = V_ADD;
	boolean flipdraw = false;	// flip the order we draw it in for MORE splitscreen bs. fun.
	boolean flipvdraw = false;	// used only for 2p splitscreen so overtaking doesn't make 1P's position fly off the screen.
	boolean overtake = false;

	if ((mapheaderinfo[gamemap - 1]->levelflags & LF_SUBTRACTNUM) == LF_SUBTRACTNUM)
	{
		addOrSub = V_SUBTRACT;
	}

	if (stplyr->positiondelay || stplyr->exiting)
	{
		scale *= 2;
		overtake = true;	// this is used for splitscreen stuff in conjunction with flipdraw.
	}

	if (r_splitscreen || (cv_drawinput.value && !r_splitscreen))
	{
		scale /= 2;
	}

	W = FixedMul(W<<FRACBITS, scale)>>FRACBITS;

	// pain and suffering defined below
	if (!r_splitscreen)
	{
		fx = POSI_X + xoffs;
		fy = BASEVIDHEIGHT - 8;
		fflags = V_SNAPTOBOTTOM|V_SNAPTORIGHT|V_SPLITSCREEN;
	}
	else if (r_splitscreen == 1)	// for this splitscreen, we'll use case by case because it's a bit different.
	{
		fx = POSI_X;
		if (stplyr == &players[displayplayers[0]])	// for player 1: display this at the top right, above the minimap.
		{
			fy = 30;
			fflags = V_SNAPTOTOP|V_SNAPTORIGHT|V_SPLITSCREEN;
			if (overtake)
				flipvdraw = true;	// make sure overtaking doesn't explode us
		}
		else	// if we're not p1, that means we're p2. display this at the bottom right, below the minimap.
		{
			fy = (BASEVIDHEIGHT/2) - 8;
			fflags = V_SNAPTOBOTTOM|V_SNAPTORIGHT|V_SPLITSCREEN;
		}
	}
	else
	{
		if (stplyr == &players[displayplayers[0]] || stplyr == &players[displayplayers[2]])	// If we are P1 or P3...
		{
			fx = POSI_X;
			fy = POSI_Y;
			fflags = V_SNAPTOLEFT|V_SNAPTOBOTTOM|V_SPLITSCREEN;
			flipdraw = true;
			if (num && num >= 10)
				fx += W;	// this seems dumb, but we need to do this in order for positions above 10 going off screen.
		}
		else // else, that means we're P2 or P4.
		{
			fx = POSI2_X;
			fy = POSI2_Y;
			fflags = V_SNAPTORIGHT|V_SNAPTOBOTTOM|V_SPLITSCREEN;
		}
	}

	// Special case for 0
	if (num <= 0)
	{
		V_DrawFixedPatch(fx<<FRACBITS, fy<<FRACBITS, scale, addOrSub|fflags, kp_oldpositionnum[0][0], NULL);
		return;
	}

	// Draw the number
	while (num)
	{
		if (win) // 1st place winner? You get rainbows!!
		{
			localpatch = kp_oldwinnernum[(leveltime % (6*3)) / 3];
		}
		else if (stplyr->laps >= numlaps || stplyr->exiting) // Check for the final lap, or won
		{
			boolean useRedNums = K_IsPlayerLosing(stplyr);

			if (addOrSub == V_SUBTRACT)
			{
				// Subtracting RED will look BLUE, and vice versa.
				useRedNums = !useRedNums;
			}

			// Alternate frame every three frames
			switch ((leveltime % 9) / 3)
			{
				case 0:
					if (useRedNums == true)
						localpatch = kp_oldpositionnum[num % 10][4];
					else
						localpatch = kp_oldpositionnum[num % 10][1];
					break;
				case 1:
					if (useRedNums == true)
						localpatch = kp_oldpositionnum[num % 10][5];
					else
						localpatch = kp_oldpositionnum[num % 10][2];
					break;
				case 2:
					if (useRedNums == true)
						localpatch = kp_oldpositionnum[num % 10][6];
					else
						localpatch = kp_oldpositionnum[num % 10][3];
					break;
				default:
					localpatch = kp_oldpositionnum[num % 10][0];
					break;
			}
		}
		else
		{
			localpatch = kp_oldpositionnum[num % 10][0];
		}

		V_DrawFixedPatch(
			(fx<<FRACBITS) + ((overtake && flipdraw) ? (SHORT(localpatch->width)*scale/2) : 0),
			(fy<<FRACBITS) + ((overtake && flipvdraw) ? (SHORT(localpatch->height)*scale/2) : 0),
			scale, addOrSub|V_HUDTRANSHALF|fflags, localpatch, NULL
		);
		// ^ if we overtake as p1 or p3 in splitscren, we shift it so that it doesn't go off screen.
		// ^ if we overtake as p1 in 2p splits, shift vertically so that this doesn't happen either.

		fx -= W;
		num /= 10;
	}
}
