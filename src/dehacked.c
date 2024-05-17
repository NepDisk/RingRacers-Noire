// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2020 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  dehacked.c
/// \brief Load dehacked file and change tables and text

#include "doomdef.h"
#include "d_main.h" // for srb2home
#include "g_game.h"
#include "sounds.h"
#include "info.h"
#include "d_think.h"
#include "m_argv.h"
#include "z_zone.h"
#include "w_wad.h"
#include "m_menu.h"
#include "m_misc.h"
#include "f_finale.h"
#include "y_inter.h"
#include "dehacked.h"
#include "st_stuff.h"
#include "i_system.h"
#include "p_local.h" // for var1 and var2, and some constants
#include "p_setup.h"
#include "r_data.h"
#include "r_textures.h"
#include "r_draw.h"
#include "r_picformats.h"
#include "r_things.h" // R_Char2Frame
#include "r_sky.h"
#include "fastcmp.h"
#include "lua_script.h"
#include "lua_hook.h"
#include "d_clisrv.h"

#include "m_cond.h"

#include "v_video.h" // video flags (for lua)

#ifdef HWRENDER
#include "hardware/hw_light.h"
#endif

// SRB2Kart
#include "filesrch.h" // for refreshdirmenu
#include "r_skins.h" // for followers

// Free slot names
// The crazy word-reading stuff uses these.
static char *FREE_STATES[NUMSTATEFREESLOTS];
static char *FREE_MOBJS[NUMMOBJFREESLOTS];
static char *FREE_SKINCOLORS[NUMCOLORFREESLOTS];
static UINT8 used_spr[(NUMSPRITEFREESLOTS / 8) + 1]; // Bitwise flag for sprite freeslot in use! I would use ceil() here if I could, but it only saves 1 byte of memory anyway.
#define initfreeslots() {\
memset(FREE_STATES,0,sizeof(char *) * NUMSTATEFREESLOTS);\
memset(FREE_MOBJS,0,sizeof(char *) * NUMMOBJFREESLOTS);\
memset(FREE_SKINCOLORS,0,sizeof(char *) * NUMCOLORFREESLOTS);\
memset(used_spr,0,sizeof(UINT8) * ((NUMSPRITEFREESLOTS / 8) + 1));\
}

// Crazy word-reading stuff
/// \todo Put these in a seperate file or something.
static mobjtype_t get_mobjtype(const char *word);
static statenum_t get_state(const char *word);
static spritenum_t get_sprite(const char *word);
static playersprite_t get_sprite2(const char *word);
static sfxenum_t get_sfx(const char *word);
#ifdef MUSICSLOT_COMPATIBILITY
static UINT16 get_mus(const char *word, UINT8 dehacked_mode);
#endif
static hudnum_t get_huditem(const char *word);
static menutype_t get_menutype(const char *word);
//static INT16 get_gametype(const char *word);
//static powertype_t get_power(const char *word);
skincolornum_t get_skincolor(const char *word);

boolean deh_loaded = false;
static int dbg_line;

static boolean gamedataadded = false;
static boolean titlechanged = false;
static boolean introchanged = false;

ATTRINLINE static FUNCINLINE char myfget_color(MYFILE *f)
{
	char c = *f->curpos++;
	if (c == '^') // oh, nevermind then.
		return '^';

	if (c >= '0' && c <= '9')
		return 0x80+(c-'0');
	return 0x80; // Unhandled -- default to no color
}

ATTRINLINE static FUNCINLINE char myfget_hex(MYFILE *f)
{
	char c = *f->curpos++, endchr = 0;
	if (c == '\\') // oh, nevermind then.
		return '\\';

	if (c >= '0' && c <= '9')
		endchr += (c-'0') << 4;
	else if (c >= 'A' && c <= 'F')
		endchr += ((c-'A') + 10) << 4;
	else if (c >= 'a' && c <= 'f')
		endchr += ((c-'a') + 10) << 4;
	else // invalid. stop and return a question mark.
		return '?';

	c = *f->curpos++;
	if (c >= '0' && c <= '9')
		endchr += (c-'0');
	else if (c >= 'A' && c <= 'F')
		endchr += ((c-'A') + 10);
	else if (c >= 'a' && c <= 'f')
		endchr += ((c-'a') + 10);
	else // invalid. stop and return a question mark.
		return '?';

	return endchr;
}

char *myfgets(char *buf, size_t bufsize, MYFILE *f)
{
	size_t i = 0;
	if (myfeof(f))
		return NULL;
	// we need one byte for a null terminated string
	bufsize--;
	while (i < bufsize && !myfeof(f))
	{
		char c = *f->curpos++;
		if (c == '^')
			buf[i++] = myfget_color(f);
		else if (c == '\\')
			buf[i++] = myfget_hex(f);
		else if (c != '\r')
			buf[i++] = c;
		if (c == '\n')
			break;
	}
	buf[i] = '\0';

	dbg_line++;
	return buf;
}

static char *myhashfgets(char *buf, size_t bufsize, MYFILE *f)
{
	size_t i = 0;
	if (myfeof(f))
		return NULL;
	// we need one byte for a null terminated string
	bufsize--;
	while (i < bufsize && !myfeof(f))
	{
		char c = *f->curpos++;
		if (c == '^')
			buf[i++] = myfget_color(f);
		else if (c == '\\')
			buf[i++] = myfget_hex(f);
		else if (c != '\r')
			buf[i++] = c;
		if (c == '\n') // Ensure debug line is right...
			dbg_line++;
		if (c == '#')
		{
			if (i > 0) // don't let i wrap past 0
				i--; // don't include hash char in string
			break;
		}
	}
	if (buf[i] != '#') // don't include hash char in string
		i++;
	buf[i] = '\0';

	return buf;
}

static INT32 deh_num_warning = 0;

FUNCPRINTF static void deh_warning(const char *first, ...)
{
	va_list argptr;
	char *buf = Z_Malloc(1000, PU_STATIC, NULL);

	va_start(argptr, first);
	vsnprintf(buf, 1000, first, argptr); // sizeof only returned 4 here. it didn't like that pointer.
	va_end(argptr);

	if(dbg_line == -1) // Not in a SOC, line number unknown.
		CONS_Alert(CONS_WARNING, "%s\n", buf);
	else
		CONS_Alert(CONS_WARNING, "Line %u: %s\n", dbg_line, buf);

	deh_num_warning++;

	Z_Free(buf);
}

static void deh_strlcpy(char *dst, const char *src, size_t size, const char *warntext)
{
	size_t len = strlen(src)+1; // Used to determine if truncation has been done
	if (len > size)
		deh_warning("%s exceeds max length of %s", warntext, sizeu1(size-1));
	strlcpy(dst, src, size);
}

/* ======================================================================== */
// Load a dehacked file format
/* ======================================================================== */
/* a sample to see
                   Thing 1 (Player)       {           // MT_PLAYER
INT32 doomednum;     ID # = 3232              -1,             // doomednum
INT32 spawnstate;    Initial frame = 32       "PLAY",         // spawnstate
INT32 spawnhealth;   Hit points = 3232        100,            // spawnhealth
INT32 seestate;      First moving frame = 32  "PLAY_RUN1",    // seestate
INT32 seesound;      Alert sound = 32         sfx_None,       // seesound
INT32 reactiontime;  Reaction time = 3232     0,              // reactiontime
INT32 attacksound;   Attack sound = 32        sfx_None,       // attacksound
INT32 painstate;     Injury frame = 32        "PLAY_PAIN",    // painstate
INT32 painchance;    Pain chance = 3232       255,            // painchance
INT32 painsound;     Pain sound = 32          sfx_plpain,     // painsound
INT32 meleestate;    Close attack frame = 32  "NULL",         // meleestate
INT32 missilestate;  Far attack frame = 32    "PLAY_ATK1",    // missilestate
INT32 deathstate;    Death frame = 32         "PLAY_DIE1",    // deathstate
INT32 xdeathstate;   Exploding frame = 32     "PLAY_XDIE1",   // xdeathstate
INT32 deathsound;    Death sound = 32         sfx_pldeth,     // deathsound
INT32 speed;         Speed = 3232             0,              // speed
INT32 radius;        Width = 211812352        16*FRACUNIT,    // radius
INT32 height;        Height = 211812352       56*FRACUNIT,    // height
INT32 dispoffset;    DispOffset = 0           0,              // dispoffset
INT32 mass;          Mass = 3232              100,            // mass
INT32 damage;        Missile damage = 3232    0,              // damage
INT32 activesound;   Action sound = 32        sfx_None,       // activesound
INT32 flags;         Bits = 3232              MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH,
INT32 raisestate;    Respawn frame = 32       S_NULL          // raisestate
                                         }, */

#ifdef HWRENDER
static INT32 searchvalue(const char *s)
{
	while (s[0] != '=' && s[0])
		s++;
	if (s[0] == '=')
		return atoi(&s[1]);
	else
	{
		deh_warning("No value found");
		return 0;
	}
}

static float searchfvalue(const char *s)
{
	while (s[0] != '=' && s[0])
		s++;
	if (s[0] == '=')
		return (float)atof(&s[1]);
	else
	{
		deh_warning("No value found");
		return 0;
	}
}
#endif

// These are for clearing all of various things
static void clear_conditionsets(void)
{
	UINT8 i;
	for (i = 0; i < MAXCONDITIONSETS; ++i)
		M_ClearConditionSet(i+1);
}

static void clear_levels(void)
{
	INT16 i;

	// This is potentially dangerous but if we're resetting these headers,
	// we may as well try to save some memory, right?
	for (i = 0; i < NUMMAPS; ++i)
	{
		if (!mapheaderinfo[i] || i == (tutorialmap-1))
			continue;

		// Custom map header info
		// (no need to set num to 0, we're freeing the entire header shortly)
		Z_Free(mapheaderinfo[i]->customopts);

		P_DeleteFlickies(i);
		P_DeleteGrades(i);

		Z_Free(mapheaderinfo[i]);
		mapheaderinfo[i] = NULL;
	}

	// Realloc the one for the current gamemap as a safeguard
	P_AllocMapHeader(gamemap-1);
}

static boolean findFreeSlot(INT32 *num)
{
	// Send the character select entry to a free slot.
	while (*num < MAXSKINS && (description[*num].used))
		*num = *num+1;

	// No more free slots. :(
	if (*num >= MAXSKINS)
		return false;

	// Redesign your logo. (See M_DrawSetupChoosePlayerMenu in m_menu.c...)
	description[*num].picname[0] = '\0';
	description[*num].nametag[0] = '\0';
	description[*num].displayname[0] = '\0';
	description[*num].oppositecolor = SKINCOLOR_NONE;
	description[*num].tagtextcolor = SKINCOLOR_NONE;
	description[*num].tagoutlinecolor = SKINCOLOR_NONE;

	// Found one! ^_^
	return (description[*num].used = true);
}

// Reads a player.
// For modifying the character select screen
static void readPlayer(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *displayname = ZZ_Alloc(MAXLINELEN+1);
	INT32 i;
	boolean slotfound = false;

	#define SLOTFOUND \
		if (!slotfound && (slotfound = findFreeSlot(&num)) == false) \
			goto done;

	displayname[MAXLINELEN] = '\0';

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			for (i = 0; i < MAXLINELEN-3; i++)
			{
				char *tmp;
				if (s[i] == '=')
				{
					tmp = &s[i+2];
					strncpy(displayname, tmp, SKINNAMESIZE);
					break;
				}
			}

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			if (fastcmp(word, "PLAYERTEXT"))
			{
				char *playertext = NULL;

				SLOTFOUND

				for (i = 0; i < MAXLINELEN-3; i++)
				{
					if (s[i] == '=')
					{
						playertext = &s[i+2];
						break;
					}
				}
				if (playertext)
				{
					strcpy(description[num].notes, playertext);
					strcat(description[num].notes, myhashfgets(playertext, sizeof (description[num].notes), f));
				}
				else
					strcpy(description[num].notes, "");

				// For some reason, cutting the string did not work above. Most likely due to strcpy or strcat...
				// It works down here, though.
				{
					INT32 numline = 0;
					for (i = 0; (size_t)i < sizeof(description[num].notes)-1; i++)
					{
						if (numline < 20 && description[num].notes[i] == '\n')
							numline++;

						if (numline >= 20 || description[num].notes[i] == '\0' || description[num].notes[i] == '#')
							break;
					}
				}
				description[num].notes[strlen(description[num].notes)-1] = '\0';
				description[num].notes[i] = '\0';
				continue;
			}

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';
			i = atoi(word2);

			if (fastcmp(word, "PICNAME"))
			{
				SLOTFOUND
				strncpy(description[num].picname, word2, 8);
			}
			// new character select
			else if (fastcmp(word, "DISPLAYNAME"))
			{
				SLOTFOUND
				// replace '#' with line breaks
				// (also remove any '\n')
				{
					char *cur = NULL;

					// remove '\n'
					cur = strchr(displayname, '\n');
					if (cur)
						*cur = '\0';

					// turn '#' into '\n'
					cur = strchr(displayname, '#');
					while (cur)
					{
						*cur = '\n';
						cur = strchr(cur, '#');
					}
				}
				// copy final string
				strncpy(description[num].displayname, displayname, SKINNAMESIZE);
			}
			else if (fastcmp(word, "OPPOSITECOLOR") || fastcmp(word, "OPPOSITECOLOUR"))
			{
				SLOTFOUND
				description[num].oppositecolor = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "NAMETAG") || fastcmp(word, "TAGNAME"))
			{
				SLOTFOUND
				strncpy(description[num].nametag, word2, 8);
			}
			else if (fastcmp(word, "TAGTEXTCOLOR") || fastcmp(word, "TAGTEXTCOLOUR"))
			{
				SLOTFOUND
				description[num].tagtextcolor = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "TAGOUTLINECOLOR") || fastcmp(word, "TAGOUTLINECOLOUR"))
			{
				SLOTFOUND
				description[num].tagoutlinecolor = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "STATUS"))
			{
				/*
					You MAY disable previous entries if you so desire...
					But try to enable something that's already enabled and you will be sent to a free slot.

					Because of this, you are allowed to edit any previous entries you like, but only if you
					signal that you are purposely doing so by disabling and then reenabling the slot.
				*/
				if (i && !slotfound && (slotfound = findFreeSlot(&num)) == false)
					goto done;

				description[num].used = (!!i);
			}
			else if (fastcmp(word, "SKINNAME"))
			{
				// Send to free slot.
				SLOTFOUND
				strlcpy(description[num].skinname, word2, sizeof description[num].skinname);
				strlwr(description[num].skinname);
			}
			else
				deh_warning("readPlayer %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty
	#undef SLOTFOUND
done:
	Z_Free(displayname);
	Z_Free(s);
}

static int freeslotusage[2][2] = {{0, 0}, {0, 0}}; // [S_, MT_][max, previous .wad's max]

void DEH_UpdateMaxFreeslots(void)
{
	freeslotusage[0][1] = freeslotusage[0][0];
	freeslotusage[1][1] = freeslotusage[1][0];
}

// TODO: Figure out how to do undolines for this....
// TODO: Warnings for running out of freeslots
static void readfreeslots(MYFILE *f)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word,*type;
	char *tmp;
	int i;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			type = strtok(s, "_");
			if (type)
				strupr(type);
			else
				break;

			word = strtok(NULL, "\n");
			if (word)
				strupr(word);
			else
				break;

			// TODO: Check for existing freeslot mobjs/states/etc. and make errors.
			// TODO: Out-of-slots warnings/errors.
			// TODO: Name too long (truncated) warnings.
			if (fastcmp(type, "SFX"))
				S_AddSoundFx(word, false, 0, false);
			else if (fastcmp(type, "SPR"))
			{
				for (i = SPR_FIRSTFREESLOT; i <= SPR_LASTFREESLOT; i++)
				{
					if (used_spr[(i-SPR_FIRSTFREESLOT)/8] & (1<<(i%8)))
					{
						if (!sprnames[i][4] && memcmp(sprnames[i],word,4)==0)
							sprnames[i][4] = (char)f->wad;
						continue; // Already allocated, next.
					}
					// Found a free slot!
					strncpy(sprnames[i],word,4);
					//sprnames[i][4] = 0;
					used_spr[(i-SPR_FIRSTFREESLOT)/8] |= 1<<(i%8); // Okay, this sprite slot has been named now.
					break;
				}
			}
			else if (fastcmp(type, "S"))
			{
				for (i = 0; i < NUMSTATEFREESLOTS; i++)
					if (!FREE_STATES[i]) {
						FREE_STATES[i] = Z_Malloc(strlen(word)+1, PU_STATIC, NULL);
						strcpy(FREE_STATES[i],word);
						freeslotusage[0][0]++;
						break;
					}
			}
			else if (fastcmp(type, "MT"))
			{
				for (i = 0; i < NUMMOBJFREESLOTS; i++)
					if (!FREE_MOBJS[i]) {
						FREE_MOBJS[i] = Z_Malloc(strlen(word)+1, PU_STATIC, NULL);
						strcpy(FREE_MOBJS[i],word);
						freeslotusage[1][0]++;
						break;
					}
			}
			else if (fastcmp(type, "SKINCOLOR"))
			{
				for (i = 0; i < NUMCOLORFREESLOTS; i++)
					if (!FREE_SKINCOLORS[i]) {
						FREE_SKINCOLORS[i] = Z_Malloc(strlen(word)+1, PU_STATIC, NULL);
						strcpy(FREE_SKINCOLORS[i],word);
						M_AddMenuColor(numskincolors++);
						break;
					}
			}
			else if (fastcmp(type, "SPR2"))
			{
				// Search if we already have an SPR2 by that name...
				for (i = SPR2_FIRSTFREESLOT; i < (int)free_spr2; i++)
					if (memcmp(spr2names[i],word,4) == 0)
						break;
				// We found it? (Two mods using the same SPR2 name?) Then don't allocate another one.
				if (i < (int)free_spr2)
					continue;
				// Copy in the spr2 name and increment free_spr2.
				if (free_spr2 < NUMPLAYERSPRITES) {
					strncpy(spr2names[free_spr2],word,4);
					spr2defaults[free_spr2] = 0;
					spr2names[free_spr2++][4] = 0;
				} else
					deh_warning("Ran out of free SPR2 slots!\n");
			}
			else if (fastcmp(type, "TOL"))
			{
				// Search if we already have a typeoflevel by that name...
				for (i = 0; TYPEOFLEVEL[i].name; i++)
					if (fastcmp(word, TYPEOFLEVEL[i].name))
						break;

				// We found it? Then don't allocate another one.
				if (TYPEOFLEVEL[i].name)
					continue;

				// We don't, so freeslot it.
				if (lastcustomtol == (UINT32)MAXTOL) // Unless you have way too many, since they're flags.
					deh_warning("Ran out of free typeoflevel slots!\n");
				else
				{
					G_AddTOL(lastcustomtol, word);
					lastcustomtol <<= 1;
				}
			}
			else
				deh_warning("Freeslots: unknown enum class '%s' for '%s_%s'", type, type, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

// This here is our current only way to make followers.
INT32 numfollowers = 0;

static void readfollower(MYFILE *f)
{
	char *s;
	char *word, *word2, dname[SKINNAMESIZE+1];
	char *tmp;
	char testname[SKINNAMESIZE];

	boolean nameset;
	INT32 fallbackstate = 0;
	INT32 res;
	INT32 i;

	if (numfollowers > MAXSKINS)
	{
		deh_warning("Error: Too many followers, cannot add anymore.\n");
		return;
	}

	s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);

	// Ready the default variables for followers. We will overwrite them as we go! We won't set the name or states RIGHT HERE as this is handled down instead.
	followers[numfollowers].scale = FRACUNIT;
	followers[numfollowers].bubblescale = 0;	// No bubble by default
	followers[numfollowers].atangle = 230;
	followers[numfollowers].dist = 32;		// changed from 16 to 32 to better account for ogl models
	followers[numfollowers].height = 16;
	followers[numfollowers].zoffs = 32;
	followers[numfollowers].horzlag = 2;
	followers[numfollowers].vertlag = 6;
	followers[numfollowers].bobspeed = TICRATE*2;
	followers[numfollowers].bobamp = 4;
	followers[numfollowers].hitconfirmtime = TICRATE;
	followers[numfollowers].defaultcolor = SKINCOLOR_GREEN;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " = ");

			if (!word2)
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';

			if (fastcmp(word, "NAME"))
			{
				strcpy(followers[numfollowers].name, word2);
				nameset = true;
			}
			else if (fastcmp(word, "DEFAULTCOLOR"))
			{
				followers[numfollowers].defaultcolor = (UINT16)get_number(word2);
			}

			else if (fastcmp(word, "SCALE"))
			{
				followers[numfollowers].scale = get_number(word2);
			}
			else if (fastcmp(word, "BUBBLESCALE"))
			{
				followers[numfollowers].bubblescale = get_number(word2);
			}
			else if (fastcmp(word, "ATANGLE"))
			{
				followers[numfollowers].atangle = (INT32)atoi(word2);
			}
			else if (fastcmp(word, "HORZLAG"))
			{
				followers[numfollowers].horzlag = (INT32)atoi(word2);
			}
			else if (fastcmp(word, "VERTLAG"))
			{
				followers[numfollowers].vertlag = (INT32)atoi(word2);
			}
			else if (fastcmp(word, "BOBSPEED"))
			{
				followers[numfollowers].bobspeed = (INT32)atoi(word2);
			}
			else if (fastcmp(word, "BOBAMP"))
			{
				followers[numfollowers].bobamp = (INT32)atoi(word2);
			}
			else if (fastcmp(word, "ZOFFSET") || (fastcmp(word, "ZOFFS")))
			{
				followers[numfollowers].zoffs = (INT32)atoi(word2);
			}
			else if (fastcmp(word, "DISTANCE") || (fastcmp(word, "DIST")))
			{
				followers[numfollowers].dist = (INT32)atoi(word2);
			}
			else if (fastcmp(word, "HEIGHT"))
			{
				followers[numfollowers].height = (INT32)atoi(word2);
			}
			else if (fastcmp(word, "IDLESTATE"))
			{
				if (word2)
					strupr(word2);
				followers[numfollowers].idlestate = get_number(word2);
				fallbackstate = followers[numfollowers].idlestate;
			}
			else if (fastcmp(word, "FOLLOWSTATE"))
			{
				if (word2)
					strupr(word2);
				followers[numfollowers].followstate = get_number(word2);
			}
			else if (fastcmp(word, "HURTSTATE"))
			{
				if (word2)
					strupr(word2);
				followers[numfollowers].hurtstate = get_number(word2);
			}
			else if (fastcmp(word, "LOSESTATE"))
			{
				if (word2)
					strupr(word2);
				followers[numfollowers].losestate = get_number(word2);
			}
			else if (fastcmp(word, "WINSTATE"))
			{
				if (word2)
					strupr(word2);
				followers[numfollowers].winstate = get_number(word2);
			}
			else if (fastcmp(word, "HITSTATE") || (fastcmp(word, "HITCONFIRMSTATE")))
			{
				if (word2)
					strupr(word2);
				followers[numfollowers].hitconfirmstate = get_number(word2);
			}
			else if (fastcmp(word, "HITTIME") || (fastcmp(word, "HITCONFIRMTIME")))
			{
				followers[numfollowers].hitconfirmtime = (INT32)atoi(word2);
			}
			else
				deh_warning("Follower %d: unknown word '%s'", numfollowers, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	if (!nameset)	// well this is problematic.
	{
		strcpy(followers[numfollowers].name, va("Follower%d", numfollowers));	// this is lazy, so what
	}

	// set skin name (this is just the follower's name in lowercases):
	// but before we do, let's... actually check if another follower isn't doing the same shit...

	strcpy(testname, followers[numfollowers].name);

	// lower testname for skin checks...
	strlwr(testname);
	res = R_FollowerAvailable(testname);
	if (res > -1)	// yikes, someone else has stolen our name already
	{
		INT32 startlen = strlen(testname);
		char cpy[2];
		//deh_warning("There was already a follower with the same name. (%s)", testname);	This warning probably isn't necessary anymore?
		sprintf(cpy, "%d", numfollowers);
		memcpy(&testname[startlen], cpy, 2);
		// in that case, we'll be very lazy and copy numfollowers to the end of our skin name.
	}

	strcpy(followers[numfollowers].skinname, testname);
	strcpy(dname, followers[numfollowers].skinname);	// display name, just used for printing succesful stuff or errors later down the line.

	// now that the skin name is ready, post process the actual name to turn the underscores into spaces!
	for (i = 0; followers[numfollowers].name[i]; i++)
	{
		if (followers[numfollowers].name[i] == '_')
			followers[numfollowers].name[i] = ' ';
	}

	// fallbacks for variables
	// Print a warning if the variable is on a weird value and set it back to the minimum available if that's the case.
#define FALLBACK(field, field2, threshold, set) \
if (followers[numfollowers].field < threshold) \
{ \
	followers[numfollowers].field = set; \
	deh_warning("Follower '%s': Value for '%s' is too low! Minimum should be %d. Value was overwritten to %d.", dname, field2, set, set); \
} \

	FALLBACK(dist, "DIST", 0, 0);
	FALLBACK(height, "HEIGHT", 1, 1);
	FALLBACK(zoffs, "ZOFFS", 0, 0);
	FALLBACK(horzlag, "HORZLAG", 1, 1);
	FALLBACK(vertlag, "VERTLAG", 1, 1);
	FALLBACK(bobamp, "BOBAMP", 0, 0);
	FALLBACK(bobspeed, "BOBSPEED", 0, 0);
	FALLBACK(hitconfirmtime, "HITCONFIRMTIME", 1, 1);
	FALLBACK(scale, "SCALE", 1, 1);				// No null/negative scale
	FALLBACK(bubblescale, "BUBBLESCALE", 0, 0);	// No negative scale

	// Special case for color I suppose
	if (followers[numfollowers].defaultcolor > numskincolors-1)
	{
		followers[numfollowers].defaultcolor = SKINCOLOR_GREEN;
		deh_warning("Follower \'%s\': Value for 'color' should be between 1 and %d.\n", dname, numskincolors-1);
	}

#undef FALLBACK

	// also check if we forgot states. If we did, we will set any missing state to the follower's idlestate.
	// Print a warning in case we don't have a fallback and set the state to S_INVISIBLE (rather than S_NULL) if unavailable.

#define NOSTATE(field, field2) \
if (!followers[numfollowers].field) \
{ \
	followers[numfollowers].field = fallbackstate ? fallbackstate : S_INVISIBLE; \
	if (!fallbackstate) \
		deh_warning("Follower '%s' is missing state definition for '%s', no idlestate fallback was found", dname, field2); \
} \

	NOSTATE(idlestate, "IDLESTATE");
	NOSTATE(followstate, "FOLLOWSTATE");
	NOSTATE(hurtstate, "HURTSTATE");
	NOSTATE(losestate, "LOSESTATE");
	NOSTATE(winstate, "WINSTATE");
	NOSTATE(hitconfirmstate, "HITCONFIRMSTATE");
#undef NOSTATE

	CONS_Printf("Added follower '%s'\n", dname);
	numfollowers++;	// add 1 follower
	Z_Free(s);
}

static void readthing(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word, *word2;
	char *tmp;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;
			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';

			if (fastcmp(word, "MAPTHINGNUM") || fastcmp(word, "DOOMEDNUM"))
			{
				mobjinfo[num].doomednum = (INT32)atoi(word2);
			}
			else if (fastcmp(word, "SPAWNSTATE"))
			{
				mobjinfo[num].spawnstate = get_number(word2);
			}
			else if (fastcmp(word, "SPAWNHEALTH"))
			{
				mobjinfo[num].spawnhealth = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "SEESTATE"))
			{
				mobjinfo[num].seestate = get_number(word2);
			}
			else if (fastcmp(word, "SEESOUND"))
			{
				mobjinfo[num].seesound = get_number(word2);
			}
			else if (fastcmp(word, "REACTIONTIME"))
			{
				mobjinfo[num].reactiontime = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "ATTACKSOUND"))
			{
				mobjinfo[num].attacksound = get_number(word2);
			}
			else if (fastcmp(word, "PAINSTATE"))
			{
				mobjinfo[num].painstate = get_number(word2);
			}
			else if (fastcmp(word, "PAINCHANCE"))
			{
				mobjinfo[num].painchance = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "PAINSOUND"))
			{
				mobjinfo[num].painsound = get_number(word2);
			}
			else if (fastcmp(word, "MELEESTATE"))
			{
				mobjinfo[num].meleestate = get_number(word2);
			}
			else if (fastcmp(word, "MISSILESTATE"))
			{
				mobjinfo[num].missilestate = get_number(word2);
			}
			else if (fastcmp(word, "DEATHSTATE"))
			{
				mobjinfo[num].deathstate = get_number(word2);
			}
			else if (fastcmp(word, "DEATHSOUND"))
			{
				mobjinfo[num].deathsound = get_number(word2);
			}
			else if (fastcmp(word, "XDEATHSTATE"))
			{
				mobjinfo[num].xdeathstate = get_number(word2);
			}
			else if (fastcmp(word, "SPEED"))
			{
				mobjinfo[num].speed = get_number(word2);
			}
			else if (fastcmp(word, "RADIUS"))
			{
				mobjinfo[num].radius = get_number(word2);
			}
			else if (fastcmp(word, "HEIGHT"))
			{
				mobjinfo[num].height = get_number(word2);
			}
			else if (fastcmp(word, "DISPOFFSET"))
			{
				mobjinfo[num].dispoffset = get_number(word2);
			}
			else if (fastcmp(word, "MASS"))
			{
				mobjinfo[num].mass = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "DAMAGE"))
			{
				mobjinfo[num].damage = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "ACTIVESOUND"))
			{
				mobjinfo[num].activesound = get_number(word2);
			}
			else if (fastcmp(word, "FLAGS"))
			{
				mobjinfo[num].flags = (INT32)get_number(word2);
			}
			else if (fastcmp(word, "RAISESTATE"))
			{
				mobjinfo[num].raisestate = get_number(word2);
			}
			else
				deh_warning("Thing %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readskincolor(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;

	Color_cons_t[num].value = num;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;

			if (fastcmp(word, "NAME"))
			{
				size_t namesize = sizeof(skincolors[num].name);
				char truncword[namesize];
				UINT16 dupecheck;

				deh_strlcpy(truncword, word2, namesize, va("Skincolor %d: name", num)); // truncate here to check for dupes
				dupecheck = R_GetColorByName(truncword);
				if (truncword[0] != '\0' && (!stricmp(truncword, skincolors[SKINCOLOR_NONE].name) || (dupecheck && dupecheck != num)))
				{
					size_t lastchar = strlen(truncword);
					char oldword[lastchar+1];
					char dupenum = '1';

					strlcpy(oldword, truncword, lastchar+1);
					lastchar--;
					if (lastchar == namesize-2) // exactly max length, replace last character with 0
						truncword[lastchar] = '0';
					else // append 0
					{
						strcat(truncword, "0");
						lastchar++;
					}

					while (R_GetColorByName(truncword))
					{
						truncword[lastchar] = dupenum;
						if (dupenum == '9')
							dupenum = 'A';
						else if (dupenum == 'Z') // give up :?
							break;
						else
							dupenum++;
					}

					deh_warning("Skincolor %d: name %s is a duplicate of another skincolor's name - renamed to %s", num, oldword, truncword);
				}

				strlcpy(skincolors[num].name, truncword, namesize); // already truncated
			}
			else if (fastcmp(word, "RAMP"))
			{
				UINT8 i;
				tmp = strtok(word2,",");
				for (i = 0; i < COLORRAMPSIZE; i++) {
					skincolors[num].ramp[i] = (UINT8)get_number(tmp);
					if ((tmp = strtok(NULL,",")) == NULL)
						break;
				}
				skincolor_modified[num] = true;
			}
			else if (fastcmp(word, "INVCOLOR"))
			{
				UINT16 v = (UINT16)get_number(word2);
				if (v < numskincolors)
					skincolors[num].invcolor = v;
				else
					skincolors[num].invcolor = SKINCOLOR_GREEN;
			}
			else if (fastcmp(word, "INVSHADE"))
			{
				skincolors[num].invshade = get_number(word2)%COLORRAMPSIZE;
			}
			else if (fastcmp(word, "CHATCOLOR"))
			{
				skincolors[num].chatcolor = get_number(word2);
			}
			else if (fastcmp(word, "ACCESSIBLE"))
			{
				if (num > FIRSTSUPERCOLOR)
					skincolors[num].accessible = (boolean)(atoi(word2) || word2[0] == 'T' || word2[0] == 'Y');
			}
			else
				deh_warning("Skincolor %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

#ifdef HWRENDER
static void readlight(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *tmp;
	INT32 value;
	float fvalue;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			fvalue = searchfvalue(s);
			value = searchvalue(s);

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			if (fastcmp(word, "TYPE"))
			{
				lspr[num].type = (UINT16)value;
			}
			else if (fastcmp(word, "OFFSETX"))
			{
				lspr[num].light_xoffset = fvalue;
			}
			else if (fastcmp(word, "OFFSETY"))
			{
				lspr[num].light_yoffset = fvalue;
			}
			else if (fastcmp(word, "CORONACOLOR"))
			{
				lspr[num].corona_color = value;
			}
			else if (fastcmp(word, "CORONARADIUS"))
			{
				lspr[num].corona_radius = fvalue;
			}
			else if (fastcmp(word, "DYNAMICCOLOR"))
			{
				lspr[num].dynamic_color = value;
			}
			else if (fastcmp(word, "DYNAMICRADIUS"))
			{
				lspr[num].dynamic_radius = fvalue;

				/// \note Update the sqrradius! unnecessary?
				lspr[num].dynamic_sqrradius = fvalue * fvalue;
			}
			else
				deh_warning("Light %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}
#endif // HWRENDER

static void readspriteframe(MYFILE *f, spriteinfo_t *sprinfo, UINT8 frame)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word, *word2;
	char *tmp;
	INT32 value;
	char *lastline;

	do
	{
		lastline = f->curpos;
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Set / reset word
			word = s;
			while ((*word == '\t') || (*word == ' '))
				word++;

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
			{
				*(tmp-1) = '\0';
				// Now get the part after
				word2 = tmp += 2;
			}
			else
			{
				// Get the part before the " "
				tmp = strchr(s, ' ');
				if (tmp)
				{
					*tmp = '\0';
					// Now get the part after
					tmp++;
					word2 = tmp;
				}
				else
					break;
			}
			strupr(word);
			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "XPIVOT"))
				sprinfo->pivot[frame].x = value;
			else if (fastcmp(word, "YPIVOT"))
				sprinfo->pivot[frame].y = value;
			else
			{
				f->curpos = lastline;
				break;
			}
		}
	} while (!myfeof(f)); // finish when the line is empty
	Z_Free(s);
}

static void readspriteinfo(MYFILE *f, INT32 num, boolean sprite2)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word, *word2;
	char *tmp;
#ifdef HWRENDER
	INT32 value;
#endif
	char *lastline;
	INT32 skinnumbers[MAXSKINS];
	INT32 foundskins = 0;

	// allocate a spriteinfo
	spriteinfo_t *info = Z_Calloc(sizeof(spriteinfo_t), PU_STATIC, NULL);
	info->available = true;

#ifdef ROTSPRITE
	if ((sprites != NULL) && (!sprite2))
		R_FreeSingleRotSprite(&sprites[num]);
#endif

	do
	{
		lastline = f->curpos;
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Set / reset word
			word = s;
			while ((*word == '\t') || (*word == ' '))
				word++;

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
			{
				*(tmp-1) = '\0';
				// Now get the part after
				word2 = tmp += 2;
			}
			else
			{
				// Get the part before the " "
				tmp = strchr(s, ' ');
				if (tmp)
				{
					*tmp = '\0';
					// Now get the part after
					tmp++;
					word2 = tmp;
				}
				else
					break;
			}
			strupr(word);
#ifdef HWRENDER
			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "LIGHTTYPE"))
			{
				if (sprite2)
					deh_warning("Sprite2 %s: invalid word '%s'", spr2names[num], word);
				else
				{
					INT32 oldvar;
					for (oldvar = 0; t_lspr[num] != &lspr[oldvar]; oldvar++)
						;
					t_lspr[num] = &lspr[value];
				}
			}
			else
#endif
			if (fastcmp(word, "SKIN"))
			{
				INT32 skinnum = -1;
				if (!sprite2)
				{
					deh_warning("Sprite %s: %s keyword found outside of SPRITE2INFO block, ignoring", spr2names[num], word);
					continue;
				}

				// make lowercase
				strlwr(word2);
				skinnum = R_SkinAvailable(word2);
				if (skinnum == -1)
				{
					deh_warning("Sprite2 %s: unknown skin %s", spr2names[num], word2);
					break;
				}

				skinnumbers[foundskins] = skinnum;
				foundskins++;
			}
			else if (fastcmp(word, "DEFAULT"))
			{
				if (!sprite2)
				{
					deh_warning("Sprite %s: %s keyword found outside of SPRITE2INFO block, ignoring", spr2names[num], word);
					continue;
				}
				if (num < (INT32)free_spr2 && num >= (INT32)SPR2_FIRSTFREESLOT)
					spr2defaults[num] = get_number(word2);
				else
				{
					deh_warning("Sprite2 %s: out of range (%d - %d), ignoring", spr2names[num], SPR2_FIRSTFREESLOT, free_spr2-1);
					continue;
				}
			}
			else if (fastcmp(word, "FRAME"))
			{
				UINT8 frame = R_Char2Frame(word2[0]);
				// frame number too high
				if (frame >= 64)
				{
					if (sprite2)
						deh_warning("Sprite2 %s: invalid frame %s", spr2names[num], word2);
					else
						deh_warning("Sprite %s: invalid frame %s", sprnames[num], word2);
					break;
				}

				// read sprite frame and store it in the spriteinfo_t struct
				readspriteframe(f, info, frame);
				if (sprite2)
				{
					INT32 i;
					if (!foundskins)
					{
						deh_warning("Sprite2 %s: no skins specified", spr2names[num]);
						break;
					}
					for (i = 0; i < foundskins; i++)
					{
						size_t skinnum = skinnumbers[i];
						skin_t *skin = &skins[skinnum];
						spriteinfo_t *sprinfo = skin->sprinfo;
#ifdef ROTSPRITE
						R_FreeSkinRotSprite(skinnum);
#endif
						M_Memcpy(&sprinfo[num], info, sizeof(spriteinfo_t));
					}
				}
				else
					M_Memcpy(&spriteinfo[num], info, sizeof(spriteinfo_t));
			}
			else
			{
				//deh_warning("Sprite %s: unknown word '%s'", sprnames[num], word);
				f->curpos = lastline;
				break;
			}
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
	Z_Free(info);
}

static void readsprite2(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word, *word2;
	char *tmp;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;
			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';

			if (fastcmp(word, "DEFAULT"))
				spr2defaults[num] = get_number(word2);
			else
				deh_warning("Sprite2 %s: unknown word '%s'", spr2names[num], word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

// copypasted from readPlayer :]
static const char *const GAMETYPERULE_LIST[];
static void readgametype(MYFILE *f, char *gtname)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2, *word2lwr = NULL;
	char *tmp;
	INT32 i, j;

	INT16 newgtidx = 0;
	UINT32 newgtrules = 0;
	UINT32 newgttol = 0;
	INT32 newgtpointlimit = 0;
	INT32 newgttimelimit = 0;
	INT16 newgtrankingstype = -1;
	int newgtinttype = 0;
	char gtconst[MAXLINELEN];

	// Empty strings.
	gtconst[0] = '\0';

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " = ");
			if (word2)
			{
				if (!word2lwr)
					word2lwr = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
				strcpy(word2lwr, word2);
				strupr(word2);
			}
			else
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';
			i = atoi(word2);

			// Game type rules
			if (fastcmp(word, "RULES"))
			{
				// GTR_
				newgtrules = (UINT32)get_number(word2);
			}
			// Identifier
			else if (fastcmp(word, "IDENTIFIER"))
			{
				// GT_
				strncpy(gtconst, word2, MAXLINELEN);
			}
			// Point and time limits
			else if (fastcmp(word, "DEFAULTPOINTLIMIT"))
				newgtpointlimit = (INT32)i;
			else if (fastcmp(word, "DEFAULTTIMELIMIT"))
				newgttimelimit = (INT32)i;
			// Rankings type
			else if (fastcmp(word, "RANKINGTYPE"))
			{
				// Case insensitive
				newgtrankingstype = (int)get_number(word2);
			}
			// Intermission type
			else if (fastcmp(word, "INTERMISSIONTYPE"))
			{
				// Case sensitive
				newgtinttype = (int)get_number(word2lwr);
			}
			// Type of level
			else if (fastcmp(word, "TYPEOFLEVEL"))
			{
				if (i) // it's just a number
					newgttol = (UINT32)i;
				else
				{
					UINT32 tol = 0;
					tmp = strtok(word2,",");
					do {
						for (i = 0; TYPEOFLEVEL[i].name; i++)
							if (fasticmp(tmp, TYPEOFLEVEL[i].name))
								break;
						if (!TYPEOFLEVEL[i].name)
							deh_warning("readgametype %s: unknown typeoflevel flag %s\n", gtname, tmp);
						tol |= TYPEOFLEVEL[i].flag;
					} while((tmp = strtok(NULL,",")) != NULL);
					newgttol = tol;
				}
			}
			// The SOC probably provided gametype rules as words,
			// instead of using the RULES keyword.
			// Like for example "NOSPECTATORSPAWN = TRUE".
			// This is completely valid, and looks better anyway.
			else
			{
				UINT32 wordgt = 0;
				for (j = 0; GAMETYPERULE_LIST[j]; j++)
					if (fastcmp(word, GAMETYPERULE_LIST[j])) {
						wordgt |= (1<<j);
						if (i || word2[0] == 'T' || word2[0] == 'Y')
							newgtrules |= wordgt;
						break;
					}
				if (!wordgt)
					deh_warning("readgametype %s: unknown word '%s'", gtname, word);
			}
		}
	} while (!myfeof(f)); // finish when the line is empty

	// Free strings.
	Z_Free(s);
	if (word2lwr)
		Z_Free(word2lwr);

	// Ran out of gametype slots
	if (gametypecount == NUMGAMETYPEFREESLOTS)
	{
		CONS_Alert(CONS_WARNING, "Ran out of free gametype slots!\n");
		return;
	}

	// Add the new gametype
	newgtidx = G_AddGametype(newgtrules);
	G_AddGametypeTOL(newgtidx, newgttol);

	// Not covered by G_AddGametype alone.
	if (newgtrankingstype == -1)
		newgtrankingstype = newgtidx;
	gametyperankings[newgtidx] = newgtrankingstype;
	intermissiontypes[newgtidx] = newgtinttype;
	pointlimits[newgtidx] = newgtpointlimit;
	timelimits[newgtidx] = newgttimelimit;

	// Write the new gametype name.
	Gametype_Names[newgtidx] = Z_StrDup((const char *)gtname);

	// Write the constant name.
	if (gtconst[0] == '\0')
		strncpy(gtconst, gtname, MAXLINELEN);
	G_AddGametypeConstant(newgtidx, (const char *)gtconst);

	// Update gametype_cons_t accordingly.
	G_UpdateGametypeSelections();

	CONS_Printf("Added gametype %s\n", Gametype_Names[newgtidx]);
}

static const struct {
	const char *name;
	const mobjtype_t type;
} FLICKYTYPES[] = {
	{"BLUEBIRD", MT_FLICKY_01}, // Flicky (Flicky)
	{"RABBIT",   MT_FLICKY_02}, // Pocky (1)
	{"CHICKEN",  MT_FLICKY_03}, // Cucky (1)
	{"SEAL",     MT_FLICKY_04}, // Rocky (1)
	{"PIG",      MT_FLICKY_05}, // Picky (1)
	{"CHIPMUNK", MT_FLICKY_06}, // Ricky (1)
	{"PENGUIN",  MT_FLICKY_07}, // Pecky (1)
	{"FISH",     MT_FLICKY_08}, // Nicky (CD)
	{"RAM",      MT_FLICKY_09}, // Flocky (CD)
	{"PUFFIN",   MT_FLICKY_10}, // Wicky (CD)
	{"COW",      MT_FLICKY_11}, // Macky (SRB2)
	{"RAT",      MT_FLICKY_12}, // Micky (2)
	{"BEAR",     MT_FLICKY_13}, // Becky (2)
	{"DOVE",     MT_FLICKY_14}, // Docky (CD)
	{"CAT",      MT_FLICKY_15}, // Nyannyan (Flicky)
	{"CANARY",   MT_FLICKY_16}, // Lucky (CD)
	{"a", 0}, // End of normal flickies - a lower case character so will never fastcmp valid with uppercase tmp
	//{"FLICKER",          MT_FLICKER}, // Flacky (SRB2)
	{"SPIDER",   MT_SECRETFLICKY_01}, // Sticky (SRB2)
	{"BAT",      MT_SECRETFLICKY_02}, // Backy (SRB2)
	{"SEED",                MT_SEED}, // Seed (CD)
	{NULL, 0}
};

#define MAXFLICKIES 64

static void readlevelheader(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	//char *word3; // Non-uppercase version of word2
	char *tmp;
	INT32 i;

	// Reset all previous map header information
	P_AllocMapHeader((INT16)(num-1));

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Set / reset word, because some things (Lua.) move it
			word = s;

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			i = atoi(word2); // used for numerical settings


			if (fastcmp(word, "LEVELNAME"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->lvlttl, word2,
					sizeof(mapheaderinfo[num-1]->lvlttl), va("Level header %d: levelname", num));
				strlcpy(mapheaderinfo[num-1]->selectheading, word2, sizeof(mapheaderinfo[num-1]->selectheading)); // not deh_ so only complains once
				continue;
			}
			// CHEAP HACK: move this over here for lowercase subtitles
			if (fastcmp(word, "SUBTITLE"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->subttl, word2,
					sizeof(mapheaderinfo[num-1]->subttl), va("Level header %d: subtitle", num));
				continue;
			}

			// Lua custom options also go above, contents may be case sensitive.
			if (fastncmp(word, "LUA.", 4))
			{
				UINT8 j;
				customoption_t *modoption;

				// Note: we actualy strlwr word here, so things are made a little easier for Lua
				strlwr(word);
				word += 4; // move past "lua."

				// ... and do a simple name sanity check; the name must start with a letter
				if (*word < 'a' || *word > 'z')
				{
					deh_warning("Level header %d: invalid custom option name \"%s\"", num, word);
					continue;
				}

				// Sanity limit of 128 params
				if (mapheaderinfo[num-1]->numCustomOptions == 128)
				{
					deh_warning("Level header %d: too many custom parameters", num);
					continue;
				}
				j = mapheaderinfo[num-1]->numCustomOptions++;

				mapheaderinfo[num-1]->customopts =
					Z_Realloc(mapheaderinfo[num-1]->customopts,
						sizeof(customoption_t) * mapheaderinfo[num-1]->numCustomOptions, PU_STATIC, NULL);

				// Newly allocated
				modoption = &mapheaderinfo[num-1]->customopts[j];

				strncpy(modoption->option, word,  31);
				modoption->option[31] = '\0';
				strncpy(modoption->value,  word2, 255);
				modoption->value[255] = '\0';
				continue;
			}

			// Now go to uppercase
			strupr(word2);

			// List of flickies that are be freed in this map
			if (fastcmp(word, "FLICKYLIST") || fastcmp(word, "ANIMALLIST"))
			{
				if (fastcmp(word2, "NONE"))
					P_DeleteFlickies(num-1);
				else if (fastcmp(word2, "DEMO"))
					P_SetDemoFlickies(num-1);
				else if (fastcmp(word2, "ALL"))
				{
					mobjtype_t tmpflickies[MAXFLICKIES];

					for (mapheaderinfo[num-1]->numFlickies = 0;
					((mapheaderinfo[num-1]->numFlickies < MAXFLICKIES) && FLICKYTYPES[mapheaderinfo[num-1]->numFlickies].type);
					mapheaderinfo[num-1]->numFlickies++)
						tmpflickies[mapheaderinfo[num-1]->numFlickies] = FLICKYTYPES[mapheaderinfo[num-1]->numFlickies].type;

					if (mapheaderinfo[num-1]->numFlickies) // just in case...
					{
						size_t newsize = sizeof(mobjtype_t) * mapheaderinfo[num-1]->numFlickies;
						mapheaderinfo[num-1]->flickies = Z_Realloc(mapheaderinfo[num-1]->flickies, newsize, PU_STATIC, NULL);
						M_Memcpy(mapheaderinfo[num-1]->flickies, tmpflickies, newsize);
					}
				}
				else
				{
					mobjtype_t tmpflickies[MAXFLICKIES];
					mapheaderinfo[num-1]->numFlickies = 0;
					tmp = strtok(word2,",");
					// get up to the first MAXFLICKIES flickies
					do {
						if (mapheaderinfo[num-1]->numFlickies == MAXFLICKIES) // never going to get above that number
						{
							deh_warning("Level header %d: too many flickies\n", num);
							break;
						}

						if (fastncmp(tmp, "MT_", 3)) // support for specified mobjtypes...
						{
							i = get_mobjtype(tmp);
							if (!i)
							{
								//deh_warning("Level header %d: unknown flicky mobj type %s\n", num, tmp); -- no need for this line as get_mobjtype complains too
								continue;
							}
							tmpflickies[mapheaderinfo[num-1]->numFlickies] = i;
						}
						else // ...or a quick, limited selection of default flickies!
						{
							for (i = 0; FLICKYTYPES[i].name; i++)
								if (fastcmp(tmp, FLICKYTYPES[i].name))
									break;

							if (!FLICKYTYPES[i].name)
							{
								deh_warning("Level header %d: unknown flicky selection %s\n", num, tmp);
								continue;
							}
							tmpflickies[mapheaderinfo[num-1]->numFlickies] = FLICKYTYPES[i].type;
						}
						mapheaderinfo[num-1]->numFlickies++;
					} while ((tmp = strtok(NULL,",")) != NULL);

					if (mapheaderinfo[num-1]->numFlickies)
					{
						size_t newsize = sizeof(mobjtype_t) * mapheaderinfo[num-1]->numFlickies;
						mapheaderinfo[num-1]->flickies = Z_Realloc(mapheaderinfo[num-1]->flickies, newsize, PU_STATIC, NULL);
						// now we add them to the list!
						M_Memcpy(mapheaderinfo[num-1]->flickies, tmpflickies, newsize);
					}
					else
						deh_warning("Level header %d: no valid flicky types found\n", num);
				}
			}

			// Strings that can be truncated
			else if (fastcmp(word, "ZONETITLE"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->zonttl, word2,
					sizeof(mapheaderinfo[num-1]->zonttl), va("Level header %d: zonetitle", num));
			}
			else if (fastcmp(word, "SCRIPTNAME"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->scriptname, word2,
					sizeof(mapheaderinfo[num-1]->scriptname), va("Level header %d: scriptname", num));
			}
			else if (fastcmp(word, "RUNSOC"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->runsoc, word2,
					sizeof(mapheaderinfo[num-1]->runsoc), va("Level header %d: runsoc", num));
			}
			else if (fastcmp(word, "ACT"))
			{
				if (i >= 0 && i <= 99) // 0 for no act number
					mapheaderinfo[num-1]->actnum = (UINT8)i;
				else
					deh_warning("Level header %d: invalid act number %d", num, i);
			}
			else if (fastcmp(word, "NEXTLEVEL"))
			{
				if      (fastcmp(word2, "TITLE"))      i = 1100;
				else if (fastcmp(word2, "EVALUATION")) i = 1101;
				else if (fastcmp(word2, "CREDITS"))    i = 1102;
				else if (fastcmp(word2, "ENDING"))     i = 1103;
				else
				// Support using the actual map name,
				// i.e., Nextlevel = AB, Nextlevel = FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z' && word2[2] == '\0')
					i = M_MapNumber(word2[0], word2[1]);

				mapheaderinfo[num-1]->nextlevel = (INT16)i;
			}
			else if (fastcmp(word, "MARATHONNEXT"))
			{
				if      (fastcmp(word2, "TITLE"))      i = 1100;
				else if (fastcmp(word2, "EVALUATION")) i = 1101;
				else if (fastcmp(word2, "CREDITS"))    i = 1102;
				else if (fastcmp(word2, "ENDING"))     i = 1103;
				else
				// Support using the actual map name,
				// i.e., MarathonNext = AB, MarathonNext = FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z' && word2[2] == '\0')
					i = M_MapNumber(word2[0], word2[1]);

				mapheaderinfo[num-1]->marathonnext = (INT16)i;
			}
			else if (fastcmp(word, "TYPEOFLEVEL"))
			{
				if (i) // it's just a number
					mapheaderinfo[num-1]->typeoflevel = (UINT32)i;
				else
				{
					UINT32 tol = 0;
					tmp = strtok(word2,",");
					do {
						for (i = 0; TYPEOFLEVEL[i].name; i++)
							if (fastcmp(tmp, TYPEOFLEVEL[i].name))
								break;
						if (!TYPEOFLEVEL[i].name)
							deh_warning("Level header %d: unknown typeoflevel flag %s\n", num, tmp);
						tol |= TYPEOFLEVEL[i].flag;
					} while((tmp = strtok(NULL,",")) != NULL);
					mapheaderinfo[num-1]->typeoflevel = tol;
				}
			}
			else if (fastcmp(word, "KEYWORDS"))
			{
				deh_strlcpy(mapheaderinfo[num-1]->keywords, word2,
						sizeof(mapheaderinfo[num-1]->keywords), va("Level header %d: keywords", num));
			}
			else if (fastcmp(word, "MUSIC"))
			{
				if (fastcmp(word2, "NONE"))
					mapheaderinfo[num-1]->musname[0] = 0; // becomes empty string
				else
				{
					deh_strlcpy(mapheaderinfo[num-1]->musname, word2,
						sizeof(mapheaderinfo[num-1]->musname), va("Level header %d: music", num));
				}
			}
#ifdef MUSICSLOT_COMPATIBILITY
			else if (fastcmp(word, "MUSICSLOT"))
			{
				i = get_mus(word2, true);
				if (i && i <= 1035)
					snprintf(mapheaderinfo[num-1]->musname, 7, "%sM", G_BuildMapName(i));
				else if (i && i <= 1050)
					strncpy(mapheaderinfo[num-1]->musname, compat_special_music_slots[i - 1036], 7);
				else
					mapheaderinfo[num-1]->musname[0] = 0; // becomes empty string
				mapheaderinfo[num-1]->musname[6] = 0;
			}
#endif
			else if (fastcmp(word, "MUSICTRACK"))
				mapheaderinfo[num-1]->mustrack = ((UINT16)i - 1);
			else if (fastcmp(word, "MUSICPOS"))
				mapheaderinfo[num-1]->muspos = (UINT32)get_number(word2);
			else if (fastcmp(word, "FORCECHARACTER"))
			{
				strlcpy(mapheaderinfo[num-1]->forcecharacter, word2, SKINNAMESIZE+1);
				strlwr(mapheaderinfo[num-1]->forcecharacter); // skin names are lowercase
			}
			else if (fastcmp(word, "WEATHER"))
				mapheaderinfo[num-1]->weather = (UINT8)get_number(word2);
			else if (fastcmp(word, "SKYTEXTURE"))
				deh_strlcpy(mapheaderinfo[num-1]->skytexture, word2,
					sizeof(mapheaderinfo[num-1]->skytexture), va("Level header %d: sky texture", num));
			else if (fastcmp(word, "PRECUTSCENENUM"))
				mapheaderinfo[num-1]->precutscenenum = (UINT8)i;
			else if (fastcmp(word, "CUTSCENENUM"))
				mapheaderinfo[num-1]->cutscenenum = (UINT8)i;
			else if (fastcmp(word, "PALETTE"))
				mapheaderinfo[num-1]->palette = (UINT16)i;
			else if (fastcmp(word, "ENCOREPAL"))
				mapheaderinfo[num-1]->encorepal = (UINT16)i;
			else if (fastcmp(word, "NUMLAPS"))
				mapheaderinfo[num-1]->numlaps = (UINT8)i;
			else if (fastcmp(word, "UNLOCKABLE"))
			{
				if (i >= 0 && i <= MAXUNLOCKABLES) // 0 for no unlock required, anything else requires something
					mapheaderinfo[num-1]->unlockrequired = (SINT8)i - 1;
				else
					deh_warning("Level header %d: invalid unlockable number %d", num, i);
			}
			else if (fastcmp(word, "SKYBOXSCALE"))
				mapheaderinfo[num-1]->skybox_scalex = mapheaderinfo[num-1]->skybox_scaley = mapheaderinfo[num-1]->skybox_scalez = (INT16)i;
			else if (fastcmp(word, "SKYBOXSCALEX"))
				mapheaderinfo[num-1]->skybox_scalex = (INT16)i;
			else if (fastcmp(word, "SKYBOXSCALEY"))
				mapheaderinfo[num-1]->skybox_scaley = (INT16)i;
			else if (fastcmp(word, "SKYBOXSCALEZ"))
				mapheaderinfo[num-1]->skybox_scalez = (INT16)i;
			else if (fastcmp(word, "LEVELFLAGS"))
				mapheaderinfo[num-1]->levelflags = get_number(word2);
			else if (fastcmp(word, "MENUFLAGS"))
				mapheaderinfo[num-1]->menuflags = get_number(word2);
			// SRB2Kart
			else if (fastcmp(word, "MOBJSCALE"))
				mapheaderinfo[num-1]->mobj_scale = get_number(word2);
			else if (fastcmp(word, "DEFAULTWAYPOINTRADIUS"))
				mapheaderinfo[num-1]->default_waypoint_radius = get_number(word2);
			// Individual triggers for level flags, for ease of use (and 2.0 compatibility)
			else if (fastcmp(word, "SCRIPTISFILE"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_SCRIPTISFILE;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_SCRIPTISFILE;
			}
			else if (fastcmp(word, "NOZONE"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_NOZONE;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_NOZONE;
			}
			else if (fastcmp(word, "SECTIONRACE"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->levelflags |= LF_SECTIONRACE;
				else
					mapheaderinfo[num-1]->levelflags &= ~LF_SECTIONRACE;
			}

			// Individual triggers for menu flags
			else if (fastcmp(word, "HIDDEN"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags |= LF2_HIDEINMENU;
				else
					mapheaderinfo[num-1]->menuflags &= ~LF2_HIDEINMENU;
			}
			else if (fastcmp(word, "HIDEINSTATS"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags |= LF2_HIDEINSTATS;
				else
					mapheaderinfo[num-1]->menuflags &= ~LF2_HIDEINSTATS;
			}
			else if (fastcmp(word, "TIMEATTACK") || fastcmp(word, "RECORDATTACK"))
			{ // RECORDATTACK is an accepted alias
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags |= LF2_TIMEATTACK;
				else
					mapheaderinfo[num-1]->menuflags &= ~LF2_TIMEATTACK;
			}
			else if (fastcmp(word, "VISITNEEDED"))
			{
				if (i || word2[0] == 'T' || word2[0] == 'Y')
					mapheaderinfo[num-1]->menuflags |= LF2_VISITNEEDED;
				else
					mapheaderinfo[num-1]->menuflags &= ~LF2_VISITNEEDED;
			}
			else if (fastcmp(word, "GRAVITY"))
				mapheaderinfo[num-1]->gravity = FLOAT_TO_FIXED(atof(word2));
			else
				deh_warning("Level header %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

#undef MAXFLICKIES

static void readcupheader(MYFILE *f, cupheader_t *cup)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *tmp;
	INT32 i;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Set / reset word, because some things (Lua.) move it
			word = s;

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			i = atoi(word2); // used for numerical settings
			strupr(word2);

			if (fastcmp(word, "ICON"))
			{
				deh_strlcpy(cup->icon, word2,
					sizeof(cup->icon), va("%s Cup: icon", cup->name));
			}
			else if (fastcmp(word, "LEVELLIST"))
			{
				cup->numlevels = 0;

				tmp = strtok(word2,",");
				do {
					INT32 map = atoi(tmp);

					if (tmp[0] >= 'A' && tmp[0] <= 'Z' && tmp[2] == '\0')
						map = M_MapNumber(tmp[0], tmp[1]);

					if (!map)
						break;

					if (cup->numlevels >= MAXLEVELLIST)
					{
						deh_warning("%s Cup: reached max levellist (%d)\n", cup->name, MAXLEVELLIST);
						break;
					}

					cup->levellist[cup->numlevels] = map - 1;
					cup->numlevels++;
				} while((tmp = strtok(NULL,",")) != NULL);
			}
			else if (fastcmp(word, "BONUSGAME"))
			{
				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z' && word2[2] == '\0')
					i = M_MapNumber(word2[0], word2[1]);
				cup->bonusgame = (INT16)i - 1;
			}
			else if (fastcmp(word, "SPECIALSTAGE"))
			{
				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z' && word2[2] == '\0')
					i = M_MapNumber(word2[0], word2[1]);
				cup->specialstage = (INT16)i - 1;
			}
			else if (fastcmp(word, "EMERALDNUM"))
			{
				if (i >= 0 && i <= 14)
					cup->emeraldnum = (UINT8)i;
				else
					deh_warning("%s Cup: invalid emerald number %d", cup->name, i);
			}
			else if (fastcmp(word, "UNLOCKABLE"))
			{
				if (i >= 0 && i <= MAXUNLOCKABLES) // 0 for no unlock required, anything else requires something
					cup->unlockrequired = (SINT8)i - 1;
				else
					deh_warning("%s Cup: invalid unlockable number %d", cup->name, i);
			}
			else
				deh_warning("%s Cup: unknown word '%s'", cup->name, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readcutscenescene(MYFILE *f, INT32 num, INT32 scenenum)
{
	char *s = Z_Calloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	INT32 i;
	UINT16 usi;
	UINT8 picid;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			if (fastcmp(word, "SCENETEXT"))
			{
				char *scenetext = NULL;
				char *buffer;
				const int bufferlen = 4096;

				for (i = 0; i < MAXLINELEN; i++)
				{
					if (s[i] == '=')
					{
						scenetext = &s[i+2];
						break;
					}
				}

				if (!scenetext)
				{
					Z_Free(cutscenes[num]->scene[scenenum].text);
					cutscenes[num]->scene[scenenum].text = NULL;
					continue;
				}

				for (i = 0; i < MAXLINELEN; i++)
				{
					if (s[i] == '\0')
					{
						s[i] = '\n';
						s[i+1] = '\0';
						break;
					}
				}

				buffer = Z_Malloc(4096, PU_STATIC, NULL);
				strcpy(buffer, scenetext);

				strcat(buffer,
					myhashfgets(scenetext, bufferlen
					- strlen(buffer) - 1, f));

				// A cutscene overwriting another one...
				Z_Free(cutscenes[num]->scene[scenenum].text);

				cutscenes[num]->scene[scenenum].text = Z_StrDup(buffer);

				Z_Free(buffer);

				continue;
			}

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';
			i = atoi(word2);
			usi = (UINT16)i;


			if (fastcmp(word, "NUMBEROFPICS"))
			{
				cutscenes[num]->scene[scenenum].numpics = (UINT8)i;
			}
			else if (fastncmp(word, "PIC", 3))
			{
				picid = (UINT8)atoi(word + 3);
				if (picid > 8 || picid == 0)
				{
					deh_warning("CutSceneScene %d: unknown word '%s'", num, word);
					continue;
				}
				--picid;

				if (fastcmp(word+4, "NAME"))
				{
					strncpy(cutscenes[num]->scene[scenenum].picname[picid], word2, 8);
				}
				else if (fastcmp(word+4, "HIRES"))
				{
					cutscenes[num]->scene[scenenum].pichires[picid] = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
				}
				else if (fastcmp(word+4, "DURATION"))
				{
					cutscenes[num]->scene[scenenum].picduration[picid] = usi;
				}
				else if (fastcmp(word+4, "XCOORD"))
				{
					cutscenes[num]->scene[scenenum].xcoord[picid] = usi;
				}
				else if (fastcmp(word+4, "YCOORD"))
				{
					cutscenes[num]->scene[scenenum].ycoord[picid] = usi;
				}
				else
					deh_warning("CutSceneScene %d: unknown word '%s'", num, word);
			}
			else if (fastcmp(word, "MUSIC"))
			{
				strncpy(cutscenes[num]->scene[scenenum].musswitch, word2, 7);
				cutscenes[num]->scene[scenenum].musswitch[6] = 0;
			}
#ifdef MUSICSLOT_COMPATIBILITY
			else if (fastcmp(word, "MUSICSLOT"))
			{
				i = get_mus(word2, true);
				if (i && i <= 1035)
					snprintf(cutscenes[num]->scene[scenenum].musswitch, 7, "%sM", G_BuildMapName(i));
				else if (i && i <= 1050)
					strncpy(cutscenes[num]->scene[scenenum].musswitch, compat_special_music_slots[i - 1036], 7);
				else
					cutscenes[num]->scene[scenenum].musswitch[0] = 0; // becomes empty string
				cutscenes[num]->scene[scenenum].musswitch[6] = 0;
			}
#endif
			else if (fastcmp(word, "MUSICTRACK"))
			{
				cutscenes[num]->scene[scenenum].musswitchflags = ((UINT16)i) & MUSIC_TRACKMASK;
			}
			else if (fastcmp(word, "MUSICPOS"))
			{
				cutscenes[num]->scene[scenenum].musswitchposition = (UINT32)get_number(word2);
			}
			else if (fastcmp(word, "MUSICLOOP"))
			{
				cutscenes[num]->scene[scenenum].musicloop = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
			}
			else if (fastcmp(word, "TEXTXPOS"))
			{
				cutscenes[num]->scene[scenenum].textxpos = usi;
			}
			else if (fastcmp(word, "TEXTYPOS"))
			{
				cutscenes[num]->scene[scenenum].textypos = usi;
			}
			else if (fastcmp(word, "FADEINID"))
			{
				cutscenes[num]->scene[scenenum].fadeinid = (UINT8)i;
			}
			else if (fastcmp(word, "FADEOUTID"))
			{
				cutscenes[num]->scene[scenenum].fadeoutid = (UINT8)i;
			}
			else if (fastcmp(word, "FADECOLOR"))
			{
				cutscenes[num]->scene[scenenum].fadecolor = (UINT8)i;
			}
			else
				deh_warning("CutSceneScene %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readcutscene(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *tmp;
	INT32 value;

	// Allocate memory for this cutscene if we don't yet have any
	if (!cutscenes[num])
		cutscenes[num] = Z_Calloc(sizeof (cutscene_t), PU_STATIC, NULL);

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " ");
			if (word2)
				value = atoi(word2);
			else
			{
				deh_warning("No value for token %s", word);
				continue;
			}

			if (fastcmp(word, "NUMSCENES"))
			{
				cutscenes[num]->numscenes = value;
			}
			else if (fastcmp(word, "SCENE"))
			{
				if (1 <= value && value <= 128)
				{
					readcutscenescene(f, num, value - 1);
				}
				else
					deh_warning("Scene number %d out of range (1 - 128)", value);

			}
			else
				deh_warning("Cutscene %d: unknown word '%s', Scene <num> expected.", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readtextpromptpage(MYFILE *f, INT32 num, INT32 pagenum)
{
	char *s = Z_Calloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	INT32 i;
	UINT16 usi;
	UINT8 picid;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			if (fastcmp(word, "PAGETEXT"))
			{
				char *pagetext = NULL;
				char *buffer;
				const int bufferlen = 4096;

				for (i = 0; i < MAXLINELEN; i++)
				{
					if (s[i] == '=')
					{
						pagetext = &s[i+2];
						break;
					}
				}

				if (!pagetext)
				{
					Z_Free(textprompts[num]->page[pagenum].text);
					textprompts[num]->page[pagenum].text = NULL;
					continue;
				}

				for (i = 0; i < MAXLINELEN; i++)
				{
					if (s[i] == '\0')
					{
						s[i] = '\n';
						s[i+1] = '\0';
						break;
					}
				}

				buffer = Z_Malloc(4096, PU_STATIC, NULL);
				strcpy(buffer, pagetext);

				// \todo trim trailing whitespace before the #
				// and also support # at the end of a PAGETEXT with no line break

				strcat(buffer,
					myhashfgets(pagetext, bufferlen
					- strlen(buffer) - 1, f));

				// A text prompt overwriting another one...
				Z_Free(textprompts[num]->page[pagenum].text);

				textprompts[num]->page[pagenum].text = Z_StrDup(buffer);

				Z_Free(buffer);

				continue;
			}

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;

			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';
			i = atoi(word2);
			usi = (UINT16)i;

			// copypasta from readcutscenescene
			if (fastcmp(word, "NUMBEROFPICS"))
			{
				textprompts[num]->page[pagenum].numpics = (UINT8)i;
			}
			else if (fastcmp(word, "PICMODE"))
			{
				UINT8 picmode = 0; // PROMPT_PIC_PERSIST
				if (usi == 1 || word2[0] == 'L') picmode = PROMPT_PIC_LOOP;
				else if (usi == 2 || word2[0] == 'D' || word2[0] == 'H') picmode = PROMPT_PIC_DESTROY;
				textprompts[num]->page[pagenum].picmode = picmode;
			}
			else if (fastcmp(word, "PICTOLOOP"))
				textprompts[num]->page[pagenum].pictoloop = (UINT8)i;
			else if (fastcmp(word, "PICTOSTART"))
				textprompts[num]->page[pagenum].pictostart = (UINT8)i;
			else if (fastcmp(word, "PICSMETAPAGE"))
			{
				if (usi && usi <= textprompts[num]->numpages)
				{
					UINT8 metapagenum = usi - 1;

					textprompts[num]->page[pagenum].numpics = textprompts[num]->page[metapagenum].numpics;
					textprompts[num]->page[pagenum].picmode = textprompts[num]->page[metapagenum].picmode;
					textprompts[num]->page[pagenum].pictoloop = textprompts[num]->page[metapagenum].pictoloop;
					textprompts[num]->page[pagenum].pictostart = textprompts[num]->page[metapagenum].pictostart;

					for (picid = 0; picid < MAX_PROMPT_PICS; picid++)
					{
						strncpy(textprompts[num]->page[pagenum].picname[picid], textprompts[num]->page[metapagenum].picname[picid], 8);
						textprompts[num]->page[pagenum].pichires[picid] = textprompts[num]->page[metapagenum].pichires[picid];
						textprompts[num]->page[pagenum].picduration[picid] = textprompts[num]->page[metapagenum].picduration[picid];
						textprompts[num]->page[pagenum].xcoord[picid] = textprompts[num]->page[metapagenum].xcoord[picid];
						textprompts[num]->page[pagenum].ycoord[picid] = textprompts[num]->page[metapagenum].ycoord[picid];
					}
				}
			}
			else if (fastncmp(word, "PIC", 3))
			{
				picid = (UINT8)atoi(word + 3);
				if (picid > MAX_PROMPT_PICS || picid == 0)
				{
					deh_warning("textpromptscene %d: unknown word '%s'", num, word);
					continue;
				}
				--picid;

				if (fastcmp(word+4, "NAME"))
				{
					strncpy(textprompts[num]->page[pagenum].picname[picid], word2, 8);
				}
				else if (fastcmp(word+4, "HIRES"))
				{
					textprompts[num]->page[pagenum].pichires[picid] = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
				}
				else if (fastcmp(word+4, "DURATION"))
				{
					textprompts[num]->page[pagenum].picduration[picid] = usi;
				}
				else if (fastcmp(word+4, "XCOORD"))
				{
					textprompts[num]->page[pagenum].xcoord[picid] = usi;
				}
				else if (fastcmp(word+4, "YCOORD"))
				{
					textprompts[num]->page[pagenum].ycoord[picid] = usi;
				}
				else
					deh_warning("textpromptscene %d: unknown word '%s'", num, word);
			}
			else if (fastcmp(word, "MUSIC"))
			{
				strncpy(textprompts[num]->page[pagenum].musswitch, word2, 7);
				textprompts[num]->page[pagenum].musswitch[6] = 0;
			}
#ifdef MUSICSLOT_COMPATIBILITY
			else if (fastcmp(word, "MUSICSLOT"))
			{
				i = get_mus(word2, true);
				if (i && i <= 1035)
					snprintf(textprompts[num]->page[pagenum].musswitch, 7, "%sM", G_BuildMapName(i));
				else if (i && i <= 1050)
					strncpy(textprompts[num]->page[pagenum].musswitch, compat_special_music_slots[i - 1036], 7);
				else
					textprompts[num]->page[pagenum].musswitch[0] = 0; // becomes empty string
				textprompts[num]->page[pagenum].musswitch[6] = 0;
			}
#endif
			else if (fastcmp(word, "MUSICTRACK"))
			{
				textprompts[num]->page[pagenum].musswitchflags = ((UINT16)i) & MUSIC_TRACKMASK;
			}
			else if (fastcmp(word, "MUSICLOOP"))
			{
				textprompts[num]->page[pagenum].musicloop = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
			}
			// end copypasta from readcutscenescene
			else if (fastcmp(word, "NAME"))
			{
				if (*word2 != '\0')
				{
					INT32 j;

					// HACK: Add yellow control char now
					// so the drawing function doesn't call it repeatedly
					char name[34];
					name[0] = '\x82'; // color yellow
					name[1] = 0;
					strncat(name, word2, 33);
					name[33] = 0;

					// Replace _ with ' '
					for (j = 0; j < 32 && name[j]; j++)
					{
						if (name[j] == '_')
							name[j] = ' ';
					}

					strncpy(textprompts[num]->page[pagenum].name, name, 32);
				}
				else
					*textprompts[num]->page[pagenum].name = '\0';
			}
			else if (fastcmp(word, "ICON"))
				strncpy(textprompts[num]->page[pagenum].iconname, word2, 8);
			else if (fastcmp(word, "ICONALIGN"))
				textprompts[num]->page[pagenum].rightside = (i || word2[0] == 'R');
			else if (fastcmp(word, "ICONFLIP"))
				textprompts[num]->page[pagenum].iconflip = (i || word2[0] == 'T' || word2[0] == 'Y');
			else if (fastcmp(word, "LINES"))
				textprompts[num]->page[pagenum].lines = usi;
			else if (fastcmp(word, "BACKCOLOR"))
			{
				INT32 backcolor;
				if      (i == 0 || fastcmp(word2, "WHITE")) backcolor = 0;
				else if (i == 1 || fastcmp(word2, "GRAY") || fastcmp(word2, "GREY") ||
					fastcmp(word2, "BLACK")) backcolor = 1;
				else if (i == 2 || fastcmp(word2, "SEPIA")) backcolor = 2;
				else if (i == 3 || fastcmp(word2, "BROWN")) backcolor = 3;
				else if (i == 4 || fastcmp(word2, "PINK")) backcolor = 4;
				else if (i == 5 || fastcmp(word2, "RASPBERRY")) backcolor = 5;
				else if (i == 6 || fastcmp(word2, "RED")) backcolor = 6;
				else if (i == 7 || fastcmp(word2, "CREAMSICLE")) backcolor = 7;
				else if (i == 8 || fastcmp(word2, "ORANGE")) backcolor = 8;
				else if (i == 9 || fastcmp(word2, "GOLD")) backcolor = 9;
				else if (i == 10 || fastcmp(word2, "YELLOW")) backcolor = 10;
				else if (i == 11 || fastcmp(word2, "EMERALD")) backcolor = 11;
				else if (i == 12 || fastcmp(word2, "GREEN")) backcolor = 12;
				else if (i == 13 || fastcmp(word2, "CYAN") || fastcmp(word2, "AQUA")) backcolor = 13;
				else if (i == 14 || fastcmp(word2, "STEEL")) backcolor = 14;
				else if (i == 15 || fastcmp(word2, "PERIWINKLE")) backcolor = 15;
				else if (i == 16 || fastcmp(word2, "BLUE")) backcolor = 16;
				else if (i == 17 || fastcmp(word2, "PURPLE")) backcolor = 17;
				else if (i == 18 || fastcmp(word2, "LAVENDER")) backcolor = 18;
				else if (i >= 256 && i < 512) backcolor = i; // non-transparent palette index
				else if (i < 0) backcolor = INT32_MAX; // CONS_BACKCOLOR user-configured
				else backcolor = 1; // default gray
				textprompts[num]->page[pagenum].backcolor = backcolor;
			}
			else if (fastcmp(word, "ALIGN"))
			{
				UINT8 align = 0; // left
				if (usi == 1 || word2[0] == 'R') align = 1;
				else if (usi == 2 || word2[0] == 'C' || word2[0] == 'M') align = 2;
				textprompts[num]->page[pagenum].align = align;
			}
			else if (fastcmp(word, "VERTICALALIGN"))
			{
				UINT8 align = 0; // top
				if (usi == 1 || word2[0] == 'B') align = 1;
				else if (usi == 2 || word2[0] == 'C' || word2[0] == 'M') align = 2;
				textprompts[num]->page[pagenum].verticalalign = align;
			}
			else if (fastcmp(word, "TEXTSPEED"))
				textprompts[num]->page[pagenum].textspeed = get_number(word2);
			else if (fastcmp(word, "TEXTSFX"))
				textprompts[num]->page[pagenum].textsfx = get_number(word2);
			else if (fastcmp(word, "HIDEHUD"))
			{
				UINT8 hidehud = 0;
				if ((word2[0] == 'F' && (word2[1] == 'A' || !word2[1])) || word2[0] == 'N') hidehud = 0; // false
				else if (usi == 1 || word2[0] == 'T' || word2[0] == 'Y') hidehud = 1; // true (hide appropriate HUD elements)
				else if (usi == 2 || word2[0] == 'A' || (word2[0] == 'F' && word2[1] == 'O')) hidehud = 2; // force (hide all HUD elements)
				textprompts[num]->page[pagenum].hidehud = hidehud;
			}
			else if (fastcmp(word, "METAPAGE"))
			{
				if (usi && usi <= textprompts[num]->numpages)
				{
					UINT8 metapagenum = usi - 1;

					strncpy(textprompts[num]->page[pagenum].name, textprompts[num]->page[metapagenum].name, 32);
					strncpy(textprompts[num]->page[pagenum].iconname, textprompts[num]->page[metapagenum].iconname, 8);
					textprompts[num]->page[pagenum].rightside = textprompts[num]->page[metapagenum].rightside;
					textprompts[num]->page[pagenum].iconflip = textprompts[num]->page[metapagenum].iconflip;
					textprompts[num]->page[pagenum].lines = textprompts[num]->page[metapagenum].lines;
					textprompts[num]->page[pagenum].backcolor = textprompts[num]->page[metapagenum].backcolor;
					textprompts[num]->page[pagenum].align = textprompts[num]->page[metapagenum].align;
					textprompts[num]->page[pagenum].verticalalign = textprompts[num]->page[metapagenum].verticalalign;
					textprompts[num]->page[pagenum].textspeed = textprompts[num]->page[metapagenum].textspeed;
					textprompts[num]->page[pagenum].textsfx = textprompts[num]->page[metapagenum].textsfx;
					textprompts[num]->page[pagenum].hidehud = textprompts[num]->page[metapagenum].hidehud;

					// music: don't copy, else each page change may reset the music
				}
			}
			else if (fastcmp(word, "TAG"))
				strncpy(textprompts[num]->page[pagenum].tag, word2, 33);
			else if (fastcmp(word, "NEXTPROMPT"))
				textprompts[num]->page[pagenum].nextprompt = usi;
			else if (fastcmp(word, "NEXTPAGE"))
				textprompts[num]->page[pagenum].nextpage = usi;
			else if (fastcmp(word, "NEXTTAG"))
				strncpy(textprompts[num]->page[pagenum].nexttag, word2, 33);
			else if (fastcmp(word, "TIMETONEXT"))
				textprompts[num]->page[pagenum].timetonext = get_number(word2);
			else
				deh_warning("PromptPage %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readtextprompt(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *tmp;
	INT32 value;

	// Allocate memory for this prompt if we don't yet have any
	if (!textprompts[num])
		textprompts[num] = Z_Calloc(sizeof (textprompt_t), PU_STATIC, NULL);

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " ");
			if (word2)
				value = atoi(word2);
			else
			{
				deh_warning("No value for token %s", word);
				continue;
			}

			if (fastcmp(word, "NUMPAGES"))
			{
				textprompts[num]->numpages = min(max(value, 0), MAX_PAGES);
			}
			else if (fastcmp(word, "PAGE"))
			{
				if (1 <= value && value <= MAX_PAGES)
				{
					textprompts[num]->page[value - 1].backcolor = 1; // default to gray
					textprompts[num]->page[value - 1].hidehud = 1; // hide appropriate HUD elements
					readtextpromptpage(f, num, value - 1);
				}
				else
					deh_warning("Page number %d out of range (1 - %d)", value, MAX_PAGES);

			}
			else
				deh_warning("Prompt %d: unknown word '%s', Page <num> expected.", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readmenu(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 value;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = (tmp += 2);
			strupr(word2);

			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "BACKGROUNDNAME"))
			{
				strncpy(menupres[num].bgname, word2, 8);
				titlechanged = true;
			}
			else if (fastcmp(word, "HIDEBACKGROUND"))
			{
				menupres[num].bghide = (boolean)(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "BACKGROUNDCOLOR"))
			{
				menupres[num].bgcolor = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "HIDETITLEPICS") || fastcmp(word, "HIDEPICS") || fastcmp(word, "TITLEPICSHIDE"))
			{
				// true by default, except MM_MAIN
				menupres[num].hidetitlepics = (boolean)(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSMODE"))
			{
				if (fastcmp(word2, "USER"))
					menupres[num].ttmode = TTMODE_USER;
				else if (fastcmp(word2, "HIDE") || fastcmp(word2, "HIDDEN") || fastcmp(word2, "NONE"))
				{
					menupres[num].ttmode = TTMODE_USER;
					menupres[num].ttname[0] = 0;
					menupres[num].hidetitlepics = true;
				}
				else // if (fastcmp(word2, "OLD") || fastcmp(word2, "SSNTAILS"))
					menupres[num].ttmode = TTMODE_OLD;
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSSCALE"))
			{
				// Don't handle Alacroix special case here; see Maincfg section.
				menupres[num].ttscale = max(1, min(8, (UINT8)get_number(word2)));
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSNAME"))
			{
				strncpy(menupres[num].ttname, word2, 9);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSX"))
			{
				menupres[num].ttx = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSY"))
			{
				menupres[num].tty = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSLOOP"))
			{
				menupres[num].ttloop = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSTICS"))
			{
				menupres[num].tttics = (UINT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLESCROLLSPEED") || fastcmp(word, "TITLESCROLLXSPEED")
				|| fastcmp(word, "SCROLLSPEED") || fastcmp(word, "SCROLLXSPEED"))
			{
				menupres[num].titlescrollxspeed = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLESCROLLYSPEED") || fastcmp(word, "SCROLLYSPEED"))
			{
				menupres[num].titlescrollyspeed = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "MUSIC"))
			{
				strncpy(menupres[num].musname, word2, 7);
				menupres[num].musname[6] = 0;
				titlechanged = true;
			}
#ifdef MUSICSLOT_COMPATIBILITY
			else if (fastcmp(word, "MUSICSLOT"))
			{
				value = get_mus(word2, true);
				if (value && value <= 1035)
					snprintf(menupres[num].musname, 7, "%sM", G_BuildMapName(value));
				else if (value && value <= 1050)
					strncpy(menupres[num].musname, compat_special_music_slots[value - 1036], 7);
				else
					menupres[num].musname[0] = 0; // becomes empty string
				menupres[num].musname[6] = 0;
				titlechanged = true;
			}
#endif
			else if (fastcmp(word, "MUSICTRACK"))
			{
				menupres[num].mustrack = ((UINT16)value - 1);
				titlechanged = true;
			}
			else if (fastcmp(word, "MUSICLOOP"))
			{
				// true by default except MM_MAIN
				menupres[num].muslooping = (value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "NOMUSIC"))
			{
				menupres[num].musstop = (value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "IGNOREMUSIC"))
			{
				menupres[num].musignore = (value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "FADESTRENGTH"))
			{
				// one-based, <= 0 means use default value. 1-32
				menupres[num].fadestrength = get_number(word2)-1;
				titlechanged = true;
			}
			else if (fastcmp(word, "NOENTERBUBBLE"))
			{
				menupres[num].enterbubble = !(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "NOEXITBUBBLE"))
			{
				menupres[num].exitbubble = !(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "ENTERTAG"))
			{
				menupres[num].entertag = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "EXITTAG"))
			{
				menupres[num].exittag = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "ENTERWIPE"))
			{
				menupres[num].enterwipe = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "EXITWIPE"))
			{
				menupres[num].exitwipe = get_number(word2);
				titlechanged = true;
			}
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readhuditem(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 i;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			strupr(word2);

			i = atoi(word2); // used for numerical settings

			if (fastcmp(word, "X"))
			{
				hudinfo[num].x = i;
			}
			else if (fastcmp(word, "Y"))
			{
				hudinfo[num].y = i;
			}
			else if (fastcmp(word, "F"))
			{
				hudinfo[num].f = i;
			}
			else
				deh_warning("Level header %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

/*
Sprite number = 10
Sprite subnumber = 32968
Duration = 200
Next frame = 200
*/

/** Action pointer for reading actions from Dehacked lumps.
  */
typedef struct
{
	actionf_t action; ///< Function pointer corresponding to the actual action.
	const char *name; ///< Name of the action in ALL CAPS.
} actionpointer_t;

/** Array mapping action names to action functions.
  * Names must be in ALL CAPS for case insensitive comparisons.
  */
static actionpointer_t actionpointers[] =
{
	{{A_Explode},                "A_EXPLODE"},
	{{A_Pain},                   "A_PAIN"},
	{{A_Fall},                   "A_FALL"},
	{{A_Look},                   "A_LOOK"},
	{{A_Chase},                  "A_CHASE"},
	{{A_FaceStabChase},          "A_FACESTABCHASE"},
	{{A_FaceStabRev},            "A_FACESTABREV"},
	{{A_FaceStabHurl},           "A_FACESTABHURL"},
	{{A_FaceStabMiss},           "A_FACESTABMISS"},
	{{A_StatueBurst},            "A_STATUEBURST"},
	{{A_FaceTarget},             "A_FACETARGET"},
	{{A_FaceTracer},             "A_FACETRACER"},
	{{A_Scream},                 "A_SCREAM"},
	{{A_BossDeath},              "A_BOSSDEATH"},
	{{A_CustomPower},            "A_CUSTOMPOWER"},
	{{A_RingBox},                "A_RINGBOX"},
	{{A_Invincibility},          "A_INVINCIBILITY"},
	{{A_SuperSneakers},          "A_SUPERSNEAKERS"},
	{{A_BunnyHop},               "A_BUNNYHOP"},
	{{A_BubbleSpawn},            "A_BUBBLESPAWN"},
	{{A_FanBubbleSpawn},         "A_FANBUBBLESPAWN"},
	{{A_BubbleRise},             "A_BUBBLERISE"},
	{{A_BubbleCheck},            "A_BUBBLECHECK"},
	{{A_AwardScore},             "A_AWARDSCORE"},
	{{A_GiveShield},             "A_GIVESHIELD"},
	{{A_GravityBox},             "A_GRAVITYBOX"},
	{{A_ScoreRise},              "A_SCORERISE"},
	{{A_AttractChase},           "A_ATTRACTCHASE"},
	{{A_DropMine},               "A_DROPMINE"},
	{{A_FishJump},               "A_FISHJUMP"},
	{{A_SetSolidSteam},          "A_SETSOLIDSTEAM"},
	{{A_UnsetSolidSteam},        "A_UNSETSOLIDSTEAM"},
	{{A_OverlayThink},           "A_OVERLAYTHINK"},
	{{A_JetChase},               "A_JETCHASE"},
	{{A_JetbThink},              "A_JETBTHINK"},
	{{A_JetgThink},              "A_JETGTHINK"},
	{{A_JetgShoot},              "A_JETGSHOOT"},
	{{A_ShootBullet},            "A_SHOOTBULLET"},
	{{A_MinusDigging},           "A_MINUSDIGGING"},
	{{A_MinusPopup},             "A_MINUSPOPUP"},
	{{A_MinusCheck},             "A_MINUSCHECK"},
	{{A_ChickenCheck},           "A_CHICKENCHECK"},
	{{A_MouseThink},             "A_MOUSETHINK"},
	{{A_DetonChase},             "A_DETONCHASE"},
	{{A_CapeChase},              "A_CAPECHASE"},
	{{A_RotateSpikeBall},        "A_ROTATESPIKEBALL"},
	{{A_SlingAppear},            "A_SLINGAPPEAR"},
	{{A_UnidusBall},             "A_UNIDUSBALL"},
	{{A_RockSpawn},              "A_ROCKSPAWN"},
	{{A_SetFuse},                "A_SETFUSE"},
	{{A_CrawlaCommanderThink},   "A_CRAWLACOMMANDERTHINK"},
	{{A_SmokeTrailer},           "A_SMOKETRAILER"},
	{{A_RingExplode},            "A_RINGEXPLODE"},
	{{A_OldRingExplode},         "A_OLDRINGEXPLODE"},
	{{A_MixUp},                  "A_MIXUP"},
	{{A_Boss1Chase},             "A_BOSS1CHASE"},
	{{A_FocusTarget},            "A_FOCUSTARGET"},
	{{A_Boss2Chase},             "A_BOSS2CHASE"},
	{{A_Boss2Pogo},              "A_BOSS2POGO"},
	{{A_BossZoom},               "A_BOSSZOOM"},
	{{A_BossScream},             "A_BOSSSCREAM"},
	{{A_Boss2TakeDamage},        "A_BOSS2TAKEDAMAGE"},
	{{A_Boss7Chase},             "A_BOSS7CHASE"},
	{{A_GoopSplat},              "A_GOOPSPLAT"},
	{{A_Boss2PogoSFX},           "A_BOSS2POGOSFX"},
	{{A_Boss2PogoTarget},        "A_BOSS2POGOTARGET"},
	{{A_BossJetFume},            "A_BOSSJETFUME"},
	{{A_EggmanBox},              "A_EGGMANBOX"},
	{{A_TurretFire},             "A_TURRETFIRE"},
	{{A_SuperTurretFire},        "A_SUPERTURRETFIRE"},
	{{A_TurretStop},             "A_TURRETSTOP"},
	{{A_JetJawRoam},             "A_JETJAWROAM"},
	{{A_JetJawChomp},            "A_JETJAWCHOMP"},
	{{A_PointyThink},            "A_POINTYTHINK"},
	{{A_CheckBuddy},             "A_CHECKBUDDY"},
	{{A_HoodFire},               "A_HOODFIRE"},
	{{A_HoodThink},              "A_HOODTHINK"},
	{{A_HoodFall},               "A_HOODFALL"},
	{{A_ArrowBonks},             "A_ARROWBONKS"},
	{{A_SnailerThink},           "A_SNAILERTHINK"},
	{{A_SharpChase},             "A_SHARPCHASE"},
	{{A_SharpSpin},              "A_SHARPSPIN"},
	{{A_SharpDecel},             "A_SHARPDECEL"},
	{{A_CrushstaceanWalk},       "A_CRUSHSTACEANWALK"},
	{{A_CrushstaceanPunch},      "A_CRUSHSTACEANPUNCH"},
	{{A_CrushclawAim},           "A_CRUSHCLAWAIM"},
	{{A_CrushclawLaunch},        "A_CRUSHCLAWLAUNCH"},
	{{A_VultureVtol},            "A_VULTUREVTOL"},
	{{A_VultureCheck},           "A_VULTURECHECK"},
	{{A_VultureHover},           "A_VULTUREHOVER"},
	{{A_VultureBlast},           "A_VULTUREBLAST"},
	{{A_VultureFly},             "A_VULTUREFLY"},
	{{A_SkimChase},              "A_SKIMCHASE"},
	{{A_SkullAttack},            "A_SKULLATTACK"},
	{{A_LobShot},                "A_LOBSHOT"},
	{{A_FireShot},               "A_FIRESHOT"},
	{{A_SuperFireShot},          "A_SUPERFIRESHOT"},
	{{A_BossFireShot},           "A_BOSSFIRESHOT"},
	{{A_Boss7FireMissiles},      "A_BOSS7FIREMISSILES"},
	{{A_Boss1Laser},             "A_BOSS1LASER"},
	{{A_Boss4Reverse},           "A_BOSS4REVERSE"},
	{{A_Boss4SpeedUp},           "A_BOSS4SPEEDUP"},
	{{A_Boss4Raise},             "A_BOSS4RAISE"},
	{{A_SparkFollow},            "A_SPARKFOLLOW"},
	{{A_BuzzFly},                "A_BUZZFLY"},
	{{A_GuardChase},             "A_GUARDCHASE"},
	{{A_EggShield},              "A_EGGSHIELD"},
	{{A_SetReactionTime},        "A_SETREACTIONTIME"},
	{{A_Boss1Spikeballs},        "A_BOSS1SPIKEBALLS"},
	{{A_Boss3TakeDamage},        "A_BOSS3TAKEDAMAGE"},
	{{A_Boss3Path},              "A_BOSS3PATH"},
	{{A_Boss3ShockThink},        "A_BOSS3SHOCKTHINK"},
	{{A_LinedefExecute},         "A_LINEDEFEXECUTE"},
	{{A_PlaySeeSound},           "A_PLAYSEESOUND"},
	{{A_PlayAttackSound},        "A_PLAYATTACKSOUND"},
	{{A_PlayActiveSound},        "A_PLAYACTIVESOUND"},
	{{A_SpawnObjectAbsolute},    "A_SPAWNOBJECTABSOLUTE"},
	{{A_SpawnObjectRelative},    "A_SPAWNOBJECTRELATIVE"},
	{{A_ChangeAngleRelative},    "A_CHANGEANGLERELATIVE"},
	{{A_ChangeAngleAbsolute},    "A_CHANGEANGLEABSOLUTE"},
	{{A_RollAngle},              "A_ROLLANGLE"},
	{{A_ChangeRollAngleRelative},"A_CHANGEROLLANGLERELATIVE"},
	{{A_ChangeRollAngleAbsolute},"A_CHANGEROLLANGLEABSOLUTE"},
	{{A_PlaySound},              "A_PLAYSOUND"},
	{{A_FindTarget},             "A_FINDTARGET"},
	{{A_FindTracer},             "A_FINDTRACER"},
	{{A_SetTics},                "A_SETTICS"},
	{{A_SetRandomTics},          "A_SETRANDOMTICS"},
	{{A_ChangeColorRelative},    "A_CHANGECOLORRELATIVE"},
	{{A_ChangeColorAbsolute},    "A_CHANGECOLORABSOLUTE"},
	{{A_Dye},                    "A_DYE"},
	{{A_MoveRelative},           "A_MOVERELATIVE"},
	{{A_MoveAbsolute},           "A_MOVEABSOLUTE"},
	{{A_Thrust},                 "A_THRUST"},
	{{A_ZThrust},                "A_ZTHRUST"},
	{{A_SetTargetsTarget},       "A_SETTARGETSTARGET"},
	{{A_SetObjectFlags},         "A_SETOBJECTFLAGS"},
	{{A_SetObjectFlags2},        "A_SETOBJECTFLAGS2"},
	{{A_RandomState},            "A_RANDOMSTATE"},
	{{A_RandomStateRange},       "A_RANDOMSTATERANGE"},
	{{A_DualAction},             "A_DUALACTION"},
	{{A_RemoteAction},           "A_REMOTEACTION"},
	{{A_ToggleFlameJet},         "A_TOGGLEFLAMEJET"},
	{{A_OrbitNights},            "A_ORBITNIGHTS"},
	{{A_GhostMe},                "A_GHOSTME"},
	{{A_SetObjectState},         "A_SETOBJECTSTATE"},
	{{A_SetObjectTypeState},     "A_SETOBJECTTYPESTATE"},
	{{A_KnockBack},              "A_KNOCKBACK"},
	{{A_PushAway},               "A_PUSHAWAY"},
	{{A_RingDrain},              "A_RINGDRAIN"},
	{{A_SplitShot},              "A_SPLITSHOT"},
	{{A_MissileSplit},           "A_MISSILESPLIT"},
	{{A_MultiShot},              "A_MULTISHOT"},
	{{A_InstaLoop},              "A_INSTALOOP"},
	{{A_Custom3DRotate},         "A_CUSTOM3DROTATE"},
	{{A_SearchForPlayers},       "A_SEARCHFORPLAYERS"},
	{{A_CheckRandom},            "A_CHECKRANDOM"},
	{{A_CheckTargetRings},       "A_CHECKTARGETRINGS"},
	{{A_CheckRings},             "A_CHECKRINGS"},
	{{A_CheckTotalRings},        "A_CHECKTOTALRINGS"},
	{{A_CheckHealth},            "A_CHECKHEALTH"},
	{{A_CheckRange},             "A_CHECKRANGE"},
	{{A_CheckHeight},            "A_CHECKHEIGHT"},
	{{A_CheckTrueRange},         "A_CHECKTRUERANGE"},
	{{A_CheckThingCount},        "A_CHECKTHINGCOUNT"},
	{{A_CheckAmbush},            "A_CHECKAMBUSH"},
	{{A_CheckCustomValue},       "A_CHECKCUSTOMVALUE"},
	{{A_CheckCusValMemo},        "A_CHECKCUSVALMEMO"},
	{{A_SetCustomValue},         "A_SETCUSTOMVALUE"},
	{{A_UseCusValMemo},          "A_USECUSVALMEMO"},
	{{A_RelayCustomValue},       "A_RELAYCUSTOMVALUE"},
	{{A_CusValAction},           "A_CUSVALACTION"},
	{{A_ForceStop},              "A_FORCESTOP"},
	{{A_ForceWin},               "A_FORCEWIN"},
	{{A_SpikeRetract},           "A_SPIKERETRACT"},
	{{A_InfoState},              "A_INFOSTATE"},
	{{A_Repeat},                 "A_REPEAT"},
	{{A_SetScale},               "A_SETSCALE"},
	{{A_RemoteDamage},           "A_REMOTEDAMAGE"},
	{{A_HomingChase},            "A_HOMINGCHASE"},
	{{A_TrapShot},               "A_TRAPSHOT"},
	{{A_VileTarget},             "A_VILETARGET"},
	{{A_VileAttack},             "A_VILEATTACK"},
	{{A_VileFire},               "A_VILEFIRE"},
	{{A_BrakChase},              "A_BRAKCHASE"},
	{{A_BrakFireShot},           "A_BRAKFIRESHOT"},
	{{A_BrakLobShot},            "A_BRAKLOBSHOT"},
	{{A_NapalmScatter},          "A_NAPALMSCATTER"},
	{{A_SpawnFreshCopy},         "A_SPAWNFRESHCOPY"},
	{{A_FlickySpawn},            "A_FLICKYSPAWN"},
	{{A_FlickyCenter},           "A_FLICKYCENTER"},
	{{A_FlickyAim},              "A_FLICKYAIM"},
	{{A_FlickyFly},              "A_FLICKYFLY"},
	{{A_FlickySoar},             "A_FLICKYSOAR"},
	{{A_FlickyCoast},            "A_FLICKYCOAST"},
	{{A_FlickyHop},              "A_FLICKYHOP"},
	{{A_FlickyFlounder},         "A_FLICKYFLOUNDER"},
	{{A_FlickyCheck},            "A_FLICKYCHECK"},
	{{A_FlickyHeightCheck},      "A_FLICKYHEIGHTCHECK"},
	{{A_FlickyFlutter},          "A_FLICKYFLUTTER"},
	{{A_FlameParticle},          "A_FLAMEPARTICLE"},
	{{A_FadeOverlay},            "A_FADEOVERLAY"},
	{{A_Boss5Jump},              "A_BOSS5JUMP"},
	{{A_LightBeamReset},         "A_LIGHTBEAMRESET"},
	{{A_MineExplode},            "A_MINEEXPLODE"},
	{{A_MineRange},              "A_MINERANGE"},
	{{A_ConnectToGround},        "A_CONNECTTOGROUND"},
	{{A_SpawnParticleRelative},  "A_SPAWNPARTICLERELATIVE"},
	{{A_MultiShotDist},          "A_MULTISHOTDIST"},
	{{A_WhoCaresIfYourSonIsABee},"A_WHOCARESIFYOURSONISABEE"},
	{{A_ParentTriesToSleep},     "A_PARENTTRIESTOSLEEP"},
	{{A_CryingToMomma},          "A_CRYINGTOMOMMA"},
	{{A_CheckFlags2},            "A_CHECKFLAGS2"},
	{{A_Boss5FindWaypoint},      "A_BOSS5FINDWAYPOINT"},
	{{A_DoNPCSkid},              "A_DONPCSKID"},
	{{A_DoNPCPain},              "A_DONPCPAIN"},
	{{A_PrepareRepeat},          "A_PREPAREREPEAT"},
	{{A_Boss5ExtraRepeat},       "A_BOSS5EXTRAREPEAT"},
	{{A_Boss5Calm},              "A_BOSS5CALM"},
	{{A_Boss5CheckOnGround},     "A_BOSS5CHECKONGROUND"},
	{{A_Boss5CheckFalling},      "A_BOSS5CHECKFALLING"},
	{{A_Boss5PinchShot},         "A_BOSS5PINCHSHOT"},
	{{A_Boss5MakeItRain},        "A_BOSS5MAKEITRAIN"},
	{{A_Boss5MakeJunk},          "A_BOSS5MAKEJUNK"},
	{{A_LookForBetter},          "A_LOOKFORBETTER"},
	{{A_Boss5BombExplode},       "A_BOSS5BOMBEXPLODE"},
	{{A_TNTExplode},             "A_TNTEXPLODE"},
	{{A_DebrisRandom},           "A_DEBRISRANDOM"},
	{{A_TrainCameo},             "A_TRAINCAMEO"},
	{{A_TrainCameo2},            "A_TRAINCAMEO2"},
	{{A_CanarivoreGas},          "A_CANARIVOREGAS"},
	{{A_KillSegments},           "A_KILLSEGMENTS"},
	{{A_SnapperSpawn},           "A_SNAPPERSPAWN"},
	{{A_SnapperThinker},         "A_SNAPPERTHINKER"},
	{{A_SaloonDoorSpawn},        "A_SALOONDOORSPAWN"},
	{{A_MinecartSparkThink},     "A_MINECARTSPARKTHINK"},
	{{A_ModuloToState},          "A_MODULOTOSTATE"},
	{{A_LavafallRocks},          "A_LAVAFALLROCKS"},
	{{A_LavafallLava},           "A_LAVAFALLLAVA"},
	{{A_FallingLavaCheck},       "A_FALLINGLAVACHECK"},
	{{A_FireShrink},             "A_FIRESHRINK"},
	{{A_SpawnPterabytes},        "A_SPAWNPTERABYTES"},
	{{A_PterabyteHover},         "A_PTERABYTEHOVER"},
	{{A_RolloutSpawn},           "A_ROLLOUTSPAWN"},
	{{A_RolloutRock},            "A_ROLLOUTROCK"},
	{{A_DragonbomberSpawn},      "A_DRAGONBOMERSPAWN"},
	{{A_DragonWing},             "A_DRAGONWING"},
	{{A_DragonSegment},          "A_DRAGONSEGMENT"},
	{{A_ChangeHeight},           "A_CHANGEHEIGHT"},
	{{NULL},                     "NONE"},

	// This NULL entry must be the last in the list
	{{NULL},                   NULL},
};

static void readframe(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word1;
	char *word2 = NULL;
	char *tmp;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word1 = strtok(s, " ");
			if (word1)
				strupr(word1);
			else
				break;

			word2 = strtok(NULL, " = ");
			if (word2)
				strupr(word2);
			else
				break;
			if (word2[strlen(word2)-1] == '\n')
				word2[strlen(word2)-1] = '\0';

			if (fastcmp(word1, "SPRITENUMBER") || fastcmp(word1, "SPRITENAME"))
			{
				states[num].sprite = get_sprite(word2);
			}
			else if (fastcmp(word1, "SPRITESUBNUMBER") || fastcmp(word1, "SPRITEFRAME"))
			{
				states[num].frame = (INT32)get_number(word2); // So the FF_ flags get calculated
			}
			else if (fastcmp(word1, "DURATION"))
			{
				states[num].tics = (INT32)get_number(word2); // So TICRATE can be used
			}
			else if (fastcmp(word1, "NEXT"))
			{
				states[num].nextstate = get_state(word2);
			}
			else if (fastcmp(word1, "VAR1"))
			{
				states[num].var1 = (INT32)get_number(word2);
			}
			else if (fastcmp(word1, "VAR2"))
			{
				states[num].var2 = (INT32)get_number(word2);
			}
			else if (fastcmp(word1, "ACTION"))
			{
				size_t z;
				boolean found = false;
				char actiontocompare[32];

				memset(actiontocompare, 0x00, sizeof(actiontocompare));
				strlcpy(actiontocompare, word2, sizeof (actiontocompare));
				strupr(actiontocompare);

				for (z = 0; z < 32; z++)
				{
					if (actiontocompare[z] == '\n' || actiontocompare[z] == '\r')
					{
						actiontocompare[z] = '\0';
						break;
					}
				}

				for (z = 0; actionpointers[z].name; z++)
				{
					if (actionpointers[z].action.acv == states[num].action.acv)
						break;
				}

				z = 0;
				found = LUA_SetLuaAction(&states[num], actiontocompare);
				if (!found)
					while (actionpointers[z].name)
					{
						if (fastcmp(actiontocompare, actionpointers[z].name))
						{
							states[num].action = actionpointers[z].action;
							states[num].action.acv = actionpointers[z].action.acv; // assign
							states[num].action.acp1 = actionpointers[z].action.acp1;
							found = true;
							break;
						}
						z++;
					}

				if (!found)
					deh_warning("Unknown action %s", actiontocompare);
			}
			else
				deh_warning("Frame %d: unknown word '%s'", num, word1);
		}
	} while (!myfeof(f));

	Z_Free(s);
}

static void readsound(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word;
	char *word2;
	char *tmp;
	INT32 value;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			word = strtok(s, " ");
			if (word)
				strupr(word);
			else
				break;

			word2 = strtok(NULL, " ");
			if (word2)
				value = atoi(word2);
			else
			{
				deh_warning("No value for token %s", word);
				continue;
			}

			if (fastcmp(word, "SINGULAR"))
			{
				S_sfx[num].singularity = value;
			}
			else if (fastcmp(word, "PRIORITY"))
			{
				S_sfx[num].priority = value;
			}
			else if (fastcmp(word, "FLAGS"))
			{
				S_sfx[num].pitch = value;
			}
			else if (fastcmp(word, "CAPTION") || fastcmp(word, "DESCRIPTION"))
			{
				deh_strlcpy(S_sfx[num].caption, word2,
					sizeof(S_sfx[num].caption), va("Sound effect %d: caption", num));
			}
			else
				deh_warning("Sound %d : unknown word '%s'",num,word);
		}
	} while (!myfeof(f));

	Z_Free(s);
}

/** Checks if a game data file name for a mod is good.
 * "Good" means that it contains only alphanumerics, _, and -;
 * ends in ".dat"; has at least one character before the ".dat";
 * and is not "gamedata.dat" (tested case-insensitively).
 *
 * Assumption: that gamedata.dat is the only .dat file that will
 * ever be treated specially by the game.
 *
 * Note: Check for the tail ".dat" case-insensitively since at
 * present, we get passed the filename in all uppercase.
 *
 * \param s Filename string to check.
 * \return True if the filename is good.
 * \sa readmaincfg()
 * \author Graue <graue@oceanbase.org>
 */
static boolean GoodDataFileName(const char *s)
{
	const char *p;
	const char *tail = ".dat";

	for (p = s; *p != '\0'; p++)
		if (!isalnum(*p) && *p != '_' && *p != '-' && *p != '.')
			return false;

	p = s + strlen(s) - strlen(tail);
	if (p <= s) return false; // too short
	if (!fasticmp(p, tail)) return false; // doesn't end in .dat

	if (fasticmp(s, "gamedata.dat")) return false; // Vanilla SRB2 gamedata
	if (fasticmp(s, "main.dat")) return false; // Vanilla SRB2 time attack replay folder
	if (fasticmp(s, "kartdata.dat")) return false; // SRB2Kart gamedata
	if (fasticmp(s, "kart.dat")) return false; // SRB2Kart time attack replay folder
	if (fasticmp(s, "online.dat")) return false; // SRB2Kart online replay folder

	return true;
}

static void reademblemdata(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 value;

	memset(&emblemlocations[num-1], 0, sizeof(emblem_t));

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			value = atoi(word2); // used for numerical settings

			// Up here to allow lowercase in hints
			if (fastcmp(word, "HINT"))
			{
				while ((tmp = strchr(word2, '\\')))
					*tmp = '\n';
				deh_strlcpy(emblemlocations[num-1].hint, word2, sizeof (emblemlocations[num-1].hint), va("Emblem %d: hint", num));
				continue;
			}
			strupr(word2);

			if (fastcmp(word, "TYPE"))
			{
				if (fastcmp(word2, "GLOBAL"))
					emblemlocations[num-1].type = ET_GLOBAL;
				else if (fastcmp(word2, "MAP"))
					emblemlocations[num-1].type = ET_MAP;
				else if (fastcmp(word2, "TIME"))
					emblemlocations[num-1].type = ET_TIME;
				else
					emblemlocations[num-1].type = (UINT8)value;
			}
			else if (fastcmp(word, "TAG"))
				emblemlocations[num-1].tag = (INT16)value;
			else if (fastcmp(word, "MAPNUM"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);

				emblemlocations[num-1].level = (INT16)value;
			}
			else if (fastcmp(word, "SPRITE"))
			{
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = word2[0];
				else
					value += 'A'-1;

				if (value < 'A' || value > 'Z')
					deh_warning("Emblem %d: sprite must be from A - Z (1 - 26)", num);
				else
					emblemlocations[num-1].sprite = (UINT8)value;
			}
			else if (fastcmp(word, "COLOR"))
				emblemlocations[num-1].color = get_number(word2);
			else if (fastcmp(word, "VAR"))
				emblemlocations[num-1].var = get_number(word2);
			else
				deh_warning("Emblem %d: unknown word '%s'", num, word);
		}
	} while (!myfeof(f));

	// Default sprite and color definitions for lazy people like me
	if (!emblemlocations[num-1].sprite) switch (emblemlocations[num-1].type)
	{
		case ET_TIME:
			emblemlocations[num-1].sprite = 'B'; break;
		default:
			emblemlocations[num-1].sprite = 'A'; break;
	}
	if (!emblemlocations[num-1].color) switch (emblemlocations[num-1].type)
	{
		case ET_TIME: //case ET_NTIME:
			emblemlocations[num-1].color = SKINCOLOR_GREY; break;
		default:
			emblemlocations[num-1].color = SKINCOLOR_GOLD; break;
	}

	Z_Free(s);
}

static void readextraemblemdata(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 value;

	memset(&extraemblems[num-1], 0, sizeof(extraemblem_t));

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;

			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "NAME"))
				deh_strlcpy(extraemblems[num-1].name, word2,
					sizeof (extraemblems[num-1].name), va("Extra emblem %d: name", num));
			else if (fastcmp(word, "OBJECTIVE"))
				deh_strlcpy(extraemblems[num-1].description, word2,
					sizeof (extraemblems[num-1].description), va("Extra emblem %d: objective", num));
			else if (fastcmp(word, "CONDITIONSET"))
				extraemblems[num-1].conditionset = (UINT8)value;
			else if (fastcmp(word, "SHOWCONDITIONSET"))
				extraemblems[num-1].showconditionset = (UINT8)value;
			else
			{
				strupr(word2);
				if (fastcmp(word, "SPRITE"))
				{
					if (word2[0] >= 'A' && word2[0] <= 'Z')
						value = word2[0];
					else
						value += 'A'-1;

					if (value < 'A' || value > 'Z')
						deh_warning("Emblem %d: sprite must be from A - Z (1 - 26)", num);
					else
						extraemblems[num-1].sprite = (UINT8)value;
				}
				else if (fastcmp(word, "COLOR"))
					extraemblems[num-1].color = get_number(word2);
				else
					deh_warning("Extra emblem %d: unknown word '%s'", num, word);
			}
		}
	} while (!myfeof(f));

	if (!extraemblems[num-1].sprite)
		extraemblems[num-1].sprite = 'C';
	if (!extraemblems[num-1].color)
		extraemblems[num-1].color = SKINCOLOR_RED;

	Z_Free(s);
}

static void readunlockable(MYFILE *f, INT32 num)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 i;

	memset(&unlockables[num], 0, sizeof(unlockable_t));
	unlockables[num].objective[0] = '/';

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;

			i = atoi(word2); // used for numerical settings

			if (fastcmp(word, "NAME"))
				deh_strlcpy(unlockables[num].name, word2,
					sizeof (unlockables[num].name), va("Unlockable %d: name", num));
			else if (fastcmp(word, "OBJECTIVE"))
				deh_strlcpy(unlockables[num].objective, word2,
					sizeof (unlockables[num].objective), va("Unlockable %d: objective", num));
			else
			{
				strupr(word2);

				if (fastcmp(word, "CONDITIONSET"))
					unlockables[num].conditionset = (UINT8)i;
				else if (fastcmp(word, "SHOWCONDITIONSET"))
					unlockables[num].showconditionset = (UINT8)i;
				else if (fastcmp(word, "NOCECHO"))
					unlockables[num].nocecho = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
				else if (fastcmp(word, "NOCHECKLIST"))
					unlockables[num].nochecklist = (UINT8)(i || word2[0] == 'T' || word2[0] == 'Y');
				else if (fastcmp(word, "TYPE"))
				{
					if (fastcmp(word2, "NONE"))
						unlockables[num].type = SECRET_NONE;
					else if (fastcmp(word2, "HEADER"))
						unlockables[num].type = SECRET_HEADER;
					else if (fastcmp(word2, "SKIN"))
						unlockables[num].type = SECRET_SKIN;
					else if (fastcmp(word2, "WARP"))
						unlockables[num].type = SECRET_WARP;
					else if (fastcmp(word2, "LEVELSELECT"))
						unlockables[num].type = SECRET_LEVELSELECT;
					else if (fastcmp(word2, "TIMEATTACK"))
						unlockables[num].type = SECRET_TIMEATTACK;
					else if (fastcmp(word2, "BREAKTHECAPSULES"))
						unlockables[num].type = SECRET_BREAKTHECAPSULES;
					else if (fastcmp(word2, "SOUNDTEST"))
						unlockables[num].type = SECRET_SOUNDTEST;
					else if (fastcmp(word2, "CREDITS"))
						unlockables[num].type = SECRET_CREDITS;
					else if (fastcmp(word2, "ITEMFINDER"))
						unlockables[num].type = SECRET_ITEMFINDER;
					else if (fastcmp(word2, "EMBLEMHINTS"))
						unlockables[num].type = SECRET_EMBLEMHINTS;
					else if (fastcmp(word2, "ENCORE"))
						unlockables[num].type = SECRET_ENCORE;
					else if (fastcmp(word2, "HARDSPEED"))
						unlockables[num].type = SECRET_HARDSPEED;
					else if (fastcmp(word2, "HELLATTACK"))
						unlockables[num].type = SECRET_HELLATTACK;
					else if (fastcmp(word2, "PANDORA"))
						unlockables[num].type = SECRET_PANDORA;
					else
						unlockables[num].type = (INT16)i;
				}
				else if (fastcmp(word, "VAR"))
				{
					// Support using the actual map name,
					// i.e., Level AB, Level FZ, etc.

					// Convert to map number
					if (word2[0] >= 'A' && word2[0] <= 'Z')
						i = M_MapNumber(word2[0], word2[1]);

					unlockables[num].variable = (INT16)i;
				}
				else
					deh_warning("Unlockable %d: unknown word '%s'", num+1, word);
			}
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static const char NIGHTSGRADE_LIST[] = {
	'F', // GRADE_F
	'E', // GRADE_E
	'D', // GRADE_D
	'C', // GRADE_C
	'B', // GRADE_B
	'A', // GRADE_A
	'S', // GRADE_S
	'\0'
};

#define PARAMCHECK(n) do { if (!params[n]) { deh_warning("Too few parameters, need %d", n); return; }} while (0)
static void readcondition(UINT8 set, UINT32 id, char *word2)
{
	INT32 i;
	char *params[4]; // condition, requirement, extra info, extra info
	char *spos;

	conditiontype_t ty;
	INT32 re;
	INT16 x1 = 0, x2 = 0;

	INT32 offset = 0;

	spos = strtok(word2, " ");

	for (i = 0; i < 4; ++i)
	{
		if (spos != NULL)
		{
			params[i] = spos;
			spos = strtok(NULL, " ");
		}
		else
			params[i] = NULL;
	}

	if (!params[0])
	{
		deh_warning("condition line is empty");
		return;
	}

	if				(fastcmp(params[0], "PLAYTIME")
	|| (++offset && fastcmp(params[0], "MATCHESPLAYED")))
	{
		PARAMCHECK(1);
		ty = UC_PLAYTIME + offset;
		re = atoi(params[1]);
	}
	else if (fastcmp(params[0], "POWERLEVEL"))
	{
		PARAMCHECK(2);
		ty = UC_POWERLEVEL;
		re = atoi(params[1]);
		x1 = atoi(params[2]);

		if (x1 < 0 || x1 >= PWRLV_NUMTYPES)
		{
			deh_warning("Power level type %d out of range (0 - %d)", x1, PWRLV_NUMTYPES-1);
			return;
		}
	}
	else if (fastcmp(params[0], "GAMECLEAR"))
	{
		ty = UC_GAMECLEAR;
		re = (params[1]) ? atoi(params[1]) : 1;
	}
	else if (fastcmp(params[0], "OVERALLTIME"))
	{
		PARAMCHECK(1);
		ty = UC_OVERALLTIME;
		re = atoi(params[1]);
	}
	else if ((offset=0) || fastcmp(params[0], "MAPVISITED")
	||        (++offset && fastcmp(params[0], "MAPBEATEN")))
	{
		PARAMCHECK(1);
		ty = UC_MAPVISITED + offset;

		// Convert to map number if it appears to be one
		if (params[1][0] >= 'A' && params[1][0] <= 'Z')
			re = M_MapNumber(params[1][0], params[1][1]);
		else
			re = atoi(params[1]);

		if (re < 0 || re >= NUMMAPS)
		{
			deh_warning("Level number %d out of range (1 - %d)", re, NUMMAPS);
			return;
		}
	}
	else if (fastcmp(params[0], "MAPTIME"))
	{
		PARAMCHECK(2);
		ty = UC_MAPTIME;
		re = atoi(params[2]);

		// Convert to map number if it appears to be one
		if (params[1][0] >= 'A' && params[1][0] <= 'Z')
			x1 = (INT16)M_MapNumber(params[1][0], params[1][1]);
		else
			x1 = (INT16)atoi(params[1]);

		if (x1 < 0 || x1 >= NUMMAPS)
		{
			deh_warning("Level number %d out of range (1 - %d)", x1, NUMMAPS);
			return;
		}
	}
	else if (fastcmp(params[0], "TRIGGER"))
	{
		PARAMCHECK(1);
		ty = UC_TRIGGER;
		re = atoi(params[1]);

		// constrained by 32 bits
		if (re < 0 || re > 31)
		{
			deh_warning("Trigger ID %d out of range (0 - 31)", re);
			return;
		}
	}
	else if (fastcmp(params[0], "TOTALEMBLEMS"))
	{
		PARAMCHECK(1);
		ty = UC_TOTALEMBLEMS;
		re = atoi(params[1]);
	}
	else if (fastcmp(params[0], "EMBLEM"))
	{
		PARAMCHECK(1);
		ty = UC_EMBLEM;
		re = atoi(params[1]);

		if (re <= 0 || re > MAXEMBLEMS)
		{
			deh_warning("Emblem %d out of range (1 - %d)", re, MAXEMBLEMS);
			return;
		}
	}
	else if (fastcmp(params[0], "EXTRAEMBLEM"))
	{
		PARAMCHECK(1);
		ty = UC_EXTRAEMBLEM;
		re = atoi(params[1]);

		if (re <= 0 || re > MAXEXTRAEMBLEMS)
		{
			deh_warning("Extra emblem %d out of range (1 - %d)", re, MAXEXTRAEMBLEMS);
			return;
		}
	}
	else if (fastcmp(params[0], "CONDITIONSET"))
	{
		PARAMCHECK(1);
		ty = UC_CONDITIONSET;
		re = atoi(params[1]);

		if (re <= 0 || re > MAXCONDITIONSETS)
		{
			deh_warning("Condition set %d out of range (1 - %d)", re, MAXCONDITIONSETS);
			return;
		}
	}
	else
	{
		deh_warning("Invalid condition name %s", params[0]);
		return;
	}

	M_AddRawCondition(set, (UINT8)id, ty, re, x1, x2);
}
#undef PARAMCHECK

static void readconditionset(MYFILE *f, UINT8 setnum)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	UINT8 id;
	UINT8 previd = 0;

	M_ClearConditionSet(setnum);

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			strupr(word2);

			if (fastncmp(word, "CONDITION", 9))
			{
				id = (UINT8)atoi(word + 9);
				if (id == 0)
				{
					deh_warning("Condition set %d: unknown word '%s'", setnum, word);
					continue;
				}
				else if (previd > id)
				{
					// out of order conditions can cause problems, so enforce proper order
					deh_warning("Condition set %d: conditions are out of order, ignoring this line", setnum);
					continue;
				}
				previd = id;

				readcondition(setnum, id, word2);
			}
			else
				deh_warning("Condition set %d: unknown word '%s'", setnum, word);
		}
	} while (!myfeof(f)); // finish when the line is empty

	Z_Free(s);
}

static void readmaincfg(MYFILE *f)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *word2;
	char *tmp;
	INT32 value;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			strupr(word2);

			value = atoi(word2); // used for numerical settings

			if (fastcmp(word, "EXECCFG"))
			{
				if (strchr(word2, '.'))
					COM_BufAddText(va("exec %s\n", word2));
				else
				{
					lumpnum_t lumpnum;
					char newname[9];

					strncpy(newname, word2, 8);

					newname[8] = '\0';

					lumpnum = W_CheckNumForName(newname);

					if (lumpnum == LUMPERROR || W_LumpLength(lumpnum) == 0)
						CONS_Debug(DBG_SETUP, "SOC Error: script lump %s not found/not valid.\n", newname);
					else
						COM_BufInsertText(W_CacheLumpNum(lumpnum, PU_CACHE));
				}
			}

			else if (fastcmp(word, "SPSTAGE_START"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				spstage_start = spmarathon_start = (INT16)value;
			}
			else if (fastcmp(word, "SPMARATHON_START"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				spmarathon_start = (INT16)value;
			}
			else if (fastcmp(word, "SSTAGE_START"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				sstage_start = (INT16)value;
				sstage_end = (INT16)(sstage_start+7); // 7 special stages total plus one weirdo
			}
			else if (fastcmp(word, "SMPSTAGE_START"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				smpstage_start = (INT16)value;
				smpstage_end = (INT16)(smpstage_start+6); // 7 special stages total
			}
			else if (fastcmp(word, "REDTEAM"))
			{
				skincolor_redteam = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "BLUETEAM"))
			{
				skincolor_blueteam = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "REDRING"))
			{
				skincolor_redring = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "BLUERING"))
			{
				skincolor_bluering = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "INVULNTICS"))
			{
				invulntics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "SNEAKERTICS"))
			{
				sneakertics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "FLASHINGTICS"))
			{
				flashingtics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "TAILSFLYTICS"))
			{
				tailsflytics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "UNDERWATERTICS"))
			{
				underwatertics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "SPACETIMETICS"))
			{
				spacetimetics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "EXTRALIFETICS"))
			{
				extralifetics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "NIGHTSLINKTICS"))
			{
				nightslinktics = (UINT16)get_number(word2);
			}
			else if (fastcmp(word, "GAMEOVERTICS"))
			{
				gameovertics = get_number(word2);
			}
			else if (fastcmp(word, "AMMOREMOVALTICS"))
			{
				ammoremovaltics = get_number(word2);
			}
			else if (fastcmp(word, "INTROTOPLAY"))
			{
				introtoplay = (UINT8)get_number(word2);
				// range check, you morons.
				if (introtoplay > 128)
					introtoplay = 128;
				introchanged = true;
			}
			else if (fastcmp(word, "CREDITSCUTSCENE"))
			{
				creditscutscene = (UINT8)get_number(word2);
				// range check, you morons.
				if (creditscutscene > 128)
					creditscutscene = 128;
			}
			else if (fastcmp(word, "USEBLACKROCK"))
			{
				useBlackRock = (UINT8)(value || word2[0] == 'T' || word2[0] == 'Y');
			}
			else if (fastcmp(word, "LOOPTITLE"))
			{
				looptitle = (value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEMAP"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				titlemap = (INT16)value;
				titlechanged = true;
			}
			else if (fastcmp(word, "HIDETITLEPICS") || fastcmp(word, "TITLEPICSHIDE"))
			{
				hidetitlepics = (boolean)(value || word2[0] == 'T' || word2[0] == 'Y');
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSMODE"))
			{
				if (fastcmp(word2, "USER"))
					ttmode = TTMODE_USER;
				else if (fastcmp(word2, "HIDE") || fastcmp(word2, "HIDDEN") || fastcmp(word2, "NONE"))
				{
					ttmode = TTMODE_USER;
					ttname[0] = 0;
					hidetitlepics = true;
				}
				else // if (fastcmp(word2, "OLD") || fastcmp(word2, "SSNTAILS"))
					ttmode = TTMODE_OLD;
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSNAME"))
			{
				strncpy(ttname, word2, 9);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSX"))
			{
				ttx = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSY"))
			{
				tty = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSLOOP"))
			{
				ttloop = (INT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLEPICSTICS"))
			{
				tttics = (UINT16)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLESCROLLSPEED") || fastcmp(word, "TITLESCROLLXSPEED"))
			{
				titlescrollxspeed = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "TITLESCROLLYSPEED"))
			{
				titlescrollyspeed = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "CREDITSCUTSCENE"))
			{
				creditscutscene = (UINT8)get_number(word2);
				// range check, you morons.
				if (creditscutscene > 128)
					creditscutscene = 128;
			}
			else if (fastcmp(word, "NUMDEMOS"))
			{
				numDemos = (UINT8)get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "DEMODELAYTIME"))
			{
				demoDelayTime = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "DEMOIDLETIME"))
			{
				demoIdleTime = get_number(word2);
				titlechanged = true;
			}
			else if (fastcmp(word, "USE1UPSOUND"))
			{
				use1upSound = (UINT8)(value || word2[0] == 'T' || word2[0] == 'Y');
			}
			else if (fastcmp(word, "MAXXTRALIFE"))
			{
				maxXtraLife = (UINT8)get_number(word2);
			}
			else if (fastcmp(word, "USECONTINUES"))
			{
				useContinues = (UINT8)(value || word2[0] == 'T' || word2[0] == 'Y');
			}

			else if (fastcmp(word, "GAMEDATA"))
			{
				size_t filenamelen;

				// Check the data filename so that mods
				// can't write arbitrary files.
				if (!GoodDataFileName(word2))
					I_Error("Maincfg: bad data file name '%s'\n", word2);

				G_SaveGameData();
				strlcpy(gamedatafilename, word2, sizeof (gamedatafilename));
				strlwr(gamedatafilename);
				savemoddata = true;
				majormods = false;

				// Also save a time attack folder
				filenamelen = strlen(gamedatafilename)-4;  // Strip off the extension
				filenamelen = min(filenamelen, sizeof (timeattackfolder));
				strncpy(timeattackfolder, gamedatafilename, filenamelen);
				timeattackfolder[min(filenamelen, sizeof (timeattackfolder) - 1)] = '\0';

				strcpy(savegamename, timeattackfolder);
				strlcat(savegamename, "%u.ssg", sizeof(savegamename));
				// can't use sprintf since there is %u in savegamename
				strcatbf(savegamename, srb2home, PATHSEP);

				strcpy(liveeventbackup, va("live%s.bkp", timeattackfolder));
				strcatbf(liveeventbackup, srb2home, PATHSEP);

				refreshdirmenu |= REFRESHDIR_GAMEDATA;
				gamedataadded = true;
				titlechanged = true;
			}
			else if (fastcmp(word, "RESETDATA"))
			{
				P_ResetData(value);
				titlechanged = true;
			}
			else if (fastcmp(word, "CUSTOMVERSION"))
			{
				strlcpy(customversionstring, word2, sizeof (customversionstring));
				//titlechanged = true;
			}
			else if (fastcmp(word, "BOOTMAP"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				bootmap = (INT16)value;
				//titlechanged = true;
			}
			else if (fastcmp(word, "TUTORIALMAP"))
			{
				// Support using the actual map name,
				// i.e., Level AB, Level FZ, etc.

				// Convert to map number
				if (word2[0] >= 'A' && word2[0] <= 'Z')
					value = M_MapNumber(word2[0], word2[1]);
				else
					value = get_number(word2);

				tutorialmap = (INT16)value;
			}
			else
				deh_warning("Maincfg: unknown word '%s'", word);
		}
	} while (!myfeof(f));

	Z_Free(s);
}

static void readwipes(MYFILE *f)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char *word = s;
	char *pword = word;
	char *word2;
	char *tmp;
	INT32 value;
	INT32 wipeoffset;

	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;

			// First remove trailing newline, if there is one
			tmp = strchr(s, '\n');
			if (tmp)
				*tmp = '\0';

			tmp = strchr(s, '#');
			if (tmp)
				*tmp = '\0';
			if (s == tmp)
				continue; // Skip comment lines, but don't break.

			// Get the part before the " = "
			tmp = strchr(s, '=');
			if (tmp)
				*(tmp-1) = '\0';
			else
				break;
			strupr(word);

			// Now get the part after
			word2 = tmp += 2;
			value = atoi(word2); // used for numerical settings

			if (value < -1 || value > 99)
			{
				deh_warning("Wipes: bad value '%s'", word2);
				continue;
			}
			else if (value == -1)
				value = UINT8_MAX;

			// error catching
			wipeoffset = -1;

			if (fastncmp(word, "LEVEL_", 6))
			{
				pword = word + 6;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_level_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_level_final;
			}
			else if (fastncmp(word, "INTERMISSION_", 13))
			{
				pword = word + 13;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_intermission_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_intermission_final;
			}
			else if (fastncmp(word, "VOTING_", 7))
			{
				pword = word + 7;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_voting_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_voting_final;
			}
			else if (fastncmp(word, "CONTINUING_", 11))
			{
				pword = word + 11;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_continuing_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_continuing_final;
			}
			else if (fastncmp(word, "TITLESCREEN_", 12))
			{
				pword = word + 12;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_titlescreen_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_titlescreen_final;
			}
			else if (fastncmp(word, "TIMEATTACK_", 11))
			{
				pword = word + 11;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_timeattack_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_timeattack_final;
			}
			else if (fastncmp(word, "CREDITS_", 8))
			{
				pword = word + 8;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_credits_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_credits_final;
				else if (fastcmp(pword, "INTERMEDIATE"))
					wipeoffset = wipe_credits_intermediate;
			}
			else if (fastncmp(word, "EVALUATION_", 11))
			{
				pword = word + 11;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_evaluation_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_evaluation_final;
			}
			else if (fastncmp(word, "GAMEEND_", 8))
			{
				pword = word + 8;
				if (fastcmp(pword, "TOBLACK"))
					wipeoffset = wipe_gameend_toblack;
				else if (fastcmp(pword, "FINAL"))
					wipeoffset = wipe_gameend_final;
			}
			else if (fastncmp(word, "ENCORE_", 7))
			{
				pword = word + 7;
				if (fastcmp(pword, "TOINVERT"))
					wipeoffset = wipe_encore_toinvert;
				else if (fastcmp(pword, "TOWHITE"))
					wipeoffset = wipe_encore_towhite;
			}

			if (wipeoffset < 0)
			{
				deh_warning("Wipes: unknown word '%s'", word);
				continue;
			}

			if (value == UINT8_MAX
			 && (wipeoffset <= wipe_level_toblack || wipeoffset >= wipe_encore_toinvert))
			{
				 // Cannot disable non-toblack wipes
				 // (or the level toblack wipe, or the special encore wipe)
				deh_warning("Wipes: can't disable wipe of type '%s'", word);
				continue;
			}

			wipedefs[wipeoffset] = (UINT8)value;
		}
	} while (!myfeof(f));

	Z_Free(s);
}

// Used when you do something invalid like read a bad item number
// to prevent extra unnecessary errors
static void ignorelines(MYFILE *f)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	do
	{
		if (myfgets(s, MAXLINELEN, f))
		{
			if (s[0] == '\n')
				break;
		}
	} while (!myfeof(f));
	Z_Free(s);
}

static void DEH_LoadDehackedFile(MYFILE *f, boolean mainfile)
{
	char *s = Z_Malloc(MAXLINELEN, PU_STATIC, NULL);
	char textline[MAXLINELEN];
	char *word;
	char *word2;
	INT32 i;

	if (!deh_loaded)
		initfreeslots();

	deh_num_warning = 0;

	gamedataadded = titlechanged = introchanged = false;

	// it doesn't test the version of SRB2 and version of dehacked file
	dbg_line = -1; // start at -1 so the first line is 0.
	while (!myfeof(f))
	{
		char origpos[128];
		INT32 size = 0;
		char *traverse;

		myfgets(s, MAXLINELEN, f);
		memcpy(textline, s, MAXLINELEN);
		if (s[0] == '\n' || s[0] == '#')
			continue;

		traverse = s;

		while (traverse[0] != '\n')
		{
			traverse++;
			size++;
		}

		strncpy(origpos, s, size);
		origpos[size] = '\0';

		if (NULL != (word = strtok(s, " "))) {
			strupr(word);
			if (word[strlen(word)-1] == '\n')
				word[strlen(word)-1] = '\0';
		}
		if (word)
		{
			if (fastcmp(word, "FREESLOT"))
			{
				readfreeslots(f);
				// This is not a major mod.
				continue;
			}
			else if (fastcmp(word, "MAINCFG"))
			{
				G_SetGameModified(multiplayer, true);
				readmaincfg(f);
				continue;
			}
			else if (fastcmp(word, "WIPES"))
			{
				readwipes(f);
				continue;
			}
			else if (fastcmp(word, "FOLLOWER"))
			{
				readfollower(f);	// at the same time this will be our only way to ADD followers for now. Yikes.
				// This is not a major mod either.
				continue;	// continue so that we don't error.
			}
			word2 = strtok(NULL, " ");
			if (word2) {
				strupr(word2);
				if (word2[strlen(word2) - 1] == '\n')
					word2[strlen(word2) - 1] = '\0';
				i = atoi(word2);
			}
			else
				i = 0;
			if (fastcmp(word, "CHARACTER"))
			{
				if (i >= 0 && i < 32)
					readPlayer(f, i);
				else
				{
					deh_warning("Character %d out of range (0 - 31)", i);
					ignorelines(f);
				}
				continue;
			}
			else if (fastcmp(word, "EMBLEM"))
			{
				if (!mainfile && !gamedataadded)
				{
					deh_warning("You must define a custom gamedata to use \"%s\"", word);
					ignorelines(f);
				}
				else
				{
					if (!word2)
						i = numemblems + 1;

					if (i > 0 && i <= MAXEMBLEMS)
					{
						if (numemblems < i)
							numemblems = i;
						reademblemdata(f, i);
					}
					else
					{
						deh_warning("Emblem number %d out of range (1 - %d)", i, MAXEMBLEMS);
						ignorelines(f);
					}
				}
				continue;
			}
			else if (fastcmp(word, "EXTRAEMBLEM"))
			{
				if (!mainfile && !gamedataadded)
				{
					deh_warning("You must define a custom gamedata to use \"%s\"", word);
					ignorelines(f);
				}
				else
				{
					if (!word2)
						i = numextraemblems + 1;

					if (i > 0 && i <= MAXEXTRAEMBLEMS)
					{
						if (numextraemblems < i)
							numextraemblems = i;
						readextraemblemdata(f, i);
					}
					else
					{
						deh_warning("Extra emblem number %d out of range (1 - %d)", i, MAXEXTRAEMBLEMS);
						ignorelines(f);
					}
				}
				continue;
			}
			if (word2)
			{
				if (fastcmp(word, "THING") || fastcmp(word, "MOBJ") || fastcmp(word, "OBJECT"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_mobjtype(word2); // find a thing by name
					if (i < NUMMOBJTYPES && i > 0)
						readthing(f, i);
					else
					{
						deh_warning("Thing %d out of range (1 - %d)", i, NUMMOBJTYPES-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "SKINCOLOR") || fastcmp(word, "COLOR"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_skincolor(word2); // find a skincolor by name
					if (i && i < numskincolors)
						readskincolor(f, i);
					else
					{
						deh_warning("Skincolor %d out of range (1 - %d)", i, numskincolors-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "SPRITE2"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_sprite2(word2); // find a sprite by name
					if (i < (INT32)free_spr2 && i >= (INT32)SPR2_FIRSTFREESLOT)
						readsprite2(f, i);
					else
					{
						deh_warning("Sprite2 number %d out of range (%d - %d)", i, SPR2_FIRSTFREESLOT, free_spr2-1);
						ignorelines(f);
					}
				}
#ifdef HWRENDER
				else if (fastcmp(word, "LIGHT"))
				{
					// TODO: Read lights by name
					if (i > 0 && i < NUMLIGHTS)
						readlight(f, i);
					else
					{
						deh_warning("Light number %d out of range (1 - %d)", i, NUMLIGHTS-1);
						ignorelines(f);
					}
				}
#endif
				else if (fastcmp(word, "SPRITE") || fastcmp(word, "SPRITEINFO"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_sprite(word2); // find a sprite by name
					if (i < NUMSPRITES && i > 0)
						readspriteinfo(f, i, false);
					else
					{
						deh_warning("Sprite number %d out of range (0 - %d)", i, NUMSPRITES-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "SPRITE2INFO"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_sprite2(word2); // find a sprite by name
					if (i < NUMPLAYERSPRITES && i >= 0)
						readspriteinfo(f, i, true);
					else
					{
						deh_warning("Sprite2 number %d out of range (0 - %d)", i, NUMPLAYERSPRITES-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "LEVEL"))
				{
					// Support using the actual map name,
					// i.e., Level AB, Level FZ, etc.

					// Convert to map number
					if (word2[0] >= 'A' && word2[0] <= 'Z')
						i = M_MapNumber(word2[0], word2[1]);

					if (i > 0 && i <= NUMMAPS)
					{
						if (mapheaderinfo[i])
							G_SetGameModified(multiplayer, true); // only mark as a major mod if it replaces an already-existing mapheaderinfo
						readlevelheader(f, i);
					}
					else
					{
						deh_warning("Level number %d out of range (1 - %d)", i, NUMMAPS);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "GAMETYPE"))
				{
					// Get the gametype name from textline
					// instead of word2, so that gametype names
					// aren't allcaps
					INT32 c;
					for (c = 0; c < MAXLINELEN; c++)
					{
						if (textline[c] == '\0')
							break;
						if (textline[c] == ' ')
						{
							char *gtname = (textline+c+1);
							if (gtname)
							{
								// remove funny characters
								INT32 j;
								for (j = 0; j < (MAXLINELEN - c); j++)
								{
									if (gtname[j] == '\0')
										break;
									if (gtname[j] < 32)
										gtname[j] = '\0';
								}
								readgametype(f, gtname);
							}
							break;
						}
					}
				}
				else if (fastcmp(word, "CUP"))
				{
					cupheader_t *cup = kartcupheaders;
					cupheader_t *prev = NULL;

					while (cup)
					{
						if (fastcmp(cup->name, word2))
						{
							// mark as a major mod if it replaces an already-existing cup
							G_SetGameModified(multiplayer, true);
							break;
						}

						prev = cup;
						cup = cup->next;
					}

					// Nothing found, add to the end.
					if (!cup)
					{
						cup = Z_Calloc(sizeof (cupheader_t), PU_STATIC, NULL);
						cup->id = numkartcupheaders;
						deh_strlcpy(cup->name, word2,
							sizeof(cup->name), va("Cup header %s: name", word2));
						if (prev != NULL)
							prev->next = cup;
						if (kartcupheaders == NULL)
							kartcupheaders = cup;
						numkartcupheaders++;
						CONS_Printf("Added cup %d ('%s')\n", cup->id, cup->name);
					}

					readcupheader(f, cup);
				}
				else if (fastcmp(word, "CUTSCENE"))
				{
					if (i > 0 && i < 129)
						readcutscene(f, i - 1);
					else
					{
						deh_warning("Cutscene number %d out of range (1 - 128)", i);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "PROMPT"))
				{
					if (i > 0 && i < MAX_PROMPTS)
						readtextprompt(f, i - 1);
					else
					{
						deh_warning("Prompt number %d out of range (1 - %d)", i, MAX_PROMPTS);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "FRAME") || fastcmp(word, "STATE"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_state(word2); // find a state by name
					if (i < NUMSTATES && i >= 0)
					{
						if (i < (S_FIRSTFREESLOT+freeslotusage[0][1]))
							G_SetGameModified(multiplayer, true); // affecting something earlier than the first freeslot allocated in this .wad? DENIED
						readframe(f, i);
					}
					else
					{
						deh_warning("Frame %d out of range (0 - %d)", i, NUMSTATES-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "SOUND"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_sfx(word2); // find a sound by name
					if (i < NUMSFX && i > 0)
						readsound(f, i);
					else
					{
						deh_warning("Sound %d out of range (1 - %d)", i, NUMSFX-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "HUDITEM"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_huditem(word2); // find a huditem by name
					if (i >= 0 && i < NUMHUDITEMS)
						readhuditem(f, i);
					else
					{
						deh_warning("HUD item number %d out of range (0 - %d)", i, NUMHUDITEMS-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "MENU"))
				{
					if (i == 0 && word2[0] != '0') // If word2 isn't a number
						i = get_menutype(word2); // find a huditem by name
					if (i >= 1 && i < NUMMENUTYPES)
						readmenu(f, i);
					else
					{
						// zero-based, but let's start at 1
						deh_warning("Menu number %d out of range (1 - %d)", i, NUMMENUTYPES-1);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "UNLOCKABLE"))
				{
					if (!mainfile && !gamedataadded)
					{
						deh_warning("You must define a custom gamedata to use \"%s\"", word);
						ignorelines(f);
					}
					else if (i > 0 && i <= MAXUNLOCKABLES)
						readunlockable(f, i - 1);
					else
					{
						deh_warning("Unlockable number %d out of range (1 - %d)", i, MAXUNLOCKABLES);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "CONDITIONSET"))
				{
					if (!mainfile && !gamedataadded)
					{
						deh_warning("You must define a custom gamedata to use \"%s\"", word);
						ignorelines(f);
					}
					else if (i > 0 && i <= MAXCONDITIONSETS)
						readconditionset(f, (UINT8)i);
					else
					{
						deh_warning("Condition set number %d out of range (1 - %d)", i, MAXCONDITIONSETS);
						ignorelines(f);
					}
				}
				else if (fastcmp(word, "SRB2KART"))
				{
					if (isdigit(word2[0]))
					{
						i = atoi(word2);
						if (i != PATCHVERSION)
						{
							deh_warning(
									"Patch is for SRB2Kart version %d, "
									"only version %d is supported",
									i,
									PATCHVERSION
							);
						}
					}
					else
					{
						deh_warning(
								"SRB2Kart version definition has incorrect format, "
								"use \"SRB2KART %d\"",
								PATCHVERSION
						);
					}
				}
				else if (fastcmp(word, "SRB2"))
				{
					deh_warning("Patch is only compatible with base SRB2.");
				}
				// Clear all data in certain locations (mostly for unlocks)
				// Unless you REALLY want to piss people off,
				// define a custom gamedata /before/ doing this!!
				// (then again, modifiedgame will prevent game data saving anyway)
				else if (fastcmp(word, "CLEAR"))
				{
					boolean clearall = (fastcmp(word2, "ALL"));

					if (!mainfile && !gamedataadded)
					{
						deh_warning("You must define a custom gamedata to use \"%s\"", word);
						continue;
					}

					if (clearall || fastcmp(word2, "UNLOCKABLES"))
						memset(&unlockables, 0, sizeof(unlockables));

					if (clearall || fastcmp(word2, "EMBLEMS"))
					{
						memset(&emblemlocations, 0, sizeof(emblemlocations));
						numemblems = 0;
					}

					if (clearall || fastcmp(word2, "EXTRAEMBLEMS"))
					{
						memset(&extraemblems, 0, sizeof(extraemblems));
						numextraemblems = 0;
					}

					if (clearall || fastcmp(word2, "CONDITIONSETS"))
						clear_conditionsets();

					if (clearall || fastcmp(word2, "LEVELS"))
						clear_levels();
				}
				else
					deh_warning("Unknown word: %s", word);
			}
			else
				deh_warning("missing argument for '%s'", word);
		}
		else
			deh_warning("No word in this line: %s", s);
	} // end while

	/* REFRESHDIR_GAMEDATA murdered this
	if (gamedataadded)
		G_LoadGameData();
	*/

	if (gamestate == GS_TITLESCREEN)
	{
		if (introchanged)
		{
			menuactive = false;
			I_UpdateMouseGrab();
			COM_BufAddText("playintro");
		}
		else if (titlechanged)
		{
			menuactive = false;
			I_UpdateMouseGrab();
			COM_BufAddText("exitgame"); // Command_ExitGame_f() but delayed
		}
	}

	dbg_line = -1;
	if (deh_num_warning)
	{
		CONS_Printf(M_GetText("%d warning%s in the SOC lump\n"), deh_num_warning, deh_num_warning == 1 ? "" : "s");
		if (devparm) {
			I_Error("%s%s",va(M_GetText("%d warning%s in the SOC lump\n"), deh_num_warning, deh_num_warning == 1 ? "" : "s"), M_GetText("See log.txt for details.\n"));
			//while (!I_GetKey())
				//I_OsPolling();
		}
	}

	deh_loaded = true;
	Z_Free(s);
}

// read dehacked lump in a wad (there is special trick for for deh
// file that are converted to wad in w_wad.c)
void DEH_LoadDehackedLumpPwad(UINT16 wad, UINT16 lump, boolean mainfile)
{
	MYFILE f;
	f.wad = wad;
	f.size = W_LumpLengthPwad(wad, lump);
	f.data = Z_Malloc(f.size + 1, PU_STATIC, NULL);
	W_ReadLumpPwad(wad, lump, f.data);
	f.curpos = f.data;
	f.data[f.size] = 0;
	DEH_LoadDehackedFile(&f, mainfile);
	Z_Free(f.data);
}

void DEH_LoadDehackedLump(lumpnum_t lumpnum)
{
	DEH_LoadDehackedLumpPwad(WADFILENUM(lumpnum),LUMPNUM(lumpnum), false);
}

////////////////////////////////////////////////////////////////////////////////
// CRAZY LIST OF STATE NAMES AND ALL FROM HERE DOWN
// TODO: Make this all a seperate file or something, like part of info.c??
// TODO: Read the list from a text lump in a WAD as necessary instead
// or something, don't just keep it all in memory like this.
// TODO: Make the lists public so we can start using actual mobj
// and state names in warning and error messages! :D

// RegEx to generate this from info.h: ^\tS_([^,]+), --> \t"S_\1",
// I am leaving the prefixes solely for clarity to programmers,
// because sadly no one remembers this place while searching for full state names.
static const char *const STATE_LIST[] = { // array length left dynamic for sanity testing later.
	"S_NULL",
	"S_UNKNOWN",
	"S_INVISIBLE", // state for invisible sprite

	"S_SPAWNSTATE",
	"S_SEESTATE",
	"S_MELEESTATE",
	"S_MISSILESTATE",
	"S_DEATHSTATE",
	"S_XDEATHSTATE",
	"S_RAISESTATE",

	// Thok
	"S_THOK",

	// SRB2kart Frames
	"S_KART_STILL",
	"S_KART_STILL_L",
	"S_KART_STILL_R",
	"S_KART_SLOW",
	"S_KART_SLOW_L",
	"S_KART_SLOW_R",
	"S_KART_FAST",
	"S_KART_FAST_L",
	"S_KART_FAST_R",
	"S_KART_DRIFT_L",
	"S_KART_DRIFT_L_OUT",
	"S_KART_DRIFT_L_IN",
	"S_KART_DRIFT_R",
	"S_KART_DRIFT_R_OUT",
	"S_KART_DRIFT_R_IN",
	"S_KART_SPINOUT",
	"S_KART_DEAD",
	"S_KART_SIGN",

	// technically the player goes here but it's an infinite tic state
	"S_OBJPLACE_DUMMY",

	// Blue Crawla
	"S_POSS_STND",
	"S_POSS_RUN1",
	"S_POSS_RUN2",
	"S_POSS_RUN3",
	"S_POSS_RUN4",
	"S_POSS_RUN5",
	"S_POSS_RUN6",

	// Red Crawla
	"S_SPOS_STND",
	"S_SPOS_RUN1",
	"S_SPOS_RUN2",
	"S_SPOS_RUN3",
	"S_SPOS_RUN4",
	"S_SPOS_RUN5",
	"S_SPOS_RUN6",

	// Greenflower Fish
	"S_FISH1",
	"S_FISH2",
	"S_FISH3",
	"S_FISH4",

	// Buzz (Gold)
	"S_BUZZLOOK1",
	"S_BUZZLOOK2",
	"S_BUZZFLY1",
	"S_BUZZFLY2",

	// Buzz (Red)
	"S_RBUZZLOOK1",
	"S_RBUZZLOOK2",
	"S_RBUZZFLY1",
	"S_RBUZZFLY2",

	// Jetty-Syn Bomber
	"S_JETBLOOK1",
	"S_JETBLOOK2",
	"S_JETBZOOM1",
	"S_JETBZOOM2",

	// Jetty-Syn Gunner
	"S_JETGLOOK1",
	"S_JETGLOOK2",
	"S_JETGZOOM1",
	"S_JETGZOOM2",
	"S_JETGSHOOT1",
	"S_JETGSHOOT2",

	// Crawla Commander
	"S_CCOMMAND1",
	"S_CCOMMAND2",
	"S_CCOMMAND3",
	"S_CCOMMAND4",

	// Deton
	"S_DETON1",
	"S_DETON2",
	"S_DETON3",
	"S_DETON4",
	"S_DETON5",
	"S_DETON6",
	"S_DETON7",
	"S_DETON8",
	"S_DETON9",
	"S_DETON10",
	"S_DETON11",
	"S_DETON12",
	"S_DETON13",
	"S_DETON14",
	"S_DETON15",

	// Skim Mine Dropper
	"S_SKIM1",
	"S_SKIM2",
	"S_SKIM3",
	"S_SKIM4",

	// THZ Turret
	"S_TURRET",
	"S_TURRETFIRE",
	"S_TURRETSHOCK1",
	"S_TURRETSHOCK2",
	"S_TURRETSHOCK3",
	"S_TURRETSHOCK4",
	"S_TURRETSHOCK5",
	"S_TURRETSHOCK6",
	"S_TURRETSHOCK7",
	"S_TURRETSHOCK8",
	"S_TURRETSHOCK9",

	// Popup Turret
	"S_TURRETLOOK",
	"S_TURRETSEE",
	"S_TURRETPOPUP1",
	"S_TURRETPOPUP2",
	"S_TURRETPOPUP3",
	"S_TURRETPOPUP4",
	"S_TURRETPOPUP5",
	"S_TURRETPOPUP6",
	"S_TURRETPOPUP7",
	"S_TURRETPOPUP8",
	"S_TURRETSHOOT",
	"S_TURRETPOPDOWN1",
	"S_TURRETPOPDOWN2",
	"S_TURRETPOPDOWN3",
	"S_TURRETPOPDOWN4",
	"S_TURRETPOPDOWN5",
	"S_TURRETPOPDOWN6",
	"S_TURRETPOPDOWN7",
	"S_TURRETPOPDOWN8",

	// Spincushion
	"S_SPINCUSHION_LOOK",
	"S_SPINCUSHION_CHASE1",
	"S_SPINCUSHION_CHASE2",
	"S_SPINCUSHION_CHASE3",
	"S_SPINCUSHION_CHASE4",
	"S_SPINCUSHION_AIM1",
	"S_SPINCUSHION_AIM2",
	"S_SPINCUSHION_AIM3",
	"S_SPINCUSHION_AIM4",
	"S_SPINCUSHION_AIM5",
	"S_SPINCUSHION_SPIN1",
	"S_SPINCUSHION_SPIN2",
	"S_SPINCUSHION_SPIN3",
	"S_SPINCUSHION_SPIN4",
	"S_SPINCUSHION_STOP1",
	"S_SPINCUSHION_STOP2",
	"S_SPINCUSHION_STOP3",
	"S_SPINCUSHION_STOP4",

	// Crushstacean
	"S_CRUSHSTACEAN_ROAM1",
	"S_CRUSHSTACEAN_ROAM2",
	"S_CRUSHSTACEAN_ROAM3",
	"S_CRUSHSTACEAN_ROAM4",
	"S_CRUSHSTACEAN_ROAMPAUSE",
	"S_CRUSHSTACEAN_PUNCH1",
	"S_CRUSHSTACEAN_PUNCH2",
	"S_CRUSHCLAW_AIM",
	"S_CRUSHCLAW_OUT",
	"S_CRUSHCLAW_STAY",
	"S_CRUSHCLAW_IN",
	"S_CRUSHCLAW_WAIT",
	"S_CRUSHCHAIN",

	// Banpyura
	"S_BANPYURA_ROAM1",
	"S_BANPYURA_ROAM2",
	"S_BANPYURA_ROAM3",
	"S_BANPYURA_ROAM4",
	"S_BANPYURA_ROAMPAUSE",
	"S_CDIAG1",
	"S_CDIAG2",
	"S_CDIAG3",
	"S_CDIAG4",
	"S_CDIAG5",
	"S_CDIAG6",
	"S_CDIAG7",
	"S_CDIAG8",

	// Jet Jaw
	"S_JETJAW_ROAM1",
	"S_JETJAW_ROAM2",
	"S_JETJAW_ROAM3",
	"S_JETJAW_ROAM4",
	"S_JETJAW_ROAM5",
	"S_JETJAW_ROAM6",
	"S_JETJAW_ROAM7",
	"S_JETJAW_ROAM8",
	"S_JETJAW_CHOMP1",
	"S_JETJAW_CHOMP2",
	"S_JETJAW_CHOMP3",
	"S_JETJAW_CHOMP4",
	"S_JETJAW_CHOMP5",
	"S_JETJAW_CHOMP6",
	"S_JETJAW_CHOMP7",
	"S_JETJAW_CHOMP8",
	"S_JETJAW_CHOMP9",
	"S_JETJAW_CHOMP10",
	"S_JETJAW_CHOMP11",
	"S_JETJAW_CHOMP12",
	"S_JETJAW_CHOMP13",
	"S_JETJAW_CHOMP14",
	"S_JETJAW_CHOMP15",
	"S_JETJAW_CHOMP16",
	"S_JETJAW_SOUND",

	// Snailer
	"S_SNAILER1",
	"S_SNAILER_FLICKY",

	// Vulture
	"S_VULTURE_STND",
	"S_VULTURE_DRIFT",
	"S_VULTURE_ZOOM1",
	"S_VULTURE_ZOOM2",
	"S_VULTURE_STUNNED",

	// Pointy
	"S_POINTY1",
	"S_POINTYBALL1",

	// Robo-Hood
	"S_ROBOHOOD_LOOK",
	"S_ROBOHOOD_STAND",
	"S_ROBOHOOD_FIRE1",
	"S_ROBOHOOD_FIRE2",
	"S_ROBOHOOD_JUMP1",
	"S_ROBOHOOD_JUMP2",
	"S_ROBOHOOD_JUMP3",

	// Castlebot Facestabber
	"S_FACESTABBER_STND1",
	"S_FACESTABBER_STND2",
	"S_FACESTABBER_STND3",
	"S_FACESTABBER_STND4",
	"S_FACESTABBER_STND5",
	"S_FACESTABBER_STND6",
	"S_FACESTABBER_CHARGE1",
	"S_FACESTABBER_CHARGE2",
	"S_FACESTABBER_CHARGE3",
	"S_FACESTABBER_CHARGE4",
	"S_FACESTABBER_PAIN",
	"S_FACESTABBER_DIE1",
	"S_FACESTABBER_DIE2",
	"S_FACESTABBER_DIE3",
	"S_FACESTABBERSPEAR",

	// Egg Guard
	"S_EGGGUARD_STND",
	"S_EGGGUARD_WALK1",
	"S_EGGGUARD_WALK2",
	"S_EGGGUARD_WALK3",
	"S_EGGGUARD_WALK4",
	"S_EGGGUARD_MAD1",
	"S_EGGGUARD_MAD2",
	"S_EGGGUARD_MAD3",
	"S_EGGGUARD_RUN1",
	"S_EGGGUARD_RUN2",
	"S_EGGGUARD_RUN3",
	"S_EGGGUARD_RUN4",

	// Egg Shield for Egg Guard
	"S_EGGSHIELD",
	"S_EGGSHIELDBREAK",

	// Green Snapper
	"S_SNAPPER_SPAWN",
	"S_SNAPPER_SPAWN2",
	"S_GSNAPPER_STND",
	"S_GSNAPPER1",
	"S_GSNAPPER2",
	"S_GSNAPPER3",
	"S_GSNAPPER4",
	"S_SNAPPER_XPLD",
	"S_SNAPPER_LEG",
	"S_SNAPPER_LEGRAISE",
	"S_SNAPPER_HEAD",

	// Minus
	"S_MINUS_INIT",
	"S_MINUS_STND",
	"S_MINUS_DIGGING1",
	"S_MINUS_DIGGING2",
	"S_MINUS_DIGGING3",
	"S_MINUS_DIGGING4",
	"S_MINUS_BURST0",
	"S_MINUS_BURST1",
	"S_MINUS_BURST2",
	"S_MINUS_BURST3",
	"S_MINUS_BURST4",
	"S_MINUS_BURST5",
	"S_MINUS_POPUP",
	"S_MINUS_AERIAL1",
	"S_MINUS_AERIAL2",
	"S_MINUS_AERIAL3",
	"S_MINUS_AERIAL4",

	// Minus dirt
	"S_MINUSDIRT1",
	"S_MINUSDIRT2",
	"S_MINUSDIRT3",
	"S_MINUSDIRT4",
	"S_MINUSDIRT5",
	"S_MINUSDIRT6",
	"S_MINUSDIRT7",

	// Spring Shell
	"S_SSHELL_STND",
	"S_SSHELL_RUN1",
	"S_SSHELL_RUN2",
	"S_SSHELL_RUN3",
	"S_SSHELL_RUN4",
	"S_SSHELL_SPRING1",
	"S_SSHELL_SPRING2",
	"S_SSHELL_SPRING3",
	"S_SSHELL_SPRING4",

	// Spring Shell (yellow)
	"S_YSHELL_STND",
	"S_YSHELL_RUN1",
	"S_YSHELL_RUN2",
	"S_YSHELL_RUN3",
	"S_YSHELL_RUN4",
	"S_YSHELL_SPRING1",
	"S_YSHELL_SPRING2",
	"S_YSHELL_SPRING3",
	"S_YSHELL_SPRING4",

	// Unidus
	"S_UNIDUS_STND",
	"S_UNIDUS_RUN",
	"S_UNIDUS_BALL",

	// Canarivore
	"S_CANARIVORE_LOOK",
	"S_CANARIVORE_AWAKEN1",
	"S_CANARIVORE_AWAKEN2",
	"S_CANARIVORE_AWAKEN3",
	"S_CANARIVORE_GAS1",
	"S_CANARIVORE_GAS2",
	"S_CANARIVORE_GAS3",
	"S_CANARIVORE_GAS4",
	"S_CANARIVORE_GAS5",
	"S_CANARIVORE_GASREPEAT",
	"S_CANARIVORE_CLOSE1",
	"S_CANARIVORE_CLOSE2",
	"S_CANARIVOREGAS_1",
	"S_CANARIVOREGAS_2",
	"S_CANARIVOREGAS_3",
	"S_CANARIVOREGAS_4",
	"S_CANARIVOREGAS_5",
	"S_CANARIVOREGAS_6",
	"S_CANARIVOREGAS_7",
	"S_CANARIVOREGAS_8",

	// Pyre Fly
	"S_PYREFLY_FLY",
	"S_PYREFLY_BURN",
	"S_PYREFIRE1",
	"S_PYREFIRE2",

	// Pterabyte
	"S_PTERABYTESPAWNER",
	"S_PTERABYTEWAYPOINT",
	"S_PTERABYTE_FLY1",
	"S_PTERABYTE_FLY2",
	"S_PTERABYTE_FLY3",
	"S_PTERABYTE_FLY4",
	"S_PTERABYTE_SWOOPDOWN",
	"S_PTERABYTE_SWOOPUP",

	// Dragonbomber
	"S_DRAGONBOMBER",
	"S_DRAGONWING1",
	"S_DRAGONWING2",
	"S_DRAGONWING3",
	"S_DRAGONWING4",
	"S_DRAGONTAIL_LOADED",
	"S_DRAGONTAIL_EMPTY",
	"S_DRAGONTAIL_EMPTYLOOP",
	"S_DRAGONTAIL_RELOAD",
	"S_DRAGONMINE",
	"S_DRAGONMINE_LAND1",
	"S_DRAGONMINE_LAND2",
	"S_DRAGONMINE_SLOWFLASH1",
	"S_DRAGONMINE_SLOWFLASH2",
	"S_DRAGONMINE_SLOWLOOP",
	"S_DRAGONMINE_FASTFLASH1",
	"S_DRAGONMINE_FASTFLASH2",
	"S_DRAGONMINE_FASTLOOP",

	// Boss Explosion
	"S_BOSSEXPLODE",

	// S3&K Boss Explosion
	"S_SONIC3KBOSSEXPLOSION1",
	"S_SONIC3KBOSSEXPLOSION2",
	"S_SONIC3KBOSSEXPLOSION3",
	"S_SONIC3KBOSSEXPLOSION4",
	"S_SONIC3KBOSSEXPLOSION5",
	"S_SONIC3KBOSSEXPLOSION6",

	"S_JETFUME1",

	// Boss 1
	"S_EGGMOBILE_STND",
	"S_EGGMOBILE_ROFL",
	"S_EGGMOBILE_LATK1",
	"S_EGGMOBILE_LATK2",
	"S_EGGMOBILE_LATK3",
	"S_EGGMOBILE_LATK4",
	"S_EGGMOBILE_LATK5",
	"S_EGGMOBILE_LATK6",
	"S_EGGMOBILE_LATK7",
	"S_EGGMOBILE_LATK8",
	"S_EGGMOBILE_LATK9",
	"S_EGGMOBILE_RATK1",
	"S_EGGMOBILE_RATK2",
	"S_EGGMOBILE_RATK3",
	"S_EGGMOBILE_RATK4",
	"S_EGGMOBILE_RATK5",
	"S_EGGMOBILE_RATK6",
	"S_EGGMOBILE_RATK7",
	"S_EGGMOBILE_RATK8",
	"S_EGGMOBILE_RATK9",
	"S_EGGMOBILE_PANIC1",
	"S_EGGMOBILE_PANIC2",
	"S_EGGMOBILE_PANIC3",
	"S_EGGMOBILE_PANIC4",
	"S_EGGMOBILE_PANIC5",
	"S_EGGMOBILE_PANIC6",
	"S_EGGMOBILE_PANIC7",
	"S_EGGMOBILE_PANIC8",
	"S_EGGMOBILE_PANIC9",
	"S_EGGMOBILE_PANIC10",
	"S_EGGMOBILE_PANIC11",
	"S_EGGMOBILE_PANIC12",
	"S_EGGMOBILE_PANIC13",
	"S_EGGMOBILE_PANIC14",
	"S_EGGMOBILE_PANIC15",
	"S_EGGMOBILE_PAIN",
	"S_EGGMOBILE_PAIN2",
	"S_EGGMOBILE_DIE1",
	"S_EGGMOBILE_DIE2",
	"S_EGGMOBILE_DIE3",
	"S_EGGMOBILE_DIE4",
	"S_EGGMOBILE_FLEE1",
	"S_EGGMOBILE_FLEE2",
	"S_EGGMOBILE_BALL",
	"S_EGGMOBILE_TARGET",

	"S_BOSSEGLZ1",
	"S_BOSSEGLZ2",

	// Boss 2
	"S_EGGMOBILE2_STND",
	"S_EGGMOBILE2_POGO1",
	"S_EGGMOBILE2_POGO2",
	"S_EGGMOBILE2_POGO3",
	"S_EGGMOBILE2_POGO4",
	"S_EGGMOBILE2_POGO5",
	"S_EGGMOBILE2_POGO6",
	"S_EGGMOBILE2_POGO7",
	"S_EGGMOBILE2_PAIN",
	"S_EGGMOBILE2_PAIN2",
	"S_EGGMOBILE2_DIE1",
	"S_EGGMOBILE2_DIE2",
	"S_EGGMOBILE2_DIE3",
	"S_EGGMOBILE2_DIE4",
	"S_EGGMOBILE2_FLEE1",
	"S_EGGMOBILE2_FLEE2",

	"S_BOSSTANK1",
	"S_BOSSTANK2",
	"S_BOSSSPIGOT",

	// Boss 2 Goop
	"S_GOOP1",
	"S_GOOP2",
	"S_GOOP3",
	"S_GOOPTRAIL",

	// Boss 3
	"S_EGGMOBILE3_STND",
	"S_EGGMOBILE3_SHOCK",
	"S_EGGMOBILE3_ATK1",
	"S_EGGMOBILE3_ATK2",
	"S_EGGMOBILE3_ATK3A",
	"S_EGGMOBILE3_ATK3B",
	"S_EGGMOBILE3_ATK3C",
	"S_EGGMOBILE3_ATK3D",
	"S_EGGMOBILE3_ATK4",
	"S_EGGMOBILE3_ATK5",
	"S_EGGMOBILE3_ROFL",
	"S_EGGMOBILE3_PAIN",
	"S_EGGMOBILE3_PAIN2",
	"S_EGGMOBILE3_DIE1",
	"S_EGGMOBILE3_DIE2",
	"S_EGGMOBILE3_DIE3",
	"S_EGGMOBILE3_DIE4",
	"S_EGGMOBILE3_FLEE1",
	"S_EGGMOBILE3_FLEE2",

	// Boss 3 Pinch
	"S_FAKEMOBILE_INIT",
	"S_FAKEMOBILE",
	"S_FAKEMOBILE_ATK1",
	"S_FAKEMOBILE_ATK2",
	"S_FAKEMOBILE_ATK3A",
	"S_FAKEMOBILE_ATK3B",
	"S_FAKEMOBILE_ATK3C",
	"S_FAKEMOBILE_ATK3D",
	"S_FAKEMOBILE_DIE1",
	"S_FAKEMOBILE_DIE2",

	"S_BOSSSEBH1",
	"S_BOSSSEBH2",

	// Boss 3 Shockwave
	"S_SHOCKWAVE1",
	"S_SHOCKWAVE2",

	// Boss 4
	"S_EGGMOBILE4_STND",
	"S_EGGMOBILE4_LATK1",
	"S_EGGMOBILE4_LATK2",
	"S_EGGMOBILE4_LATK3",
	"S_EGGMOBILE4_LATK4",
	"S_EGGMOBILE4_LATK5",
	"S_EGGMOBILE4_LATK6",
	"S_EGGMOBILE4_RATK1",
	"S_EGGMOBILE4_RATK2",
	"S_EGGMOBILE4_RATK3",
	"S_EGGMOBILE4_RATK4",
	"S_EGGMOBILE4_RATK5",
	"S_EGGMOBILE4_RATK6",
	"S_EGGMOBILE4_RAISE1",
	"S_EGGMOBILE4_RAISE2",
	"S_EGGMOBILE4_PAIN1",
	"S_EGGMOBILE4_PAIN2",
	"S_EGGMOBILE4_DIE1",
	"S_EGGMOBILE4_DIE2",
	"S_EGGMOBILE4_DIE3",
	"S_EGGMOBILE4_DIE4",
	"S_EGGMOBILE4_FLEE1",
	"S_EGGMOBILE4_FLEE2",
	"S_EGGMOBILE4_MACE",
	"S_EGGMOBILE4_MACE_DIE1",
	"S_EGGMOBILE4_MACE_DIE2",
	"S_EGGMOBILE4_MACE_DIE3",

	// Boss 4 jet flame
	"S_JETFLAME",

	// Boss 4 Spectator Eggrobo
	"S_EGGROBO1_STND",
	"S_EGGROBO1_BSLAP1",
	"S_EGGROBO1_BSLAP2",
	"S_EGGROBO1_PISSED",

	// Boss 4 Spectator Eggrobo jet flame
	"S_EGGROBOJET",

	// Boss 5
	"S_FANG_SETUP",
	"S_FANG_INTRO0",
	"S_FANG_INTRO1",
	"S_FANG_INTRO2",
	"S_FANG_INTRO3",
	"S_FANG_INTRO4",
	"S_FANG_INTRO5",
	"S_FANG_INTRO6",
	"S_FANG_INTRO7",
	"S_FANG_INTRO8",
	"S_FANG_INTRO9",
	"S_FANG_INTRO10",
	"S_FANG_INTRO11",
	"S_FANG_INTRO12",
	"S_FANG_CLONE1",
	"S_FANG_CLONE2",
	"S_FANG_CLONE3",
	"S_FANG_CLONE4",
	"S_FANG_IDLE0",
	"S_FANG_IDLE1",
	"S_FANG_IDLE2",
	"S_FANG_IDLE3",
	"S_FANG_IDLE4",
	"S_FANG_IDLE5",
	"S_FANG_IDLE6",
	"S_FANG_IDLE7",
	"S_FANG_IDLE8",
	"S_FANG_PAIN1",
	"S_FANG_PAIN2",
	"S_FANG_PATHINGSTART1",
	"S_FANG_PATHINGSTART2",
	"S_FANG_PATHING",
	"S_FANG_BOUNCE1",
	"S_FANG_BOUNCE2",
	"S_FANG_BOUNCE3",
	"S_FANG_BOUNCE4",
	"S_FANG_FALL1",
	"S_FANG_FALL2",
	"S_FANG_CHECKPATH1",
	"S_FANG_CHECKPATH2",
	"S_FANG_PATHINGCONT1",
	"S_FANG_PATHINGCONT2",
	"S_FANG_PATHINGCONT3",
	"S_FANG_SKID1",
	"S_FANG_SKID2",
	"S_FANG_SKID3",
	"S_FANG_CHOOSEATTACK",
	"S_FANG_FIRESTART1",
	"S_FANG_FIRESTART2",
	"S_FANG_FIRE1",
	"S_FANG_FIRE2",
	"S_FANG_FIRE3",
	"S_FANG_FIRE4",
	"S_FANG_FIREREPEAT",
	"S_FANG_LOBSHOT0",
	"S_FANG_LOBSHOT1",
	"S_FANG_LOBSHOT2",
	"S_FANG_WAIT1",
	"S_FANG_WAIT2",
	"S_FANG_WALLHIT",
	"S_FANG_PINCHPATHINGSTART1",
	"S_FANG_PINCHPATHINGSTART2",
	"S_FANG_PINCHPATHING",
	"S_FANG_PINCHBOUNCE0",
	"S_FANG_PINCHBOUNCE1",
	"S_FANG_PINCHBOUNCE2",
	"S_FANG_PINCHBOUNCE3",
	"S_FANG_PINCHBOUNCE4",
	"S_FANG_PINCHFALL0",
	"S_FANG_PINCHFALL1",
	"S_FANG_PINCHFALL2",
	"S_FANG_PINCHSKID1",
	"S_FANG_PINCHSKID2",
	"S_FANG_PINCHLOBSHOT0",
	"S_FANG_PINCHLOBSHOT1",
	"S_FANG_PINCHLOBSHOT2",
	"S_FANG_PINCHLOBSHOT3",
	"S_FANG_PINCHLOBSHOT4",
	"S_FANG_DIE1",
	"S_FANG_DIE2",
	"S_FANG_DIE3",
	"S_FANG_DIE4",
	"S_FANG_DIE5",
	"S_FANG_DIE6",
	"S_FANG_DIE7",
	"S_FANG_DIE8",
	"S_FANG_FLEEPATHING1",
	"S_FANG_FLEEPATHING2",
	"S_FANG_FLEEBOUNCE1",
	"S_FANG_FLEEBOUNCE2",
	"S_FANG_KO",

	"S_BROKENROBOTRANDOM",
	"S_BROKENROBOTA",
	"S_BROKENROBOTB",
	"S_BROKENROBOTC",
	"S_BROKENROBOTD",
	"S_BROKENROBOTE",
	"S_BROKENROBOTF",

	"S_ALART1",
	"S_ALART2",

	"S_VWREF",
	"S_VWREB",

	"S_PROJECTORLIGHT1",
	"S_PROJECTORLIGHT2",
	"S_PROJECTORLIGHT3",
	"S_PROJECTORLIGHT4",
	"S_PROJECTORLIGHT5",

	"S_FBOMB1",
	"S_FBOMB2",
	"S_FBOMB_EXPL1",
	"S_FBOMB_EXPL2",
	"S_FBOMB_EXPL3",
	"S_FBOMB_EXPL4",
	"S_FBOMB_EXPL5",
	"S_FBOMB_EXPL6",
	"S_TNTDUST_1",
	"S_TNTDUST_2",
	"S_TNTDUST_3",
	"S_TNTDUST_4",
	"S_TNTDUST_5",
	"S_TNTDUST_6",
	"S_TNTDUST_7",
	"S_TNTDUST_8",
	"S_FSGNA",
	"S_FSGNB",
	"S_FSGNC",
	"S_FSGND",

	// Black Eggman (Boss 7)
	"S_BLACKEGG_STND",
	"S_BLACKEGG_STND2",
	"S_BLACKEGG_WALK1",
	"S_BLACKEGG_WALK2",
	"S_BLACKEGG_WALK3",
	"S_BLACKEGG_WALK4",
	"S_BLACKEGG_WALK5",
	"S_BLACKEGG_WALK6",
	"S_BLACKEGG_SHOOT1",
	"S_BLACKEGG_SHOOT2",
	"S_BLACKEGG_PAIN1",
	"S_BLACKEGG_PAIN2",
	"S_BLACKEGG_PAIN3",
	"S_BLACKEGG_PAIN4",
	"S_BLACKEGG_PAIN5",
	"S_BLACKEGG_PAIN6",
	"S_BLACKEGG_PAIN7",
	"S_BLACKEGG_PAIN8",
	"S_BLACKEGG_PAIN9",
	"S_BLACKEGG_PAIN10",
	"S_BLACKEGG_PAIN11",
	"S_BLACKEGG_PAIN12",
	"S_BLACKEGG_PAIN13",
	"S_BLACKEGG_PAIN14",
	"S_BLACKEGG_PAIN15",
	"S_BLACKEGG_PAIN16",
	"S_BLACKEGG_PAIN17",
	"S_BLACKEGG_PAIN18",
	"S_BLACKEGG_PAIN19",
	"S_BLACKEGG_PAIN20",
	"S_BLACKEGG_PAIN21",
	"S_BLACKEGG_PAIN22",
	"S_BLACKEGG_PAIN23",
	"S_BLACKEGG_PAIN24",
	"S_BLACKEGG_PAIN25",
	"S_BLACKEGG_PAIN26",
	"S_BLACKEGG_PAIN27",
	"S_BLACKEGG_PAIN28",
	"S_BLACKEGG_PAIN29",
	"S_BLACKEGG_PAIN30",
	"S_BLACKEGG_PAIN31",
	"S_BLACKEGG_PAIN32",
	"S_BLACKEGG_PAIN33",
	"S_BLACKEGG_PAIN34",
	"S_BLACKEGG_PAIN35",
	"S_BLACKEGG_HITFACE1",
	"S_BLACKEGG_HITFACE2",
	"S_BLACKEGG_HITFACE3",
	"S_BLACKEGG_HITFACE4",
	"S_BLACKEGG_DIE1",
	"S_BLACKEGG_DIE2",
	"S_BLACKEGG_DIE3",
	"S_BLACKEGG_DIE4",
	"S_BLACKEGG_DIE5",
	"S_BLACKEGG_MISSILE1",
	"S_BLACKEGG_MISSILE2",
	"S_BLACKEGG_MISSILE3",
	"S_BLACKEGG_GOOP",
	"S_BLACKEGG_JUMP1",
	"S_BLACKEGG_JUMP2",
	"S_BLACKEGG_DESTROYPLAT1",
	"S_BLACKEGG_DESTROYPLAT2",
	"S_BLACKEGG_DESTROYPLAT3",

	"S_BLACKEGG_HELPER", // Collision helper

	"S_BLACKEGG_GOOP1",
	"S_BLACKEGG_GOOP2",
	"S_BLACKEGG_GOOP3",
	"S_BLACKEGG_GOOP4",
	"S_BLACKEGG_GOOP5",
	"S_BLACKEGG_GOOP6",
	"S_BLACKEGG_GOOP7",

	"S_BLACKEGG_MISSILE",

	// New Very-Last-Minute 2.1 Brak Eggman (Cy-Brak-demon)
	"S_CYBRAKDEMON_IDLE",
	"S_CYBRAKDEMON_WALK1",
	"S_CYBRAKDEMON_WALK2",
	"S_CYBRAKDEMON_WALK3",
	"S_CYBRAKDEMON_WALK4",
	"S_CYBRAKDEMON_WALK5",
	"S_CYBRAKDEMON_WALK6",
	"S_CYBRAKDEMON_CHOOSE_ATTACK1",
	"S_CYBRAKDEMON_MISSILE_ATTACK1", // Aim
	"S_CYBRAKDEMON_MISSILE_ATTACK2", // Fire
	"S_CYBRAKDEMON_MISSILE_ATTACK3", // Aim
	"S_CYBRAKDEMON_MISSILE_ATTACK4", // Fire
	"S_CYBRAKDEMON_MISSILE_ATTACK5", // Aim
	"S_CYBRAKDEMON_MISSILE_ATTACK6", // Fire
	"S_CYBRAKDEMON_FLAME_ATTACK1", // Reset
	"S_CYBRAKDEMON_FLAME_ATTACK2", // Aim
	"S_CYBRAKDEMON_FLAME_ATTACK3", // Fire
	"S_CYBRAKDEMON_FLAME_ATTACK4", // Loop
	"S_CYBRAKDEMON_CHOOSE_ATTACK2",
	"S_CYBRAKDEMON_VILE_ATTACK1",
	"S_CYBRAKDEMON_VILE_ATTACK2",
	"S_CYBRAKDEMON_VILE_ATTACK3",
	"S_CYBRAKDEMON_VILE_ATTACK4",
	"S_CYBRAKDEMON_VILE_ATTACK5",
	"S_CYBRAKDEMON_VILE_ATTACK6",
	"S_CYBRAKDEMON_NAPALM_ATTACK1",
	"S_CYBRAKDEMON_NAPALM_ATTACK2",
	"S_CYBRAKDEMON_NAPALM_ATTACK3",
	"S_CYBRAKDEMON_FINISH_ATTACK1", // If just attacked, remove MF2_FRET w/out going back to spawnstate
	"S_CYBRAKDEMON_FINISH_ATTACK2", // Force a delay between attacks so you don't get bombarded with them back-to-back
	"S_CYBRAKDEMON_PAIN1",
	"S_CYBRAKDEMON_PAIN2",
	"S_CYBRAKDEMON_PAIN3",
	"S_CYBRAKDEMON_DIE1",
	"S_CYBRAKDEMON_DIE2",
	"S_CYBRAKDEMON_DIE3",
	"S_CYBRAKDEMON_DIE4",
	"S_CYBRAKDEMON_DIE5",
	"S_CYBRAKDEMON_DIE6",
	"S_CYBRAKDEMON_DIE7",
	"S_CYBRAKDEMON_DIE8",
	"S_CYBRAKDEMON_DEINVINCIBLERIZE",
	"S_CYBRAKDEMON_INVINCIBLERIZE",

	"S_CYBRAKDEMONMISSILE",
	"S_CYBRAKDEMONMISSILE_EXPLODE1",
	"S_CYBRAKDEMONMISSILE_EXPLODE2",
	"S_CYBRAKDEMONMISSILE_EXPLODE3",

	"S_CYBRAKDEMONFLAMESHOT_FLY1",
	"S_CYBRAKDEMONFLAMESHOT_FLY2",
	"S_CYBRAKDEMONFLAMESHOT_FLY3",
	"S_CYBRAKDEMONFLAMESHOT_DIE",

	"S_CYBRAKDEMONFLAMEREST",

	"S_CYBRAKDEMONELECTRICBARRIER_INIT1",
	"S_CYBRAKDEMONELECTRICBARRIER_INIT2",
	"S_CYBRAKDEMONELECTRICBARRIER_PLAYSOUND",
	"S_CYBRAKDEMONELECTRICBARRIER1",
	"S_CYBRAKDEMONELECTRICBARRIER2",
	"S_CYBRAKDEMONELECTRICBARRIER3",
	"S_CYBRAKDEMONELECTRICBARRIER4",
	"S_CYBRAKDEMONELECTRICBARRIER5",
	"S_CYBRAKDEMONELECTRICBARRIER6",
	"S_CYBRAKDEMONELECTRICBARRIER7",
	"S_CYBRAKDEMONELECTRICBARRIER8",
	"S_CYBRAKDEMONELECTRICBARRIER9",
	"S_CYBRAKDEMONELECTRICBARRIER10",
	"S_CYBRAKDEMONELECTRICBARRIER11",
	"S_CYBRAKDEMONELECTRICBARRIER12",
	"S_CYBRAKDEMONELECTRICBARRIER13",
	"S_CYBRAKDEMONELECTRICBARRIER14",
	"S_CYBRAKDEMONELECTRICBARRIER15",
	"S_CYBRAKDEMONELECTRICBARRIER16",
	"S_CYBRAKDEMONELECTRICBARRIER17",
	"S_CYBRAKDEMONELECTRICBARRIER18",
	"S_CYBRAKDEMONELECTRICBARRIER19",
	"S_CYBRAKDEMONELECTRICBARRIER20",
	"S_CYBRAKDEMONELECTRICBARRIER21",
	"S_CYBRAKDEMONELECTRICBARRIER22",
	"S_CYBRAKDEMONELECTRICBARRIER23",
	"S_CYBRAKDEMONELECTRICBARRIER24",
	"S_CYBRAKDEMONELECTRICBARRIER_DIE1",
	"S_CYBRAKDEMONELECTRICBARRIER_DIE2",
	"S_CYBRAKDEMONELECTRICBARRIER_DIE3",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOMCHECK",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOMSUCCESS",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOMCHOOSE",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOM1",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOM2",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOM3",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOM4",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOM5",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOM6",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOM7",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOM8",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOM9",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOM10",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOM11",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOM12",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOMFAIL",
	"S_CYBRAKDEMONELECTRICBARRIER_SPARK_RANDOMLOOP",
	"S_CYBRAKDEMONELECTRICBARRIER_REVIVE1",
	"S_CYBRAKDEMONELECTRICBARRIER_REVIVE2",
	"S_CYBRAKDEMONELECTRICBARRIER_REVIVE3",

	"S_CYBRAKDEMONTARGETRETICULE1",
	"S_CYBRAKDEMONTARGETRETICULE2",
	"S_CYBRAKDEMONTARGETRETICULE3",
	"S_CYBRAKDEMONTARGETRETICULE4",
	"S_CYBRAKDEMONTARGETRETICULE5",
	"S_CYBRAKDEMONTARGETRETICULE6",
	"S_CYBRAKDEMONTARGETRETICULE7",
	"S_CYBRAKDEMONTARGETRETICULE8",
	"S_CYBRAKDEMONTARGETRETICULE9",
	"S_CYBRAKDEMONTARGETRETICULE10",
	"S_CYBRAKDEMONTARGETRETICULE11",
	"S_CYBRAKDEMONTARGETRETICULE12",
	"S_CYBRAKDEMONTARGETRETICULE13",
	"S_CYBRAKDEMONTARGETRETICULE14",

	"S_CYBRAKDEMONTARGETDOT",

	"S_CYBRAKDEMONNAPALMBOMBLARGE_FLY1",
	"S_CYBRAKDEMONNAPALMBOMBLARGE_FLY2",
	"S_CYBRAKDEMONNAPALMBOMBLARGE_FLY3",
	"S_CYBRAKDEMONNAPALMBOMBLARGE_FLY4",
	"S_CYBRAKDEMONNAPALMBOMBLARGE_DIE1", // Explode
	"S_CYBRAKDEMONNAPALMBOMBLARGE_DIE2", // Outer ring
	"S_CYBRAKDEMONNAPALMBOMBLARGE_DIE3", // Center
	"S_CYBRAKDEMONNAPALMBOMBLARGE_DIE4", // Sound

	"S_CYBRAKDEMONNAPALMBOMBSMALL",
	"S_CYBRAKDEMONNAPALMBOMBSMALL_DIE1", // Explode
	"S_CYBRAKDEMONNAPALMBOMBSMALL_DIE2", // Outer ring
	"S_CYBRAKDEMONNAPALMBOMBSMALL_DIE3", // Inner ring
	"S_CYBRAKDEMONNAPALMBOMBSMALL_DIE4", // Center
	"S_CYBRAKDEMONNAPALMBOMBSMALL_DIE5", // Sound

	"S_CYBRAKDEMONNAPALMFLAME_FLY1",
	"S_CYBRAKDEMONNAPALMFLAME_FLY2",
	"S_CYBRAKDEMONNAPALMFLAME_FLY3",
	"S_CYBRAKDEMONNAPALMFLAME_FLY4",
	"S_CYBRAKDEMONNAPALMFLAME_FLY5",
	"S_CYBRAKDEMONNAPALMFLAME_FLY6",
	"S_CYBRAKDEMONNAPALMFLAME_DIE",

	"S_CYBRAKDEMONVILEEXPLOSION1",
	"S_CYBRAKDEMONVILEEXPLOSION2",
	"S_CYBRAKDEMONVILEEXPLOSION3",

	// Metal Sonic (Race)
	"S_METALSONIC_RACE",
	// Metal Sonic (Battle)
	"S_METALSONIC_FLOAT",
	"S_METALSONIC_VECTOR",
	"S_METALSONIC_STUN",
	"S_METALSONIC_RAISE",
	"S_METALSONIC_GATHER",
	"S_METALSONIC_DASH",
	"S_METALSONIC_BOUNCE",
	"S_METALSONIC_BADBOUNCE",
	"S_METALSONIC_SHOOT",
	"S_METALSONIC_PAIN",
	"S_METALSONIC_DEATH1",
	"S_METALSONIC_DEATH2",
	"S_METALSONIC_DEATH3",
	"S_METALSONIC_DEATH4",
	"S_METALSONIC_FLEE1",
	"S_METALSONIC_FLEE2",

	"S_MSSHIELD_F1",
	"S_MSSHIELD_F2",

	// Ring
	"S_RING",
	"S_FASTRING1",
	"S_FASTRING2",
	"S_FASTRING3",
	"S_FASTRING4",
	"S_FASTRING5",
	"S_FASTRING6",
	"S_FASTRING7",
	"S_FASTRING8",
	"S_FASTRING9",
	"S_FASTRING10",
	"S_FASTRING11",
	"S_FASTRING12",

	// Blue Sphere
	"S_BLUESPHERE",
	"S_BLUESPHERE_SPAWN",

	"S_BLUESPHERE_BOUNCE1",
	"S_BLUESPHERE_BOUNCE2",

	"S_BLUESPHERE_BOUNCE3",
	"S_BLUESPHERE_BOUNCE4",

	"S_BLUESPHERE_BOUNCE5",
	"S_BLUESPHERE_BOUNCE6",
	"S_BLUESPHERE_BOUNCE7",
	"S_BLUESPHERE_BOUNCE8",

	"S_BLUESPHERE_BOUNCE9",
	"S_BLUESPHERE_BOUNCE10",
	"S_BLUESPHERE_BOUNCE11",
	"S_BLUESPHERE_BOUNCE12",

	"S_BLUESPHERE_BOUNCE13",
	"S_BLUESPHERE_BOUNCE14",
	"S_BLUESPHERE_BOUNCE15",
	"S_BLUESPHERE_BOUNCE16",
	"S_BLUESPHERE_BOUNCE17",
	"S_BLUESPHERE_BOUNCE18",
	"S_BLUESPHERE_BOUNCE19",
	"S_BLUESPHERE_BOUNCE20",

	"S_BLUESPHERE_BOUNCE21",
	"S_BLUESPHERE_BOUNCE22",
	"S_BLUESPHERE_BOUNCE23",
	"S_BLUESPHERE_BOUNCE24",
	"S_BLUESPHERE_BOUNCE25",
	"S_BLUESPHERE_BOUNCE26",
	"S_BLUESPHERE_BOUNCE27",
	"S_BLUESPHERE_BOUNCE28",

	// Bomb Sphere
	"S_BOMBSPHERE1",
	"S_BOMBSPHERE2",
	"S_BOMBSPHERE3",
	"S_BOMBSPHERE4",

	// NiGHTS Chip
	"S_NIGHTSCHIP",
	"S_NIGHTSCHIPBONUS",

	// NiGHTS Star
	"S_NIGHTSSTAR",
	"S_NIGHTSSTARXMAS",

	// Gravity Wells for special stages
	"S_GRAVWELLGREEN",
	"S_GRAVWELLRED",

	// Individual Team Rings
	"S_TEAMRING",

	// Special Stage Token
	"S_TOKEN",

	// CTF Flags
	"S_REDFLAG",
	"S_BLUEFLAG",

	// Emblem
	"S_EMBLEM1",
	"S_EMBLEM2",
	"S_EMBLEM3",
	"S_EMBLEM4",
	"S_EMBLEM5",
	"S_EMBLEM6",
	"S_EMBLEM7",
	"S_EMBLEM8",
	"S_EMBLEM9",
	"S_EMBLEM10",
	"S_EMBLEM11",
	"S_EMBLEM12",
	"S_EMBLEM13",
	"S_EMBLEM14",
	"S_EMBLEM15",
	"S_EMBLEM16",
	"S_EMBLEM17",
	"S_EMBLEM18",
	"S_EMBLEM19",
	"S_EMBLEM20",
	"S_EMBLEM21",
	"S_EMBLEM22",
	"S_EMBLEM23",
	"S_EMBLEM24",
	"S_EMBLEM25",
	"S_EMBLEM26",

	// Chaos Emeralds
	"S_CHAOSEMERALD1",
	"S_CHAOSEMERALD2",
	"S_CHAOSEMERALD_UNDER",

	"S_EMERALDSPARK1",
	"S_EMERALDSPARK2",
	"S_EMERALDSPARK3",
	"S_EMERALDSPARK4",
	"S_EMERALDSPARK5",
	"S_EMERALDSPARK6",
	"S_EMERALDSPARK7",

	// Emerald hunt shards
	"S_SHRD1",
	"S_SHRD2",
	"S_SHRD3",

	// Bubble Source
	"S_BUBBLES1",
	"S_BUBBLES2",
	"S_BUBBLES3",
	"S_BUBBLES4",

	// Level End Sign
	"S_SIGN_POLE",
	"S_SIGN_BACK",
	"S_SIGN_SIDE",
	"S_SIGN_FACE",
	"S_SIGN_ERROR",

	// Spike Ball
	"S_SPIKEBALL1",
	"S_SPIKEBALL2",
	"S_SPIKEBALL3",
	"S_SPIKEBALL4",
	"S_SPIKEBALL5",
	"S_SPIKEBALL6",
	"S_SPIKEBALL7",
	"S_SPIKEBALL8",

	// Elemental Shield's Spawn
	"S_SPINFIRE1",
	"S_SPINFIRE2",
	"S_SPINFIRE3",
	"S_SPINFIRE4",
	"S_SPINFIRE5",
	"S_SPINFIRE6",

	// Spikes
	"S_SPIKE1",
	"S_SPIKE2",
	"S_SPIKE3",
	"S_SPIKE4",
	"S_SPIKE5",
	"S_SPIKE6",
	"S_SPIKED1",
	"S_SPIKED2",

	// Wall spikes
	"S_WALLSPIKE1",
	"S_WALLSPIKE2",
	"S_WALLSPIKE3",
	"S_WALLSPIKE4",
	"S_WALLSPIKE5",
	"S_WALLSPIKE6",
	"S_WALLSPIKEBASE",
	"S_WALLSPIKED1",
	"S_WALLSPIKED2",

	// Starpost
	"S_STARPOST_IDLE",
	"S_STARPOST_FLASH",
	"S_STARPOST_STARTSPIN",
	"S_STARPOST_SPIN",
	"S_STARPOST_ENDSPIN",

	// Big floating mine
	"S_BIGMINE_IDLE",
	"S_BIGMINE_ALERT1",
	"S_BIGMINE_ALERT2",
	"S_BIGMINE_ALERT3",
	"S_BIGMINE_SET1",
	"S_BIGMINE_SET2",
	"S_BIGMINE_SET3",
	"S_BIGMINE_BLAST1",
	"S_BIGMINE_BLAST2",
	"S_BIGMINE_BLAST3",
	"S_BIGMINE_BLAST4",
	"S_BIGMINE_BLAST5",

	// Cannon Launcher
	"S_CANNONLAUNCHER1",
	"S_CANNONLAUNCHER2",
	"S_CANNONLAUNCHER3",

	// Monitor Miscellany
	"S_BOXSPARKLE1",
	"S_BOXSPARKLE2",
	"S_BOXSPARKLE3",
	"S_BOXSPARKLE4",

	"S_BOX_FLICKER",
	"S_BOX_POP1",
	"S_BOX_POP2",

	"S_GOLDBOX_FLICKER",
	"S_GOLDBOX_OFF1",
	"S_GOLDBOX_OFF2",
	"S_GOLDBOX_OFF3",
	"S_GOLDBOX_OFF4",
	"S_GOLDBOX_OFF5",
	"S_GOLDBOX_OFF6",
	"S_GOLDBOX_OFF7",

	// Monitor States (one per box)
	"S_MYSTERY_BOX",
	"S_RING_BOX",
	"S_PITY_BOX",
	"S_ATTRACT_BOX",
	"S_FORCE_BOX",
	"S_ARMAGEDDON_BOX",
	"S_WHIRLWIND_BOX",
	"S_ELEMENTAL_BOX",
	"S_SNEAKERS_BOX",
	"S_INVULN_BOX",
	"S_1UP_BOX",
	"S_EGGMAN_BOX",
	"S_MIXUP_BOX",
	"S_GRAVITY_BOX",
	"S_RECYCLER_BOX",
	"S_SCORE1K_BOX",
	"S_SCORE10K_BOX",
	"S_FLAMEAURA_BOX",
	"S_BUBBLEWRAP_BOX",
	"S_THUNDERCOIN_BOX",

	// Gold Repeat Monitor States (one per box)
	"S_PITY_GOLDBOX",
	"S_ATTRACT_GOLDBOX",
	"S_FORCE_GOLDBOX",
	"S_ARMAGEDDON_GOLDBOX",
	"S_WHIRLWIND_GOLDBOX",
	"S_ELEMENTAL_GOLDBOX",
	"S_SNEAKERS_GOLDBOX",
	"S_INVULN_GOLDBOX",
	"S_EGGMAN_GOLDBOX",
	"S_GRAVITY_GOLDBOX",
	"S_FLAMEAURA_GOLDBOX",
	"S_BUBBLEWRAP_GOLDBOX",
	"S_THUNDERCOIN_GOLDBOX",

	// Team Ring Boxes (these are special)
	"S_RING_REDBOX1",
	"S_RING_REDBOX2",
	"S_REDBOX_POP1",
	"S_REDBOX_POP2",

	"S_RING_BLUEBOX1",
	"S_RING_BLUEBOX2",
	"S_BLUEBOX_POP1",
	"S_BLUEBOX_POP2",

	// Box Icons -- 2 states each, animation and action
	"S_RING_ICON1",
	"S_RING_ICON2",

	"S_PITY_ICON1",
	"S_PITY_ICON2",

	"S_ATTRACT_ICON1",
	"S_ATTRACT_ICON2",

	"S_FORCE_ICON1",
	"S_FORCE_ICON2",

	"S_ARMAGEDDON_ICON1",
	"S_ARMAGEDDON_ICON2",

	"S_WHIRLWIND_ICON1",
	"S_WHIRLWIND_ICON2",

	"S_ELEMENTAL_ICON1",
	"S_ELEMENTAL_ICON2",

	"S_SNEAKERS_ICON1",
	"S_SNEAKERS_ICON2",

	"S_INVULN_ICON1",
	"S_INVULN_ICON2",

	"S_1UP_ICON1",
	"S_1UP_ICON2",

	"S_EGGMAN_ICON1",
	"S_EGGMAN_ICON2",

	"S_MIXUP_ICON1",
	"S_MIXUP_ICON2",

	"S_GRAVITY_ICON1",
	"S_GRAVITY_ICON2",

	"S_RECYCLER_ICON1",
	"S_RECYCLER_ICON2",

	"S_SCORE1K_ICON1",
	"S_SCORE1K_ICON2",

	"S_SCORE10K_ICON1",
	"S_SCORE10K_ICON2",

	"S_FLAMEAURA_ICON1",
	"S_FLAMEAURA_ICON2",

	"S_BUBBLEWRAP_ICON1",
	"S_BUBBLEWRAP_ICON2",

	"S_THUNDERCOIN_ICON1",
	"S_THUNDERCOIN_ICON2",

	// ---

	"S_ROCKET",

	"S_LASER",
	"S_LASER2",
	"S_LASERFLASH",

	"S_LASERFLAME1",
	"S_LASERFLAME2",
	"S_LASERFLAME3",
	"S_LASERFLAME4",
	"S_LASERFLAME5",

	"S_TORPEDO",

	"S_ENERGYBALL1",
	"S_ENERGYBALL2",

	// Skim Mine, also used by Jetty-Syn bomber
	"S_MINE1",
	"S_MINE_BOOM1",
	"S_MINE_BOOM2",
	"S_MINE_BOOM3",
	"S_MINE_BOOM4",

	// Jetty-Syn Bullet
	"S_JETBULLET1",
	"S_JETBULLET2",

	"S_TURRETLASER",
	"S_TURRETLASEREXPLODE1",
	"S_TURRETLASEREXPLODE2",

	// Cannonball
	"S_CANNONBALL1",

	// Arrow
	"S_ARROW",
	"S_ARROWBONK",

	// Glaregoyle Demon fire
	"S_DEMONFIRE",

	// The letter
	"S_LETTER",

	// GFZ flowers
	"S_GFZFLOWERA",
	"S_GFZFLOWERB",
	"S_GFZFLOWERC",

	"S_BLUEBERRYBUSH",
	"S_BERRYBUSH",
	"S_BUSH",

	// Trees (both GFZ and misc)
	"S_GFZTREE",
	"S_GFZBERRYTREE",
	"S_GFZCHERRYTREE",
	"S_CHECKERTREE",
	"S_CHECKERSUNSETTREE",
	"S_FHZTREE", // Frozen Hillside
	"S_FHZPINKTREE",
	"S_POLYGONTREE",
	"S_BUSHTREE",
	"S_BUSHREDTREE",
	"S_SPRINGTREE",

	// THZ flowers
	"S_THZFLOWERA", // THZ1 Steam flower
	"S_THZFLOWERB", // THZ1 Spin flower (red)
	"S_THZFLOWERC", // THZ1 Spin flower (yellow)

	// THZ Steam Whistle tree/bush
	"S_THZTREE",
	"S_THZTREEBRANCH1",
	"S_THZTREEBRANCH2",
	"S_THZTREEBRANCH3",
	"S_THZTREEBRANCH4",
	"S_THZTREEBRANCH5",
	"S_THZTREEBRANCH6",
	"S_THZTREEBRANCH7",
	"S_THZTREEBRANCH8",
	"S_THZTREEBRANCH9",
	"S_THZTREEBRANCH10",
	"S_THZTREEBRANCH11",
	"S_THZTREEBRANCH12",
	"S_THZTREEBRANCH13",

	// THZ Alarm
	"S_ALARM1",

	// Deep Sea Gargoyle
	"S_GARGOYLE",
	"S_BIGGARGOYLE",

	// DSZ Seaweed
	"S_SEAWEED1",
	"S_SEAWEED2",
	"S_SEAWEED3",
	"S_SEAWEED4",
	"S_SEAWEED5",
	"S_SEAWEED6",

	// Dripping Water
	"S_DRIPA1",
	"S_DRIPA2",
	"S_DRIPA3",
	"S_DRIPA4",
	"S_DRIPB1",
	"S_DRIPC1",
	"S_DRIPC2",

	// Coral
	"S_CORAL1",
	"S_CORAL2",
	"S_CORAL3",
	"S_CORAL4",
	"S_CORAL5",

	// Blue Crystal
	"S_BLUECRYSTAL1",

	// Kelp,
	"S_KELP",

	// Animated algae
	"S_ANIMALGAETOP1",
	"S_ANIMALGAETOP2",
	"S_ANIMALGAESEG",

	// DSZ Stalagmites
	"S_DSZSTALAGMITE",
	"S_DSZ2STALAGMITE",

	// DSZ Light beam
	"S_LIGHTBEAM1",
	"S_LIGHTBEAM2",
	"S_LIGHTBEAM3",
	"S_LIGHTBEAM4",
	"S_LIGHTBEAM5",
	"S_LIGHTBEAM6",
	"S_LIGHTBEAM7",
	"S_LIGHTBEAM8",
	"S_LIGHTBEAM9",
	"S_LIGHTBEAM10",
	"S_LIGHTBEAM11",
	"S_LIGHTBEAM12",

	// CEZ Chain
	"S_CEZCHAIN",

	// Flame
	"S_FLAME",
	"S_FLAMEPARTICLE",
	"S_FLAMEREST",

	// Eggman Statue
	"S_EGGSTATUE1",

	// CEZ hidden sling
	"S_SLING1",
	"S_SLING2",

	// CEZ maces and chains
	"S_SMALLMACECHAIN",
	"S_BIGMACECHAIN",
	"S_SMALLMACE",
	"S_BIGMACE",
	"S_SMALLGRABCHAIN",
	"S_BIGGRABCHAIN",

	// Yellow spring on a ball
	"S_YELLOWSPRINGBALL",
	"S_YELLOWSPRINGBALL2",
	"S_YELLOWSPRINGBALL3",
	"S_YELLOWSPRINGBALL4",
	"S_YELLOWSPRINGBALL5",

	// Red spring on a ball
	"S_REDSPRINGBALL",
	"S_REDSPRINGBALL2",
	"S_REDSPRINGBALL3",
	"S_REDSPRINGBALL4",
	"S_REDSPRINGBALL5",

	// Small Firebar
	"S_SMALLFIREBAR1",
	"S_SMALLFIREBAR2",
	"S_SMALLFIREBAR3",
	"S_SMALLFIREBAR4",
	"S_SMALLFIREBAR5",
	"S_SMALLFIREBAR6",
	"S_SMALLFIREBAR7",
	"S_SMALLFIREBAR8",
	"S_SMALLFIREBAR9",
	"S_SMALLFIREBAR10",
	"S_SMALLFIREBAR11",
	"S_SMALLFIREBAR12",
	"S_SMALLFIREBAR13",
	"S_SMALLFIREBAR14",
	"S_SMALLFIREBAR15",
	"S_SMALLFIREBAR16",

	// Big Firebar
	"S_BIGFIREBAR1",
	"S_BIGFIREBAR2",
	"S_BIGFIREBAR3",
	"S_BIGFIREBAR4",
	"S_BIGFIREBAR5",
	"S_BIGFIREBAR6",
	"S_BIGFIREBAR7",
	"S_BIGFIREBAR8",
	"S_BIGFIREBAR9",
	"S_BIGFIREBAR10",
	"S_BIGFIREBAR11",
	"S_BIGFIREBAR12",
	"S_BIGFIREBAR13",
	"S_BIGFIREBAR14",
	"S_BIGFIREBAR15",
	"S_BIGFIREBAR16",

	"S_CEZFLOWER",
	"S_CEZPOLE",
	"S_CEZBANNER1",
	"S_CEZBANNER2",
	"S_PINETREE",
	"S_CEZBUSH1",
	"S_CEZBUSH2",
	"S_CANDLE",
	"S_CANDLEPRICKET",
	"S_FLAMEHOLDER",
	"S_FIRETORCH",
	"S_WAVINGFLAG",
	"S_WAVINGFLAGSEG1",
	"S_WAVINGFLAGSEG2",
	"S_CRAWLASTATUE",
	"S_FACESTABBERSTATUE",
	"S_SUSPICIOUSFACESTABBERSTATUE_WAIT",
	"S_SUSPICIOUSFACESTABBERSTATUE_BURST1",
	"S_SUSPICIOUSFACESTABBERSTATUE_BURST2",
	"S_BRAMBLES",

	// Big Tumbleweed
	"S_BIGTUMBLEWEED",
	"S_BIGTUMBLEWEED_ROLL1",
	"S_BIGTUMBLEWEED_ROLL2",
	"S_BIGTUMBLEWEED_ROLL3",
	"S_BIGTUMBLEWEED_ROLL4",
	"S_BIGTUMBLEWEED_ROLL5",
	"S_BIGTUMBLEWEED_ROLL6",
	"S_BIGTUMBLEWEED_ROLL7",
	"S_BIGTUMBLEWEED_ROLL8",

	// Little Tumbleweed
	"S_LITTLETUMBLEWEED",
	"S_LITTLETUMBLEWEED_ROLL1",
	"S_LITTLETUMBLEWEED_ROLL2",
	"S_LITTLETUMBLEWEED_ROLL3",
	"S_LITTLETUMBLEWEED_ROLL4",
	"S_LITTLETUMBLEWEED_ROLL5",
	"S_LITTLETUMBLEWEED_ROLL6",
	"S_LITTLETUMBLEWEED_ROLL7",
	"S_LITTLETUMBLEWEED_ROLL8",

	// Cacti
	"S_CACTI1",
	"S_CACTI2",
	"S_CACTI3",
	"S_CACTI4",
	"S_CACTI5",
	"S_CACTI6",
	"S_CACTI7",
	"S_CACTI8",
	"S_CACTI9",
	"S_CACTI10",
	"S_CACTI11",
	"S_CACTITINYSEG",
	"S_CACTISMALLSEG",

	// Warning signs
	"S_ARIDSIGN_CAUTION",
	"S_ARIDSIGN_CACTI",
	"S_ARIDSIGN_SHARPTURN",

	// Oil lamp
	"S_OILLAMP",
	"S_OILLAMPFLARE",

	// TNT barrel
	"S_TNTBARREL_STND1",
	"S_TNTBARREL_EXPL1",
	"S_TNTBARREL_EXPL2",
	"S_TNTBARREL_EXPL3",
	"S_TNTBARREL_EXPL4",
	"S_TNTBARREL_EXPL5",
	"S_TNTBARREL_EXPL6",
	"S_TNTBARREL_EXPL7",
	"S_TNTBARREL_FLYING",

	// TNT proximity shell
	"S_PROXIMITY_TNT",
	"S_PROXIMITY_TNT_TRIGGER1",
	"S_PROXIMITY_TNT_TRIGGER2",
	"S_PROXIMITY_TNT_TRIGGER3",
	"S_PROXIMITY_TNT_TRIGGER4",
	"S_PROXIMITY_TNT_TRIGGER5",
	"S_PROXIMITY_TNT_TRIGGER6",
	"S_PROXIMITY_TNT_TRIGGER7",
	"S_PROXIMITY_TNT_TRIGGER8",
	"S_PROXIMITY_TNT_TRIGGER9",
	"S_PROXIMITY_TNT_TRIGGER10",
	"S_PROXIMITY_TNT_TRIGGER11",
	"S_PROXIMITY_TNT_TRIGGER12",
	"S_PROXIMITY_TNT_TRIGGER13",
	"S_PROXIMITY_TNT_TRIGGER14",
	"S_PROXIMITY_TNT_TRIGGER15",
	"S_PROXIMITY_TNT_TRIGGER16",
	"S_PROXIMITY_TNT_TRIGGER17",
	"S_PROXIMITY_TNT_TRIGGER18",
	"S_PROXIMITY_TNT_TRIGGER19",
	"S_PROXIMITY_TNT_TRIGGER20",
	"S_PROXIMITY_TNT_TRIGGER21",
	"S_PROXIMITY_TNT_TRIGGER22",
	"S_PROXIMITY_TNT_TRIGGER23",

	// Dust devil
	"S_DUSTDEVIL",
	"S_DUSTLAYER1",
	"S_DUSTLAYER2",
	"S_DUSTLAYER3",
	"S_DUSTLAYER4",
	"S_DUSTLAYER5",
	"S_ARIDDUST1",
	"S_ARIDDUST2",
	"S_ARIDDUST3",

	// Minecart
	"S_MINECART_IDLE",
	"S_MINECART_DTH1",
	"S_MINECARTEND",
	"S_MINECARTSEG_FRONT",
	"S_MINECARTSEG_BACK",
	"S_MINECARTSEG_LEFT",
	"S_MINECARTSEG_RIGHT",
	"S_MINECARTSIDEMARK1",
	"S_MINECARTSIDEMARK2",
	"S_MINECARTSPARK",

	// Saloon door
	"S_SALOONDOOR",
	"S_SALOONDOORCENTER",

	// Train cameo
	"S_TRAINCAMEOSPAWNER_1",
	"S_TRAINCAMEOSPAWNER_2",
	"S_TRAINCAMEOSPAWNER_3",
	"S_TRAINCAMEOSPAWNER_4",
	"S_TRAINCAMEOSPAWNER_5",
	"S_TRAINPUFFMAKER",

	// Train
	"S_TRAINDUST",
	"S_TRAINSTEAM",

	// Flame jet
	"S_FLAMEJETSTND",
	"S_FLAMEJETSTART",
	"S_FLAMEJETSTOP",
	"S_FLAMEJETFLAME1",
	"S_FLAMEJETFLAME2",
	"S_FLAMEJETFLAME3",
	"S_FLAMEJETFLAME4",
	"S_FLAMEJETFLAME5",
	"S_FLAMEJETFLAME6",
	"S_FLAMEJETFLAME7",
	"S_FLAMEJETFLAME8",
	"S_FLAMEJETFLAME9",

	// Spinning flame jets
	"S_FJSPINAXISA1", // Counter-clockwise
	"S_FJSPINAXISA2",
	"S_FJSPINAXISB1", // Clockwise
	"S_FJSPINAXISB2",

	// Blade's flame
	"S_FLAMEJETFLAMEB1",
	"S_FLAMEJETFLAMEB2",
	"S_FLAMEJETFLAMEB3",

	// Lavafall
	"S_LAVAFALL_DORMANT",
	"S_LAVAFALL_TELL",
	"S_LAVAFALL_SHOOT",
	"S_LAVAFALL_LAVA1",
	"S_LAVAFALL_LAVA2",
	"S_LAVAFALL_LAVA3",
	"S_LAVAFALLROCK",

	// Rollout Rock
	"S_ROLLOUTSPAWN",
	"S_ROLLOUTROCK",

	// RVZ scenery
	"S_BIGFERNLEAF",
	"S_BIGFERN1",
	"S_BIGFERN2",
	"S_JUNGLEPALM",
	"S_TORCHFLOWER",
	"S_WALLVINE_LONG",
	"S_WALLVINE_SHORT",

	// Glaregoyles
	"S_GLAREGOYLE",
	"S_GLAREGOYLE_CHARGE",
	"S_GLAREGOYLE_BLINK",
	"S_GLAREGOYLE_HOLD",
	"S_GLAREGOYLE_FIRE",
	"S_GLAREGOYLE_LOOP",
	"S_GLAREGOYLE_COOLDOWN",
	"S_GLAREGOYLEUP",
	"S_GLAREGOYLEUP_CHARGE",
	"S_GLAREGOYLEUP_BLINK",
	"S_GLAREGOYLEUP_HOLD",
	"S_GLAREGOYLEUP_FIRE",
	"S_GLAREGOYLEUP_LOOP",
	"S_GLAREGOYLEUP_COOLDOWN",
	"S_GLAREGOYLEDOWN",
	"S_GLAREGOYLEDOWN_CHARGE",
	"S_GLAREGOYLEDOWN_BLINK",
	"S_GLAREGOYLEDOWN_HOLD",
	"S_GLAREGOYLEDOWN_FIRE",
	"S_GLAREGOYLEDOWN_LOOP",
	"S_GLAREGOYLEDOWN_COOLDOWN",
	"S_GLAREGOYLELONG",
	"S_GLAREGOYLELONG_CHARGE",
	"S_GLAREGOYLELONG_BLINK",
	"S_GLAREGOYLELONG_HOLD",
	"S_GLAREGOYLELONG_FIRE",
	"S_GLAREGOYLELONG_LOOP",
	"S_GLAREGOYLELONG_COOLDOWN",

	// ATZ's Red Crystal/Target
	"S_TARGET_IDLE",
	"S_TARGET_HIT1",
	"S_TARGET_HIT2",
	"S_TARGET_RESPAWN",
	"S_TARGET_ALLDONE",

	// ATZ's green flame
	"S_GREENFLAME",

	// ATZ Blue Gargoyle
	"S_BLUEGARGOYLE",

	// Stalagmites
	"S_STG0",
	"S_STG1",
	"S_STG2",
	"S_STG3",
	"S_STG4",
	"S_STG5",
	"S_STG6",
	"S_STG7",
	"S_STG8",
	"S_STG9",

	// Xmas-specific stuff
	"S_XMASPOLE",
	"S_CANDYCANE",
	"S_SNOWMAN",    // normal
	"S_SNOWMANHAT", // with hat + scarf
	"S_LAMPPOST1",  // normal
	"S_LAMPPOST2",  // with snow
	"S_HANGSTAR",
	"S_MISTLETOE",
	// Xmas GFZ bushes
	"S_XMASBLUEBERRYBUSH",
	"S_XMASBERRYBUSH",
	"S_XMASBUSH",
	// FHZ
	"S_FHZICE1",
	"S_FHZICE2",
	"S_ROSY_IDLE1",
	"S_ROSY_IDLE2",
	"S_ROSY_IDLE3",
	"S_ROSY_IDLE4",
	"S_ROSY_JUMP",
	"S_ROSY_WALK",
	"S_ROSY_HUG",
	"S_ROSY_PAIN",
	"S_ROSY_STND",
	"S_ROSY_UNHAPPY",

	// Halloween Scenery
	// Pumpkins
	"S_JACKO1",
	"S_JACKO1OVERLAY_1",
	"S_JACKO1OVERLAY_2",
	"S_JACKO1OVERLAY_3",
	"S_JACKO1OVERLAY_4",
	"S_JACKO2",
	"S_JACKO2OVERLAY_1",
	"S_JACKO2OVERLAY_2",
	"S_JACKO2OVERLAY_3",
	"S_JACKO2OVERLAY_4",
	"S_JACKO3",
	"S_JACKO3OVERLAY_1",
	"S_JACKO3OVERLAY_2",
	"S_JACKO3OVERLAY_3",
	"S_JACKO3OVERLAY_4",
	// Dr Seuss Trees
	"S_HHZTREE_TOP",
	"S_HHZTREE_TRUNK",
	"S_HHZTREE_LEAF",
	// Mushroom
	"S_HHZSHROOM_1",
	"S_HHZSHROOM_2",
	"S_HHZSHROOM_3",
	"S_HHZSHROOM_4",
	"S_HHZSHROOM_5",
	"S_HHZSHROOM_6",
	"S_HHZSHROOM_7",
	"S_HHZSHROOM_8",
	"S_HHZSHROOM_9",
	"S_HHZSHROOM_10",
	"S_HHZSHROOM_11",
	"S_HHZSHROOM_12",
	"S_HHZSHROOM_13",
	"S_HHZSHROOM_14",
	"S_HHZSHROOM_15",
	"S_HHZSHROOM_16",
	// Misc
	"S_HHZGRASS",
	"S_HHZTENT1",
	"S_HHZTENT2",
	"S_HHZSTALAGMITE_TALL",
	"S_HHZSTALAGMITE_SHORT",

	// Botanic Serenity's loads of scenery states
	"S_BSZTALLFLOWER_RED",
	"S_BSZTALLFLOWER_PURPLE",
	"S_BSZTALLFLOWER_BLUE",
	"S_BSZTALLFLOWER_CYAN",
	"S_BSZTALLFLOWER_YELLOW",
	"S_BSZTALLFLOWER_ORANGE",
	"S_BSZFLOWER_RED",
	"S_BSZFLOWER_PURPLE",
	"S_BSZFLOWER_BLUE",
	"S_BSZFLOWER_CYAN",
	"S_BSZFLOWER_YELLOW",
	"S_BSZFLOWER_ORANGE",
	"S_BSZSHORTFLOWER_RED",
	"S_BSZSHORTFLOWER_PURPLE",
	"S_BSZSHORTFLOWER_BLUE",
	"S_BSZSHORTFLOWER_CYAN",
	"S_BSZSHORTFLOWER_YELLOW",
	"S_BSZSHORTFLOWER_ORANGE",
	"S_BSZTULIP_RED",
	"S_BSZTULIP_PURPLE",
	"S_BSZTULIP_BLUE",
	"S_BSZTULIP_CYAN",
	"S_BSZTULIP_YELLOW",
	"S_BSZTULIP_ORANGE",
	"S_BSZCLUSTER_RED",
	"S_BSZCLUSTER_PURPLE",
	"S_BSZCLUSTER_BLUE",
	"S_BSZCLUSTER_CYAN",
	"S_BSZCLUSTER_YELLOW",
	"S_BSZCLUSTER_ORANGE",
	"S_BSZBUSH_RED",
	"S_BSZBUSH_PURPLE",
	"S_BSZBUSH_BLUE",
	"S_BSZBUSH_CYAN",
	"S_BSZBUSH_YELLOW",
	"S_BSZBUSH_ORANGE",
	"S_BSZVINE_RED",
	"S_BSZVINE_PURPLE",
	"S_BSZVINE_BLUE",
	"S_BSZVINE_CYAN",
	"S_BSZVINE_YELLOW",
	"S_BSZVINE_ORANGE",
	"S_BSZSHRUB",
	"S_BSZCLOVER",
	"S_BIG_PALMTREE_TRUNK",
	"S_BIG_PALMTREE_TOP",
	"S_PALMTREE_TRUNK",
	"S_PALMTREE_TOP",

	"S_DBALL1",
	"S_DBALL2",
	"S_DBALL3",
	"S_DBALL4",
	"S_DBALL5",
	"S_DBALL6",
	"S_EGGSTATUE2",

	// Shield Orb
	"S_ARMA1",
	"S_ARMA2",
	"S_ARMA3",
	"S_ARMA4",
	"S_ARMA5",
	"S_ARMA6",
	"S_ARMA7",
	"S_ARMA8",
	"S_ARMA9",
	"S_ARMA10",
	"S_ARMA11",
	"S_ARMA12",
	"S_ARMA13",
	"S_ARMA14",
	"S_ARMA15",
	"S_ARMA16",

	"S_ARMF1",
	"S_ARMF2",
	"S_ARMF3",
	"S_ARMF4",
	"S_ARMF5",
	"S_ARMF6",
	"S_ARMF7",
	"S_ARMF8",
	"S_ARMF9",
	"S_ARMF10",
	"S_ARMF11",
	"S_ARMF12",
	"S_ARMF13",
	"S_ARMF14",
	"S_ARMF15",
	"S_ARMF16",
	"S_ARMF17",
	"S_ARMF18",
	"S_ARMF19",
	"S_ARMF20",
	"S_ARMF21",
	"S_ARMF22",
	"S_ARMF23",
	"S_ARMF24",
	"S_ARMF25",
	"S_ARMF26",
	"S_ARMF27",
	"S_ARMF28",
	"S_ARMF29",
	"S_ARMF30",
	"S_ARMF31",
	"S_ARMF32",

	"S_ARMB1",
	"S_ARMB2",
	"S_ARMB3",
	"S_ARMB4",
	"S_ARMB5",
	"S_ARMB6",
	"S_ARMB7",
	"S_ARMB8",
	"S_ARMB9",
	"S_ARMB10",
	"S_ARMB11",
	"S_ARMB12",
	"S_ARMB13",
	"S_ARMB14",
	"S_ARMB15",
	"S_ARMB16",
	"S_ARMB17",
	"S_ARMB18",
	"S_ARMB19",
	"S_ARMB20",
	"S_ARMB21",
	"S_ARMB22",
	"S_ARMB23",
	"S_ARMB24",
	"S_ARMB25",
	"S_ARMB26",
	"S_ARMB27",
	"S_ARMB28",
	"S_ARMB29",
	"S_ARMB30",
	"S_ARMB31",
	"S_ARMB32",

	"S_WIND1",
	"S_WIND2",
	"S_WIND3",
	"S_WIND4",
	"S_WIND5",
	"S_WIND6",
	"S_WIND7",
	"S_WIND8",

	"S_MAGN1",
	"S_MAGN2",
	"S_MAGN3",
	"S_MAGN4",
	"S_MAGN5",
	"S_MAGN6",
	"S_MAGN7",
	"S_MAGN8",
	"S_MAGN9",
	"S_MAGN10",
	"S_MAGN11",
	"S_MAGN12",
	"S_MAGN13",

	"S_FORC1",
	"S_FORC2",
	"S_FORC3",
	"S_FORC4",
	"S_FORC5",
	"S_FORC6",
	"S_FORC7",
	"S_FORC8",
	"S_FORC9",
	"S_FORC10",

	"S_FORC11",
	"S_FORC12",
	"S_FORC13",
	"S_FORC14",
	"S_FORC15",
	"S_FORC16",
	"S_FORC17",
	"S_FORC18",
	"S_FORC19",
	"S_FORC20",

	"S_FORC21",

	"S_ELEM1",
	"S_ELEM2",
	"S_ELEM3",
	"S_ELEM4",
	"S_ELEM5",
	"S_ELEM6",
	"S_ELEM7",
	"S_ELEM8",
	"S_ELEM9",
	"S_ELEM10",
	"S_ELEM11",
	"S_ELEM12",

	"S_ELEM13",
	"S_ELEM14",

	"S_ELEMF1",
	"S_ELEMF2",
	"S_ELEMF3",
	"S_ELEMF4",
	"S_ELEMF5",
	"S_ELEMF6",
	"S_ELEMF7",
	"S_ELEMF8",
	"S_ELEMF9",
	"S_ELEMF10",

	"S_PITY1",
	"S_PITY2",
	"S_PITY3",
	"S_PITY4",
	"S_PITY5",
	"S_PITY6",
	"S_PITY7",
	"S_PITY8",
	"S_PITY9",
	"S_PITY10",
	"S_PITY11",
	"S_PITY12",

	"S_FIRS1",
	"S_FIRS2",
	"S_FIRS3",
	"S_FIRS4",
	"S_FIRS5",
	"S_FIRS6",
	"S_FIRS7",
	"S_FIRS8",
	"S_FIRS9",

	"S_FIRS10",
	"S_FIRS11",

	"S_FIRSB1",
	"S_FIRSB2",
	"S_FIRSB3",
	"S_FIRSB4",
	"S_FIRSB5",
	"S_FIRSB6",
	"S_FIRSB7",
	"S_FIRSB8",
	"S_FIRSB9",

	"S_FIRSB10",

	"S_BUBS1",
	"S_BUBS2",
	"S_BUBS3",
	"S_BUBS4",
	"S_BUBS5",
	"S_BUBS6",
	"S_BUBS7",
	"S_BUBS8",
	"S_BUBS9",

	"S_BUBS10",
	"S_BUBS11",

	"S_BUBSB1",
	"S_BUBSB2",
	"S_BUBSB3",
	"S_BUBSB4",

	"S_BUBSB5",
	"S_BUBSB6",

	"S_ZAPS1",
	"S_ZAPS2",
	"S_ZAPS3",
	"S_ZAPS4",
	"S_ZAPS5",
	"S_ZAPS6",
	"S_ZAPS7",
	"S_ZAPS8",
	"S_ZAPS9",
	"S_ZAPS10",
	"S_ZAPS11",
	"S_ZAPS12",
	"S_ZAPS13", // blank frame
	"S_ZAPS14",
	"S_ZAPS15",
	"S_ZAPS16",

	"S_ZAPSB1", // blank frame
	"S_ZAPSB2",
	"S_ZAPSB3",
	"S_ZAPSB4",
	"S_ZAPSB5",
	"S_ZAPSB6",
	"S_ZAPSB7",
	"S_ZAPSB8",
	"S_ZAPSB9",
	"S_ZAPSB10",
	"S_ZAPSB11", // blank frame

	//Thunder spark
	"S_THUNDERCOIN_SPARK",

	// Invincibility Sparkles
	"S_IVSP",

	// Super Sonic Spark
	"S_SSPK1",
	"S_SSPK2",
	"S_SSPK3",
	"S_SSPK4",
	"S_SSPK5",

	// Flicky-sized bubble
	"S_FLICKY_BUBBLE",

	// Bluebird
	"S_FLICKY_01_OUT",
	"S_FLICKY_01_FLAP1",
	"S_FLICKY_01_FLAP2",
	"S_FLICKY_01_FLAP3",
	"S_FLICKY_01_STAND",
	"S_FLICKY_01_CENTER",

	// Rabbit
	"S_FLICKY_02_OUT",
	"S_FLICKY_02_AIM",
	"S_FLICKY_02_HOP",
	"S_FLICKY_02_UP",
	"S_FLICKY_02_DOWN",
	"S_FLICKY_02_STAND",
	"S_FLICKY_02_CENTER",

	// Chicken
	"S_FLICKY_03_OUT",
	"S_FLICKY_03_AIM",
	"S_FLICKY_03_HOP",
	"S_FLICKY_03_UP",
	"S_FLICKY_03_FLAP1",
	"S_FLICKY_03_FLAP2",
	"S_FLICKY_03_STAND",
	"S_FLICKY_03_CENTER",

	// Seal
	"S_FLICKY_04_OUT",
	"S_FLICKY_04_AIM",
	"S_FLICKY_04_HOP",
	"S_FLICKY_04_UP",
	"S_FLICKY_04_DOWN",
	"S_FLICKY_04_SWIM1",
	"S_FLICKY_04_SWIM2",
	"S_FLICKY_04_SWIM3",
	"S_FLICKY_04_SWIM4",
	"S_FLICKY_04_STAND",
	"S_FLICKY_04_CENTER",

	// Pig
	"S_FLICKY_05_OUT",
	"S_FLICKY_05_AIM",
	"S_FLICKY_05_HOP",
	"S_FLICKY_05_UP",
	"S_FLICKY_05_DOWN",
	"S_FLICKY_05_STAND",
	"S_FLICKY_05_CENTER",

	// Chipmunk
	"S_FLICKY_06_OUT",
	"S_FLICKY_06_AIM",
	"S_FLICKY_06_HOP",
	"S_FLICKY_06_UP",
	"S_FLICKY_06_DOWN",
	"S_FLICKY_06_STAND",
	"S_FLICKY_06_CENTER",

	// Penguin
	"S_FLICKY_07_OUT",
	"S_FLICKY_07_AIML",
	"S_FLICKY_07_HOPL",
	"S_FLICKY_07_UPL",
	"S_FLICKY_07_DOWNL",
	"S_FLICKY_07_AIMR",
	"S_FLICKY_07_HOPR",
	"S_FLICKY_07_UPR",
	"S_FLICKY_07_DOWNR",
	"S_FLICKY_07_SWIM1",
	"S_FLICKY_07_SWIM2",
	"S_FLICKY_07_SWIM3",
	"S_FLICKY_07_STAND",
	"S_FLICKY_07_CENTER",

	// Fish
	"S_FLICKY_08_OUT",
	"S_FLICKY_08_AIM",
	"S_FLICKY_08_HOP",
	"S_FLICKY_08_FLAP1",
	"S_FLICKY_08_FLAP2",
	"S_FLICKY_08_FLAP3",
	"S_FLICKY_08_FLAP4",
	"S_FLICKY_08_SWIM1",
	"S_FLICKY_08_SWIM2",
	"S_FLICKY_08_SWIM3",
	"S_FLICKY_08_SWIM4",
	"S_FLICKY_08_STAND",
	"S_FLICKY_08_CENTER",

	// Ram
	"S_FLICKY_09_OUT",
	"S_FLICKY_09_AIM",
	"S_FLICKY_09_HOP",
	"S_FLICKY_09_UP",
	"S_FLICKY_09_DOWN",
	"S_FLICKY_09_STAND",
	"S_FLICKY_09_CENTER",

	// Puffin
	"S_FLICKY_10_OUT",
	"S_FLICKY_10_FLAP1",
	"S_FLICKY_10_FLAP2",
	"S_FLICKY_10_STAND",
	"S_FLICKY_10_CENTER",

	// Cow
	"S_FLICKY_11_OUT",
	"S_FLICKY_11_AIM",
	"S_FLICKY_11_RUN1",
	"S_FLICKY_11_RUN2",
	"S_FLICKY_11_RUN3",
	"S_FLICKY_11_STAND",
	"S_FLICKY_11_CENTER",

	// Rat
	"S_FLICKY_12_OUT",
	"S_FLICKY_12_AIM",
	"S_FLICKY_12_RUN1",
	"S_FLICKY_12_RUN2",
	"S_FLICKY_12_RUN3",
	"S_FLICKY_12_STAND",
	"S_FLICKY_12_CENTER",

	// Bear
	"S_FLICKY_13_OUT",
	"S_FLICKY_13_AIM",
	"S_FLICKY_13_HOP",
	"S_FLICKY_13_UP",
	"S_FLICKY_13_DOWN",
	"S_FLICKY_13_STAND",
	"S_FLICKY_13_CENTER",

	// Dove
	"S_FLICKY_14_OUT",
	"S_FLICKY_14_FLAP1",
	"S_FLICKY_14_FLAP2",
	"S_FLICKY_14_FLAP3",
	"S_FLICKY_14_STAND",
	"S_FLICKY_14_CENTER",

	// Cat
	"S_FLICKY_15_OUT",
	"S_FLICKY_15_AIM",
	"S_FLICKY_15_HOP",
	"S_FLICKY_15_UP",
	"S_FLICKY_15_DOWN",
	"S_FLICKY_15_STAND",
	"S_FLICKY_15_CENTER",

	// Canary
	"S_FLICKY_16_OUT",
	"S_FLICKY_16_FLAP1",
	"S_FLICKY_16_FLAP2",
	"S_FLICKY_16_FLAP3",
	"S_FLICKY_16_STAND",
	"S_FLICKY_16_CENTER",

	// Spider
	"S_SECRETFLICKY_01_OUT",
	"S_SECRETFLICKY_01_AIM",
	"S_SECRETFLICKY_01_HOP",
	"S_SECRETFLICKY_01_UP",
	"S_SECRETFLICKY_01_DOWN",
	"S_SECRETFLICKY_01_STAND",
	"S_SECRETFLICKY_01_CENTER",

	// Bat
	"S_SECRETFLICKY_02_OUT",
	"S_SECRETFLICKY_02_FLAP1",
	"S_SECRETFLICKY_02_FLAP2",
	"S_SECRETFLICKY_02_FLAP3",
	"S_SECRETFLICKY_02_STAND",
	"S_SECRETFLICKY_02_CENTER",

	// Fan
	"S_FAN",
	"S_FAN2",
	"S_FAN3",
	"S_FAN4",
	"S_FAN5",

	// Steam Riser
	"S_STEAM1",
	"S_STEAM2",
	"S_STEAM3",
	"S_STEAM4",
	"S_STEAM5",
	"S_STEAM6",
	"S_STEAM7",
	"S_STEAM8",

	// Bumpers
	"S_BUMPER",
	"S_BUMPERHIT",

	// Balloons
	"S_BALLOON",
	"S_BALLOONPOP1",
	"S_BALLOONPOP2",
	"S_BALLOONPOP3",
	"S_BALLOONPOP4",
	"S_BALLOONPOP5",
	"S_BALLOONPOP6",

	// Yellow Spring
	"S_YELLOWSPRING1",
	"S_YELLOWSPRING2",
	"S_YELLOWSPRING3",
	"S_YELLOWSPRING4",

	// Red Spring
	"S_REDSPRING1",
	"S_REDSPRING2",
	"S_REDSPRING3",
	"S_REDSPRING4",

	// Blue Spring
	"S_BLUESPRING1",
	"S_BLUESPRING2",
	"S_BLUESPRING3",
	"S_BLUESPRING4",

	// Grey Spring
	"S_GREYSPRING1",
	"S_GREYSPRING2",
	"S_GREYSPRING3",
	"S_GREYSPRING4",

	// Yellow Diagonal Spring
	"S_YDIAG1",
	"S_YDIAG2",
	"S_YDIAG3",
	"S_YDIAG4",

	// Red Diagonal Spring
	"S_RDIAG1",
	"S_RDIAG2",
	"S_RDIAG3",
	"S_RDIAG4",

	// Blue Diagonal Spring
	"S_BDIAG1",
	"S_BDIAG2",
	"S_BDIAG3",
	"S_BDIAG4",

	// Grey Diagonal Spring
	"S_GDIAG1",
	"S_GDIAG2",
	"S_GDIAG3",
	"S_GDIAG4",

	// Yellow Horizontal Spring
	"S_YHORIZ1",
	"S_YHORIZ2",
	"S_YHORIZ3",
	"S_YHORIZ4",

	// Red Horizontal Spring
	"S_RHORIZ1",
	"S_RHORIZ2",
	"S_RHORIZ3",
	"S_RHORIZ4",

	// Blue Horizontal Spring
	"S_BHORIZ1",
	"S_BHORIZ2",
	"S_BHORIZ3",
	"S_BHORIZ4",

	// Grey Horizontal Spring
	"S_GHORIZ1",
	"S_GHORIZ2",
	"S_GHORIZ3",
	"S_GHORIZ4",

	// Booster
	"S_BOOSTERSOUND",
	"S_YELLOWBOOSTERROLLER",
	"S_YELLOWBOOSTERSEG_LEFT",
	"S_YELLOWBOOSTERSEG_RIGHT",
	"S_YELLOWBOOSTERSEG_FACE",
	"S_REDBOOSTERROLLER",
	"S_REDBOOSTERSEG_LEFT",
	"S_REDBOOSTERSEG_RIGHT",
	"S_REDBOOSTERSEG_FACE",

	// Rain
	"S_RAIN1",
	"S_RAINRETURN",

	// Snowflake
	"S_SNOW1",
	"S_SNOW2",
	"S_SNOW3",

	// Blizzard Snowball
	"S_BLIZZARDSNOW1",
	"S_BLIZZARDSNOW2",
	"S_BLIZZARDSNOW3",

	// Water Splish
	"S_SPLISH1",
	"S_SPLISH2",
	"S_SPLISH3",
	"S_SPLISH4",
	"S_SPLISH5",
	"S_SPLISH6",
	"S_SPLISH7",
	"S_SPLISH8",
	"S_SPLISH9",

	// Lava Splish
	"S_LAVASPLISH",

	// added water splash
	"S_SPLASH1",
	"S_SPLASH2",
	"S_SPLASH3",

	// lava/slime damage burn smoke
	"S_SMOKE1",
	"S_SMOKE2",
	"S_SMOKE3",
	"S_SMOKE4",
	"S_SMOKE5",

	// Bubbles
	"S_SMALLBUBBLE",
	"S_MEDIUMBUBBLE",
	"S_LARGEBUBBLE1",
	"S_LARGEBUBBLE2",
	"S_EXTRALARGEBUBBLE", // breathable

	"S_POP1", // Extra Large bubble goes POP!

	"S_WATERZAP",

	// Spindash dust
	"S_SPINDUST1",
	"S_SPINDUST2",
	"S_SPINDUST3",
	"S_SPINDUST4",
	"S_SPINDUST_BUBBLE1",
	"S_SPINDUST_BUBBLE2",
	"S_SPINDUST_BUBBLE3",
	"S_SPINDUST_BUBBLE4",
	"S_SPINDUST_FIRE1",
	"S_SPINDUST_FIRE2",
	"S_SPINDUST_FIRE3",
	"S_SPINDUST_FIRE4",

	"S_FOG1",
	"S_FOG2",
	"S_FOG3",
	"S_FOG4",
	"S_FOG5",
	"S_FOG6",
	"S_FOG7",
	"S_FOG8",
	"S_FOG9",
	"S_FOG10",
	"S_FOG11",
	"S_FOG12",
	"S_FOG13",
	"S_FOG14",

	"S_SEED",

	"S_PARTICLE",

	// Score Logos
	"S_SCRA", // 100
	"S_SCRB", // 200
	"S_SCRC", // 500
	"S_SCRD", // 1000
	"S_SCRE", // 10000
	"S_SCRF", // 400 (mario)
	"S_SCRG", // 800 (mario)
	"S_SCRH", // 2000 (mario)
	"S_SCRI", // 4000 (mario)
	"S_SCRJ", // 8000 (mario)
	"S_SCRK", // 1UP (mario)
	"S_SCRL", // 10

	// Drowning Timer Numbers
	"S_ZERO1",
	"S_ONE1",
	"S_TWO1",
	"S_THREE1",
	"S_FOUR1",
	"S_FIVE1",

	"S_ZERO2",
	"S_ONE2",
	"S_TWO2",
	"S_THREE2",
	"S_FOUR2",
	"S_FIVE2",

	"S_FLIGHTINDICATOR",

	"S_LOCKON1",
	"S_LOCKON2",
	"S_LOCKON3",
	"S_LOCKON4",
	"S_LOCKONINF1",
	"S_LOCKONINF2",
	"S_LOCKONINF3",
	"S_LOCKONINF4",

	// Tag Sign
	"S_TTAG",

	// Got Flag Sign
	"S_GOTFLAG",

	// Finish flag
	"S_FINISHFLAG",

	"S_CORK",
	"S_LHRT",

	// Red Ring
	"S_RRNG1",
	"S_RRNG2",
	"S_RRNG3",
	"S_RRNG4",
	"S_RRNG5",
	"S_RRNG6",
	"S_RRNG7",

	// Weapon Ring Ammo
	"S_BOUNCERINGAMMO",
	"S_RAILRINGAMMO",
	"S_INFINITYRINGAMMO",
	"S_AUTOMATICRINGAMMO",
	"S_EXPLOSIONRINGAMMO",
	"S_SCATTERRINGAMMO",
	"S_GRENADERINGAMMO",

	// Weapon pickup
	"S_BOUNCEPICKUP",
	"S_BOUNCEPICKUPFADE1",
	"S_BOUNCEPICKUPFADE2",
	"S_BOUNCEPICKUPFADE3",
	"S_BOUNCEPICKUPFADE4",
	"S_BOUNCEPICKUPFADE5",
	"S_BOUNCEPICKUPFADE6",
	"S_BOUNCEPICKUPFADE7",
	"S_BOUNCEPICKUPFADE8",

	"S_RAILPICKUP",
	"S_RAILPICKUPFADE1",
	"S_RAILPICKUPFADE2",
	"S_RAILPICKUPFADE3",
	"S_RAILPICKUPFADE4",
	"S_RAILPICKUPFADE5",
	"S_RAILPICKUPFADE6",
	"S_RAILPICKUPFADE7",
	"S_RAILPICKUPFADE8",

	"S_AUTOPICKUP",
	"S_AUTOPICKUPFADE1",
	"S_AUTOPICKUPFADE2",
	"S_AUTOPICKUPFADE3",
	"S_AUTOPICKUPFADE4",
	"S_AUTOPICKUPFADE5",
	"S_AUTOPICKUPFADE6",
	"S_AUTOPICKUPFADE7",
	"S_AUTOPICKUPFADE8",

	"S_EXPLODEPICKUP",
	"S_EXPLODEPICKUPFADE1",
	"S_EXPLODEPICKUPFADE2",
	"S_EXPLODEPICKUPFADE3",
	"S_EXPLODEPICKUPFADE4",
	"S_EXPLODEPICKUPFADE5",
	"S_EXPLODEPICKUPFADE6",
	"S_EXPLODEPICKUPFADE7",
	"S_EXPLODEPICKUPFADE8",

	"S_SCATTERPICKUP",
	"S_SCATTERPICKUPFADE1",
	"S_SCATTERPICKUPFADE2",
	"S_SCATTERPICKUPFADE3",
	"S_SCATTERPICKUPFADE4",
	"S_SCATTERPICKUPFADE5",
	"S_SCATTERPICKUPFADE6",
	"S_SCATTERPICKUPFADE7",
	"S_SCATTERPICKUPFADE8",

	"S_GRENADEPICKUP",
	"S_GRENADEPICKUPFADE1",
	"S_GRENADEPICKUPFADE2",
	"S_GRENADEPICKUPFADE3",
	"S_GRENADEPICKUPFADE4",
	"S_GRENADEPICKUPFADE5",
	"S_GRENADEPICKUPFADE6",
	"S_GRENADEPICKUPFADE7",
	"S_GRENADEPICKUPFADE8",

	// Thrown Weapon Rings
	"S_THROWNBOUNCE1",
	"S_THROWNBOUNCE2",
	"S_THROWNBOUNCE3",
	"S_THROWNBOUNCE4",
	"S_THROWNBOUNCE5",
	"S_THROWNBOUNCE6",
	"S_THROWNBOUNCE7",
	"S_THROWNINFINITY1",
	"S_THROWNINFINITY2",
	"S_THROWNINFINITY3",
	"S_THROWNINFINITY4",
	"S_THROWNINFINITY5",
	"S_THROWNINFINITY6",
	"S_THROWNINFINITY7",
	"S_THROWNAUTOMATIC1",
	"S_THROWNAUTOMATIC2",
	"S_THROWNAUTOMATIC3",
	"S_THROWNAUTOMATIC4",
	"S_THROWNAUTOMATIC5",
	"S_THROWNAUTOMATIC6",
	"S_THROWNAUTOMATIC7",
	"S_THROWNEXPLOSION1",
	"S_THROWNEXPLOSION2",
	"S_THROWNEXPLOSION3",
	"S_THROWNEXPLOSION4",
	"S_THROWNEXPLOSION5",
	"S_THROWNEXPLOSION6",
	"S_THROWNEXPLOSION7",
	"S_THROWNGRENADE1",
	"S_THROWNGRENADE2",
	"S_THROWNGRENADE3",
	"S_THROWNGRENADE4",
	"S_THROWNGRENADE5",
	"S_THROWNGRENADE6",
	"S_THROWNGRENADE7",
	"S_THROWNGRENADE8",
	"S_THROWNGRENADE9",
	"S_THROWNGRENADE10",
	"S_THROWNGRENADE11",
	"S_THROWNGRENADE12",
	"S_THROWNGRENADE13",
	"S_THROWNGRENADE14",
	"S_THROWNGRENADE15",
	"S_THROWNGRENADE16",
	"S_THROWNGRENADE17",
	"S_THROWNGRENADE18",
	"S_THROWNSCATTER",

	"S_RINGEXPLODE",

	"S_COIN1",
	"S_COIN2",
	"S_COIN3",
	"S_COINSPARKLE1",
	"S_COINSPARKLE2",
	"S_COINSPARKLE3",
	"S_COINSPARKLE4",
	"S_GOOMBA1",
	"S_GOOMBA1B",
	"S_GOOMBA2",
	"S_GOOMBA3",
	"S_GOOMBA4",
	"S_GOOMBA5",
	"S_GOOMBA6",
	"S_GOOMBA7",
	"S_GOOMBA8",
	"S_GOOMBA9",
	"S_GOOMBA_DEAD",
	"S_BLUEGOOMBA1",
	"S_BLUEGOOMBA1B",
	"S_BLUEGOOMBA2",
	"S_BLUEGOOMBA3",
	"S_BLUEGOOMBA4",
	"S_BLUEGOOMBA5",
	"S_BLUEGOOMBA6",
	"S_BLUEGOOMBA7",
	"S_BLUEGOOMBA8",
	"S_BLUEGOOMBA9",
	"S_BLUEGOOMBA_DEAD",

	// Mario-specific stuff
	"S_FIREFLOWER1",
	"S_FIREFLOWER2",
	"S_FIREFLOWER3",
	"S_FIREFLOWER4",
	"S_FIREBALL",
	"S_FIREBALLTRAIL1",
	"S_FIREBALLTRAIL2",
	"S_SHELL",
	"S_PUMA_START1",
	"S_PUMA_START2",
	"S_PUMA_UP1",
	"S_PUMA_UP2",
	"S_PUMA_UP3",
	"S_PUMA_DOWN1",
	"S_PUMA_DOWN2",
	"S_PUMA_DOWN3",
	"S_PUMATRAIL1",
	"S_PUMATRAIL2",
	"S_PUMATRAIL3",
	"S_PUMATRAIL4",
	"S_HAMMER",
	"S_KOOPA1",
	"S_KOOPA2",
	"S_KOOPAFLAME1",
	"S_KOOPAFLAME2",
	"S_KOOPAFLAME3",
	"S_AXE1",
	"S_AXE2",
	"S_AXE3",
	"S_MARIOBUSH1",
	"S_MARIOBUSH2",
	"S_TOAD",

	// Nights-specific stuff
	"S_NIGHTSDRONE_MAN1",
	"S_NIGHTSDRONE_MAN2",
	"S_NIGHTSDRONE_SPARKLING1",
	"S_NIGHTSDRONE_SPARKLING2",
	"S_NIGHTSDRONE_SPARKLING3",
	"S_NIGHTSDRONE_SPARKLING4",
	"S_NIGHTSDRONE_SPARKLING5",
	"S_NIGHTSDRONE_SPARKLING6",
	"S_NIGHTSDRONE_SPARKLING7",
	"S_NIGHTSDRONE_SPARKLING8",
	"S_NIGHTSDRONE_SPARKLING9",
	"S_NIGHTSDRONE_SPARKLING10",
	"S_NIGHTSDRONE_SPARKLING11",
	"S_NIGHTSDRONE_SPARKLING12",
	"S_NIGHTSDRONE_SPARKLING13",
	"S_NIGHTSDRONE_SPARKLING14",
	"S_NIGHTSDRONE_SPARKLING15",
	"S_NIGHTSDRONE_SPARKLING16",
	"S_NIGHTSDRONE_GOAL1",
	"S_NIGHTSDRONE_GOAL2",
	"S_NIGHTSDRONE_GOAL3",
	"S_NIGHTSDRONE_GOAL4",

	"S_NIGHTSPARKLE1",
	"S_NIGHTSPARKLE2",
	"S_NIGHTSPARKLE3",
	"S_NIGHTSPARKLE4",
	"S_NIGHTSPARKLESUPER1",
	"S_NIGHTSPARKLESUPER2",
	"S_NIGHTSPARKLESUPER3",
	"S_NIGHTSPARKLESUPER4",
	"S_NIGHTSLOOPHELPER",

	// NiGHTS bumper
	"S_NIGHTSBUMPER1",
	"S_NIGHTSBUMPER2",
	"S_NIGHTSBUMPER3",
	"S_NIGHTSBUMPER4",
	"S_NIGHTSBUMPER5",
	"S_NIGHTSBUMPER6",
	"S_NIGHTSBUMPER7",
	"S_NIGHTSBUMPER8",
	"S_NIGHTSBUMPER9",
	"S_NIGHTSBUMPER10",
	"S_NIGHTSBUMPER11",
	"S_NIGHTSBUMPER12",

	"S_HOOP",
	"S_HOOP_XMASA",
	"S_HOOP_XMASB",

	"S_NIGHTSCORE10",
	"S_NIGHTSCORE20",
	"S_NIGHTSCORE30",
	"S_NIGHTSCORE40",
	"S_NIGHTSCORE50",
	"S_NIGHTSCORE60",
	"S_NIGHTSCORE70",
	"S_NIGHTSCORE80",
	"S_NIGHTSCORE90",
	"S_NIGHTSCORE100",
	"S_NIGHTSCORE10_2",
	"S_NIGHTSCORE20_2",
	"S_NIGHTSCORE30_2",
	"S_NIGHTSCORE40_2",
	"S_NIGHTSCORE50_2",
	"S_NIGHTSCORE60_2",
	"S_NIGHTSCORE70_2",
	"S_NIGHTSCORE80_2",
	"S_NIGHTSCORE90_2",
	"S_NIGHTSCORE100_2",

	// NiGHTS Paraloop Powerups
	"S_NIGHTSSUPERLOOP",
	"S_NIGHTSDRILLREFILL",
	"S_NIGHTSHELPER",
	"S_NIGHTSEXTRATIME",
	"S_NIGHTSLINKFREEZE",
	"S_EGGCAPSULE",

	// Orbiting Chaos Emeralds
	"S_ORBITEM1",
	"S_ORBITEM2",
	"S_ORBITEM3",
	"S_ORBITEM4",
	"S_ORBITEM5",
	"S_ORBITEM6",
	"S_ORBITEM7",
	"S_ORBITEM8",
	"S_ORBIDYA1",
	"S_ORBIDYA2",
	"S_ORBIDYA3",
	"S_ORBIDYA4",
	"S_ORBIDYA5",

	// "Flicky" helper
	"S_NIGHTOPIANHELPER1",
	"S_NIGHTOPIANHELPER2",
	"S_NIGHTOPIANHELPER3",
	"S_NIGHTOPIANHELPER4",
	"S_NIGHTOPIANHELPER5",
	"S_NIGHTOPIANHELPER6",
	"S_NIGHTOPIANHELPER7",
	"S_NIGHTOPIANHELPER8",
	"S_NIGHTOPIANHELPER9",

	// Nightopian
	"S_PIAN0",
	"S_PIAN1",
	"S_PIAN2",
	"S_PIAN3",
	"S_PIAN4",
	"S_PIAN5",
	"S_PIAN6",
	"S_PIANSING",

	// Shleep
	"S_SHLEEP1",
	"S_SHLEEP2",
	"S_SHLEEP3",
	"S_SHLEEP4",
	"S_SHLEEPBOUNCE1",
	"S_SHLEEPBOUNCE2",
	"S_SHLEEPBOUNCE3",

	// Secret badniks and hazards, shhhh
	"S_PENGUINATOR_LOOK",
	"S_PENGUINATOR_WADDLE1",
	"S_PENGUINATOR_WADDLE2",
	"S_PENGUINATOR_WADDLE3",
	"S_PENGUINATOR_WADDLE4",
	"S_PENGUINATOR_SLIDE1",
	"S_PENGUINATOR_SLIDE2",
	"S_PENGUINATOR_SLIDE3",
	"S_PENGUINATOR_SLIDE4",
	"S_PENGUINATOR_SLIDE5",

	"S_POPHAT_LOOK",
	"S_POPHAT_SHOOT1",
	"S_POPHAT_SHOOT2",
	"S_POPHAT_SHOOT3",
	"S_POPHAT_SHOOT4",
	"S_POPSHOT",
	"S_POPSHOT_TRAIL",

	"S_HIVEELEMENTAL_LOOK",
	"S_HIVEELEMENTAL_PREPARE1",
	"S_HIVEELEMENTAL_PREPARE2",
	"S_HIVEELEMENTAL_SHOOT1",
	"S_HIVEELEMENTAL_SHOOT2",
	"S_HIVEELEMENTAL_DORMANT",
	"S_HIVEELEMENTAL_PAIN",
	"S_HIVEELEMENTAL_DIE1",
	"S_HIVEELEMENTAL_DIE2",
	"S_HIVEELEMENTAL_DIE3",

	"S_BUMBLEBORE_SPAWN",
	"S_BUMBLEBORE_LOOK1",
	"S_BUMBLEBORE_LOOK2",
	"S_BUMBLEBORE_FLY1",
	"S_BUMBLEBORE_FLY2",
	"S_BUMBLEBORE_RAISE",
	"S_BUMBLEBORE_FALL1",
	"S_BUMBLEBORE_FALL2",
	"S_BUMBLEBORE_STUCK1",
	"S_BUMBLEBORE_STUCK2",
	"S_BUMBLEBORE_DIE",

	"S_BUGGLEIDLE",
	"S_BUGGLEFLY",

	"S_SMASHSPIKE_FLOAT",
	"S_SMASHSPIKE_EASE1",
	"S_SMASHSPIKE_EASE2",
	"S_SMASHSPIKE_FALL",
	"S_SMASHSPIKE_STOMP1",
	"S_SMASHSPIKE_STOMP2",
	"S_SMASHSPIKE_RISE1",
	"S_SMASHSPIKE_RISE2",

	"S_CACO_LOOK",
	"S_CACO_WAKE1",
	"S_CACO_WAKE2",
	"S_CACO_WAKE3",
	"S_CACO_WAKE4",
	"S_CACO_ROAR",
	"S_CACO_CHASE",
	"S_CACO_CHASE_REPEAT",
	"S_CACO_RANDOM",
	"S_CACO_PREPARE_SOUND",
	"S_CACO_PREPARE1",
	"S_CACO_PREPARE2",
	"S_CACO_PREPARE3",
	"S_CACO_SHOOT_SOUND",
	"S_CACO_SHOOT1",
	"S_CACO_SHOOT2",
	"S_CACO_CLOSE",
	"S_CACO_DIE_FLAGS",
	"S_CACO_DIE_GIB1",
	"S_CACO_DIE_GIB2",
	"S_CACO_DIE_SCREAM",
	"S_CACO_DIE_SHATTER",
	"S_CACO_DIE_FALL",
	"S_CACOSHARD_RANDOMIZE",
	"S_CACOSHARD1_1",
	"S_CACOSHARD1_2",
	"S_CACOSHARD2_1",
	"S_CACOSHARD2_2",
	"S_CACOFIRE1",
	"S_CACOFIRE2",
	"S_CACOFIRE3",
	"S_CACOFIRE_EXPLODE1",
	"S_CACOFIRE_EXPLODE2",
	"S_CACOFIRE_EXPLODE3",
	"S_CACOFIRE_EXPLODE4",

	"S_SPINBOBERT_MOVE_FLIPUP",
	"S_SPINBOBERT_MOVE_UP",
	"S_SPINBOBERT_MOVE_FLIPDOWN",
	"S_SPINBOBERT_MOVE_DOWN",
	"S_SPINBOBERT_FIRE_MOVE",
	"S_SPINBOBERT_FIRE_GHOST",
	"S_SPINBOBERT_FIRE_TRAIL1",
	"S_SPINBOBERT_FIRE_TRAIL2",
	"S_SPINBOBERT_FIRE_TRAIL3",

	"S_HANGSTER_LOOK",
	"S_HANGSTER_SWOOP1",
	"S_HANGSTER_SWOOP2",
	"S_HANGSTER_ARC1",
	"S_HANGSTER_ARC2",
	"S_HANGSTER_ARC3",
	"S_HANGSTER_FLY1",
	"S_HANGSTER_FLY2",
	"S_HANGSTER_FLY3",
	"S_HANGSTER_FLY4",
	"S_HANGSTER_FLYREPEAT",
	"S_HANGSTER_ARCUP1",
	"S_HANGSTER_ARCUP2",
	"S_HANGSTER_ARCUP3",
	"S_HANGSTER_RETURN1",
	"S_HANGSTER_RETURN2",
	"S_HANGSTER_RETURN3",

	"S_CRUMBLE1",
	"S_CRUMBLE2",

	// Spark
	"S_SPRK1",
	"S_SPRK2",
	"S_SPRK3",

	// Robot Explosion
	"S_XPLD_FLICKY",
	"S_XPLD1",
	"S_XPLD2",
	"S_XPLD3",
	"S_XPLD4",
	"S_XPLD5",
	"S_XPLD6",
	"S_XPLD_EGGTRAP",

	// Underwater Explosion
	"S_WPLD1",
	"S_WPLD2",
	"S_WPLD3",
	"S_WPLD4",
	"S_WPLD5",
	"S_WPLD6",

	"S_DUST1",
	"S_DUST2",
	"S_DUST3",
	"S_DUST4",

	"S_ROCKSPAWN",

	"S_ROCKCRUMBLEA",
	"S_ROCKCRUMBLEB",
	"S_ROCKCRUMBLEC",
	"S_ROCKCRUMBLED",
	"S_ROCKCRUMBLEE",
	"S_ROCKCRUMBLEF",
	"S_ROCKCRUMBLEG",
	"S_ROCKCRUMBLEH",
	"S_ROCKCRUMBLEI",
	"S_ROCKCRUMBLEJ",
	"S_ROCKCRUMBLEK",
	"S_ROCKCRUMBLEL",
	"S_ROCKCRUMBLEM",
	"S_ROCKCRUMBLEN",
	"S_ROCKCRUMBLEO",
	"S_ROCKCRUMBLEP",

	// Level debris
	"S_GFZDEBRIS",
	"S_BRICKDEBRIS",
	"S_WOODDEBRIS",
	"S_REDBRICKDEBRIS",
	"S_BLUEBRICKDEBRIS",
	"S_YELLOWBRICKDEBRIS",

	//{ Random Item Box
	"S_RANDOMITEM1",
	"S_RANDOMITEM2",
	"S_RANDOMITEM3",
	"S_RANDOMITEM4",
	"S_RANDOMITEM5",
	"S_RANDOMITEM6",
	"S_RANDOMITEM7",
	"S_RANDOMITEM8",
	"S_RANDOMITEM9",
	"S_RANDOMITEM10",
	"S_RANDOMITEM11",
	"S_RANDOMITEM12",
	"S_DEADRANDOMITEM",

	// Random Item Pop
	"S_RANDOMITEMPOP1",
	"S_RANDOMITEMPOP2",
	"S_RANDOMITEMPOP3",
	"S_RANDOMITEMPOP4",
	//}

	"S_ITEMICON",

	// Signpost sparkles
	"S_SIGNSPARK1",
	"S_SIGNSPARK2",
	"S_SIGNSPARK3",
	"S_SIGNSPARK4",
	"S_SIGNSPARK5",
	"S_SIGNSPARK6",
	"S_SIGNSPARK7",
	"S_SIGNSPARK8",
	"S_SIGNSPARK9",
	"S_SIGNSPARK10",
	"S_SIGNSPARK11",

	// Drift Sparks
	"S_DRIFTSPARK_A1",
	"S_DRIFTSPARK_A2",
	"S_DRIFTSPARK_A3",
	"S_DRIFTSPARK_B1",
	"S_DRIFTSPARK_C1",
	"S_DRIFTSPARK_C2",
	"S_DRIFTSPARK_D1",
	"S_DRIFTSPARK_D2",

	// Brake drift sparks
	"S_BRAKEDRIFT",

	// Drift Smoke
	"S_DRIFTDUST1",
	"S_DRIFTDUST2",
	"S_DRIFTDUST3",
	"S_DRIFTDUST4",

	// Drift Sparkles
	"S_DRIFTWARNSPARK1",
	"S_DRIFTWARNSPARK2",
	"S_DRIFTWARNSPARK3",
	"S_DRIFTWARNSPARK4",

	// Fast lines
	"S_FASTLINE1",
	"S_FASTLINE2",
	"S_FASTLINE3",
	"S_FASTLINE4",
	"S_FASTLINE5",

	// Fast dust release
	"S_FASTDUST1",
	"S_FASTDUST2",
	"S_FASTDUST3",
	"S_FASTDUST4",
	"S_FASTDUST5",
	"S_FASTDUST6",
	"S_FASTDUST7",

	// Drift boost effect
	"S_DRIFTEXPLODE1",
	"S_DRIFTEXPLODE2",
	"S_DRIFTEXPLODE3",
	"S_DRIFTEXPLODE4",
	"S_DRIFTEXPLODE5",
	"S_DRIFTEXPLODE6",
	"S_DRIFTEXPLODE7",
	"S_DRIFTEXPLODE8",

	// Drift boost clip
	"S_DRIFTCLIPA1",
	"S_DRIFTCLIPA2",
	"S_DRIFTCLIPA3",
	"S_DRIFTCLIPA4",
	"S_DRIFTCLIPA5",
	"S_DRIFTCLIPA6",
	"S_DRIFTCLIPA7",
	"S_DRIFTCLIPA8",
	"S_DRIFTCLIPA9",
	"S_DRIFTCLIPA10",
	"S_DRIFTCLIPA11",
	"S_DRIFTCLIPA12",
	"S_DRIFTCLIPA13",
	"S_DRIFTCLIPA14",
	"S_DRIFTCLIPA15",
	"S_DRIFTCLIPA16",
	"S_DRIFTCLIPB1",
	"S_DRIFTCLIPB2",
	"S_DRIFTCLIPB3",
	"S_DRIFTCLIPB4",
	"S_DRIFTCLIPB5",
	"S_DRIFTCLIPB6",
	"S_DRIFTCLIPB7",
	"S_DRIFTCLIPB8",

	// Drift boost clip spark
	"S_DRIFTCLIPSPARK",

	// Sneaker boost effect
	"S_BOOSTFLAME",
	"S_BOOSTSMOKESPAWNER",
	"S_BOOSTSMOKE1",
	"S_BOOSTSMOKE2",
	"S_BOOSTSMOKE3",
	"S_BOOSTSMOKE4",
	"S_BOOSTSMOKE5",
	"S_BOOSTSMOKE6",

	// Sneaker Fire Trail
	"S_KARTFIRE1",
	"S_KARTFIRE2",
	"S_KARTFIRE3",
	"S_KARTFIRE4",
	"S_KARTFIRE5",
	"S_KARTFIRE6",
	"S_KARTFIRE7",
	"S_KARTFIRE8",

	// Angel Island Drift Strat Dust (what a mouthful!)
	"S_KARTAIZDRIFTSTRAT",

	// Invincibility Sparks
	"S_KARTINVULN_SMALL1",
	"S_KARTINVULN_SMALL2",
	"S_KARTINVULN_SMALL3",
	"S_KARTINVULN_SMALL4",
	"S_KARTINVULN_SMALL5",

	"S_KARTINVULN_LARGE1",
	"S_KARTINVULN_LARGE2",
	"S_KARTINVULN_LARGE3",
	"S_KARTINVULN_LARGE4",
	"S_KARTINVULN_LARGE5",

	// Invincibility flash overlay
	"S_INVULNFLASH1",
	"S_INVULNFLASH2",
	"S_INVULNFLASH3",
	"S_INVULNFLASH4",

	// Wipeout dust trail
	"S_WIPEOUTTRAIL1",
	"S_WIPEOUTTRAIL2",
	"S_WIPEOUTTRAIL3",
	"S_WIPEOUTTRAIL4",
	"S_WIPEOUTTRAIL5",

	// Rocket sneaker
	"S_ROCKETSNEAKER_L",
	"S_ROCKETSNEAKER_R",
	"S_ROCKETSNEAKER_LVIBRATE",
	"S_ROCKETSNEAKER_RVIBRATE",

	//{ Eggman Monitor
	"S_EGGMANITEM1",
	"S_EGGMANITEM2",
	"S_EGGMANITEM3",
	"S_EGGMANITEM4",
	"S_EGGMANITEM5",
	"S_EGGMANITEM6",
	"S_EGGMANITEM7",
	"S_EGGMANITEM8",
	"S_EGGMANITEM9",
	"S_EGGMANITEM10",
	"S_EGGMANITEM11",
	"S_EGGMANITEM12",
	"S_EGGMANITEM_DEAD",
	//}

	// Banana
	"S_BANANA",
	"S_BANANA_DEAD",

	//{ Orbinaut
	"S_ORBINAUT1",
	"S_ORBINAUT2",
	"S_ORBINAUT3",
	"S_ORBINAUT4",
	"S_ORBINAUT5",
	"S_ORBINAUT6",
	"S_ORBINAUT_DEAD",
	"S_ORBINAUT_SHIELD1",
	"S_ORBINAUT_SHIELD2",
	"S_ORBINAUT_SHIELD3",
	"S_ORBINAUT_SHIELD4",
	"S_ORBINAUT_SHIELD5",
	"S_ORBINAUT_SHIELD6",
	"S_ORBINAUT_SHIELDDEAD",
	//}
	//{ Jawz
	"S_JAWZ1",
	"S_JAWZ2",
	"S_JAWZ3",
	"S_JAWZ4",
	"S_JAWZ5",
	"S_JAWZ6",
	"S_JAWZ7",
	"S_JAWZ8",
	"S_JAWZ_DUD1",
	"S_JAWZ_DUD2",
	"S_JAWZ_DUD3",
	"S_JAWZ_DUD4",
	"S_JAWZ_DUD5",
	"S_JAWZ_DUD6",
	"S_JAWZ_DUD7",
	"S_JAWZ_DUD8",
	"S_JAWZ_SHIELD1",
	"S_JAWZ_SHIELD2",
	"S_JAWZ_SHIELD3",
	"S_JAWZ_SHIELD4",
	"S_JAWZ_SHIELD5",
	"S_JAWZ_SHIELD6",
	"S_JAWZ_SHIELD7",
	"S_JAWZ_SHIELD8",
	"S_JAWZ_DEAD1",
	"S_JAWZ_DEAD2",
	//}

	"S_PLAYERRETICULE", // Player reticule

	// Special Stage Mine
	"S_SSMINE1",
	"S_SSMINE2",
	"S_SSMINE3",
	"S_SSMINE4",
	"S_SSMINE_SHIELD1",
	"S_SSMINE_SHIELD2",
	"S_SSMINE_AIR1",
	"S_SSMINE_AIR2",
	"S_SSMINE_DEPLOY1",
	"S_SSMINE_DEPLOY2",
	"S_SSMINE_DEPLOY3",
	"S_SSMINE_DEPLOY4",
	"S_SSMINE_DEPLOY5",
	"S_SSMINE_DEPLOY6",
	"S_SSMINE_DEPLOY7",
	"S_SSMINE_DEPLOY8",
	"S_SSMINE_DEPLOY9",
	"S_SSMINE_DEPLOY10",
	"S_SSMINE_DEPLOY11",
	"S_SSMINE_DEPLOY12",
	"S_SSMINE_DEPLOY13",
	"S_SSMINE_EXPLODE",
	"S_MINEEXPLOSION1",
	"S_MINEEXPLOSION2",

	// New explosion
	"S_QUICKBOOM1",
	"S_QUICKBOOM2",
	"S_QUICKBOOM3",
	"S_QUICKBOOM4",
	"S_QUICKBOOM5",
	"S_QUICKBOOM6",
	"S_QUICKBOOM7",
	"S_QUICKBOOM8",
	"S_QUICKBOOM9",
	"S_QUICKBOOM10",

	"S_SLOWBOOM1",
	"S_SLOWBOOM2",
	"S_SLOWBOOM3",
	"S_SLOWBOOM4",
	"S_SLOWBOOM5",
	"S_SLOWBOOM6",
	"S_SLOWBOOM7",
	"S_SLOWBOOM8",
	"S_SLOWBOOM9",
	"S_SLOWBOOM10",

	// Land Mine
	"S_LANDMINE",
	"S_LANDMINE_EXPLODE",

	// Ballhog
	"S_BALLHOG1",
	"S_BALLHOG2",
	"S_BALLHOG3",
	"S_BALLHOG4",
	"S_BALLHOG5",
	"S_BALLHOG6",
	"S_BALLHOG7",
	"S_BALLHOG8",
	"S_BALLHOG_DEAD",
	"S_BALLHOGBOOM1",
	"S_BALLHOGBOOM2",
	"S_BALLHOGBOOM3",
	"S_BALLHOGBOOM4",
	"S_BALLHOGBOOM5",
	"S_BALLHOGBOOM6",
	"S_BALLHOGBOOM7",
	"S_BALLHOGBOOM8",
	"S_BALLHOGBOOM9",
	"S_BALLHOGBOOM10",
	"S_BALLHOGBOOM11",
	"S_BALLHOGBOOM12",
	"S_BALLHOGBOOM13",
	"S_BALLHOGBOOM14",
	"S_BALLHOGBOOM15",
	"S_BALLHOGBOOM16",

	// Self-Propelled Bomb - just an explosion for now...
	"S_SPB1",
	"S_SPB2",
	"S_SPB3",
	"S_SPB4",
	"S_SPB5",
	"S_SPB6",
	"S_SPB7",
	"S_SPB8",
	"S_SPB9",
	"S_SPB10",
	"S_SPB11",
	"S_SPB12",
	"S_SPB13",
	"S_SPB14",
	"S_SPB15",
	"S_SPB16",
	"S_SPB17",
	"S_SPB18",
	"S_SPB19",
	"S_SPB20",
	"S_SPB_DEAD",

	// Thunder Shield
	"S_THUNDERSHIELD1",
	"S_THUNDERSHIELD2",
	"S_THUNDERSHIELD3",
	"S_THUNDERSHIELD4",
	"S_THUNDERSHIELD5",
	"S_THUNDERSHIELD6",
	"S_THUNDERSHIELD7",
	"S_THUNDERSHIELD8",
	"S_THUNDERSHIELD9",
	"S_THUNDERSHIELD10",
	"S_THUNDERSHIELD11",
	"S_THUNDERSHIELD12",
	"S_THUNDERSHIELD13",
	"S_THUNDERSHIELD14",
	"S_THUNDERSHIELD15",
	"S_THUNDERSHIELD16",
	"S_THUNDERSHIELD17",
	"S_THUNDERSHIELD18",
	"S_THUNDERSHIELD19",
	"S_THUNDERSHIELD20",
	"S_THUNDERSHIELD21",
	"S_THUNDERSHIELD22",
	"S_THUNDERSHIELD23",
	"S_THUNDERSHIELD24",

	// Bubble Shield
	"S_BUBBLESHIELD1",
	"S_BUBBLESHIELD2",
	"S_BUBBLESHIELD3",
	"S_BUBBLESHIELD4",
	"S_BUBBLESHIELD5",
	"S_BUBBLESHIELD6",
	"S_BUBBLESHIELD7",
	"S_BUBBLESHIELD8",
	"S_BUBBLESHIELD9",
	"S_BUBBLESHIELD10",
	"S_BUBBLESHIELD11",
	"S_BUBBLESHIELD12",
	"S_BUBBLESHIELD13",
	"S_BUBBLESHIELD14",
	"S_BUBBLESHIELD15",
	"S_BUBBLESHIELD16",
	"S_BUBBLESHIELD17",
	"S_BUBBLESHIELD18",
	"S_BUBBLESHIELDBLOWUP",
	"S_BUBBLESHIELDTRAP1",
	"S_BUBBLESHIELDTRAP2",
	"S_BUBBLESHIELDTRAP3",
	"S_BUBBLESHIELDTRAP4",
	"S_BUBBLESHIELDTRAP5",
	"S_BUBBLESHIELDTRAP6",
	"S_BUBBLESHIELDTRAP7",
	"S_BUBBLESHIELDTRAP8",
	"S_BUBBLESHIELDWAVE1",
	"S_BUBBLESHIELDWAVE2",
	"S_BUBBLESHIELDWAVE3",
	"S_BUBBLESHIELDWAVE4",
	"S_BUBBLESHIELDWAVE5",
	"S_BUBBLESHIELDWAVE6",

	// Flame Shield
	"S_FLAMESHIELD1",
	"S_FLAMESHIELD2",
	"S_FLAMESHIELD3",
	"S_FLAMESHIELD4",
	"S_FLAMESHIELD5",
	"S_FLAMESHIELD6",
	"S_FLAMESHIELD7",
	"S_FLAMESHIELD8",
	"S_FLAMESHIELD9",
	"S_FLAMESHIELD10",
	"S_FLAMESHIELD11",
	"S_FLAMESHIELD12",
	"S_FLAMESHIELD13",
	"S_FLAMESHIELD14",
	"S_FLAMESHIELD15",
	"S_FLAMESHIELD16",
	"S_FLAMESHIELD17",
	"S_FLAMESHIELD18",

	"S_FLAMESHIELDDASH1",
	"S_FLAMESHIELDDASH2",
	"S_FLAMESHIELDDASH3",
	"S_FLAMESHIELDDASH4",
	"S_FLAMESHIELDDASH5",
	"S_FLAMESHIELDDASH6",
	"S_FLAMESHIELDDASH7",
	"S_FLAMESHIELDDASH8",
	"S_FLAMESHIELDDASH9",
	"S_FLAMESHIELDDASH10",
	"S_FLAMESHIELDDASH11",
	"S_FLAMESHIELDDASH12",

	"S_FLAMESHIELDDASH2_UNDERLAY",
	"S_FLAMESHIELDDASH5_UNDERLAY",
	"S_FLAMESHIELDDASH8_UNDERLAY",
	"S_FLAMESHIELDDASH11_UNDERLAY",

	"S_FLAMESHIELDPAPER",
	"S_FLAMESHIELDLINE1",
	"S_FLAMESHIELDLINE2",
	"S_FLAMESHIELDLINE3",
	"S_FLAMESHIELDFLASH",

	// The legend
	"S_SINK",
	"S_SINK_SHIELD",
	"S_SINKTRAIL1",
	"S_SINKTRAIL2",
	"S_SINKTRAIL3",

	// Battle Mode bumper
	"S_BATTLEBUMPER1",
	"S_BATTLEBUMPER2",
	"S_BATTLEBUMPER3",

	"S_BATTLEBUMPER_EXCRYSTALA1",
	"S_BATTLEBUMPER_EXCRYSTALA2",
	"S_BATTLEBUMPER_EXCRYSTALA3",
	"S_BATTLEBUMPER_EXCRYSTALA4",

	"S_BATTLEBUMPER_EXCRYSTALB1",
	"S_BATTLEBUMPER_EXCRYSTALB2",
	"S_BATTLEBUMPER_EXCRYSTALB3",
	"S_BATTLEBUMPER_EXCRYSTALB4",

	"S_BATTLEBUMPER_EXCRYSTALC1",
	"S_BATTLEBUMPER_EXCRYSTALC2",
	"S_BATTLEBUMPER_EXCRYSTALC3",
	"S_BATTLEBUMPER_EXCRYSTALC4",

	"S_BATTLEBUMPER_EXSHELLA1",
	"S_BATTLEBUMPER_EXSHELLA2",

	"S_BATTLEBUMPER_EXSHELLB1",
	"S_BATTLEBUMPER_EXSHELLB2",

	"S_BATTLEBUMPER_EXSHELLC1",
	"S_BATTLEBUMPER_EXSHELLC2",

	"S_BATTLEBUMPER_EXDEBRIS1",
	"S_BATTLEBUMPER_EXDEBRIS2",

	"S_BATTLEBUMPER_EXBLAST1",
	"S_BATTLEBUMPER_EXBLAST2",
	"S_BATTLEBUMPER_EXBLAST3",
	"S_BATTLEBUMPER_EXBLAST4",
	"S_BATTLEBUMPER_EXBLAST5",
	"S_BATTLEBUMPER_EXBLAST6",
	"S_BATTLEBUMPER_EXBLAST7",
	"S_BATTLEBUMPER_EXBLAST8",
	"S_BATTLEBUMPER_EXBLAST9",
	"S_BATTLEBUMPER_EXBLAST10",

	// DEZ respawn laser
	"S_DEZLASER",
	"S_DEZLASER_TRAIL1",
	"S_DEZLASER_TRAIL2",
	"S_DEZLASER_TRAIL3",
	"S_DEZLASER_TRAIL4",
	"S_DEZLASER_TRAIL5",

	// Audience Members
	"S_RANDOMAUDIENCE",
	"S_AUDIENCE_CHAO_CHEER1",
	"S_AUDIENCE_CHAO_CHEER2",
	"S_AUDIENCE_CHAO_WIN1",
	"S_AUDIENCE_CHAO_WIN2",
	"S_AUDIENCE_CHAO_LOSE",

	// 1.0 Kart Decoratives
	"S_FLAYM1",
	"S_FLAYM2",
	"S_FLAYM3",
	"S_FLAYM4",
	"S_DEVIL",
	"S_ANGEL",
	"S_PALMTREE",
	"S_FLAG",
	"S_HEDGEHOG", // (Rimshot)
	"S_BUSH1",
	"S_TWEE",
	"S_HYDRANT",

	// New Misc Decorations
	"S_BIGPUMA1",
	"S_BIGPUMA2",
	"S_BIGPUMA3",
	"S_BIGPUMA4",
	"S_BIGPUMA5",
	"S_BIGPUMA6",
	"S_APPLE1",
	"S_APPLE2",
	"S_APPLE3",
	"S_APPLE4",
	"S_APPLE5",
	"S_APPLE6",
	"S_APPLE7",
	"S_APPLE8",

	// D00Dkart - Fall Flowers
	"S_DOOD_FLOWER1",
	"S_DOOD_FLOWER2",
	"S_DOOD_FLOWER3",
	"S_DOOD_FLOWER4",
	"S_DOOD_FLOWER5",
	"S_DOOD_FLOWER6",

	// D00Dkart - Super Circuit Box
	"S_DOOD_BOX1",
	"S_DOOD_BOX2",
	"S_DOOD_BOX3",
	"S_DOOD_BOX4",
	"S_DOOD_BOX5",

	// D00Dkart - Diddy Kong Racing Bumper
	"S_DOOD_BALLOON",

	// Chaotix Big Ring
	"S_BIGRING01",
	"S_BIGRING02",
	"S_BIGRING03",
	"S_BIGRING04",
	"S_BIGRING05",
	"S_BIGRING06",
	"S_BIGRING07",
	"S_BIGRING08",
	"S_BIGRING09",
	"S_BIGRING10",
	"S_BIGRING11",
	"S_BIGRING12",

	// SNES Objects
	"S_SNES_DONUTBUSH1",
	"S_SNES_DONUTBUSH2",
	"S_SNES_DONUTBUSH3",

	// GBA Objects
	"S_GBA_BOO1",
	"S_GBA_BOO2",
	"S_GBA_BOO3",
	"S_GBA_BOO4",

	// Sapphire Coast Mobs
	"S_BUZZBOMBER_LOOK1",
	"S_BUZZBOMBER_LOOK2",
	"S_BUZZBOMBER_FLY1",
	"S_BUZZBOMBER_FLY2",
	"S_BUZZBOMBER_FLY3",
	"S_BUZZBOMBER_FLY4",

	"S_CHOMPER_SPAWN",
	"S_CHOMPER_HOP1",
	"S_CHOMPER_HOP2",
	"S_CHOMPER_TURNAROUND",

	"S_PALMTREE2",
	"S_PURPLEFLOWER1",
	"S_PURPLEFLOWER2",
	"S_YELLOWFLOWER1",
	"S_YELLOWFLOWER2",
	"S_PLANT2",
	"S_PLANT3",
	"S_PLANT4",

	// Crystal Abyss Mobs
	"S_SKULL",
	"S_PHANTREE",
	"S_FLYINGGARG1",
	"S_FLYINGGARG2",
	"S_FLYINGGARG3",
	"S_FLYINGGARG4",
	"S_FLYINGGARG5",
	"S_FLYINGGARG6",
	"S_FLYINGGARG7",
	"S_FLYINGGARG8",
	"S_LAMPPOST",
	"S_MOSSYTREE",

	"S_BUMP1",
	"S_BUMP2",
	"S_BUMP3",

	"S_FLINGENERGY1",
	"S_FLINGENERGY2",
	"S_FLINGENERGY3",

	"S_CLASH1",
	"S_CLASH2",
	"S_CLASH3",
	"S_CLASH4",
	"S_CLASH5",
	"S_CLASH6",

	"S_FIREDITEM1",
	"S_FIREDITEM2",
	"S_FIREDITEM3",
	"S_FIREDITEM4",

	"S_INSTASHIELDA1", // No damage instashield effect
	"S_INSTASHIELDA2",
	"S_INSTASHIELDA3",
	"S_INSTASHIELDA4",
	"S_INSTASHIELDA5",
	"S_INSTASHIELDA6",
	"S_INSTASHIELDA7",
	"S_INSTASHIELDB1",
	"S_INSTASHIELDB2",
	"S_INSTASHIELDB3",
	"S_INSTASHIELDB4",
	"S_INSTASHIELDB5",
	"S_INSTASHIELDB6",
	"S_INSTASHIELDB7",

	"S_PLAYERARROW", // Above player arrow
	"S_PLAYERARROW_BOX",
	"S_PLAYERARROW_ITEM",
	"S_PLAYERARROW_NUMBER",
	"S_PLAYERARROW_X",
	"S_PLAYERARROW_WANTED1",
	"S_PLAYERARROW_WANTED2",
	"S_PLAYERARROW_WANTED3",
	"S_PLAYERARROW_WANTED4",
	"S_PLAYERARROW_WANTED5",
	"S_PLAYERARROW_WANTED6",
	"S_PLAYERARROW_WANTED7",

	"S_PLAYERBOMB1", // Player bomb overlay
	"S_PLAYERBOMB2",
	"S_PLAYERBOMB3",
	"S_PLAYERBOMB4",
	"S_PLAYERBOMB5",
	"S_PLAYERBOMB6",
	"S_PLAYERBOMB7",
	"S_PLAYERBOMB8",
	"S_PLAYERBOMB9",
	"S_PLAYERBOMB10",
	"S_PLAYERBOMB11",
	"S_PLAYERBOMB12",
	"S_PLAYERBOMB13",
	"S_PLAYERBOMB14",
	"S_PLAYERBOMB15",
	"S_PLAYERBOMB16",
	"S_PLAYERBOMB17",
	"S_PLAYERBOMB18",
	"S_PLAYERBOMB19",
	"S_PLAYERBOMB20",

	"S_PLAYERITEM1", // Player item overlay
	"S_PLAYERITEM2",
	"S_PLAYERITEM3",
	"S_PLAYERITEM4",
	"S_PLAYERITEM5",
	"S_PLAYERITEM6",
	"S_PLAYERITEM7",
	"S_PLAYERITEM8",
	"S_PLAYERITEM9",
	"S_PLAYERITEM10",
	"S_PLAYERITEM11",
	"S_PLAYERITEM12",

	"S_PLAYERFAKE1", // Player fake overlay
	"S_PLAYERFAKE2",
	"S_PLAYERFAKE3",
	"S_PLAYERFAKE4",
	"S_PLAYERFAKE5",
	"S_PLAYERFAKE6",
	"S_PLAYERFAKE7",
	"S_PLAYERFAKE8",
	"S_PLAYERFAKE9",
	"S_PLAYERFAKE10",
	"S_PLAYERFAKE11",
	"S_PLAYERFAKE12",

	"S_KARMAWHEEL", // Karma player wheels

	"S_BATTLEPOINT1A", // Battle point indicators
	"S_BATTLEPOINT1B",
	"S_BATTLEPOINT1C",
	"S_BATTLEPOINT1D",
	"S_BATTLEPOINT1E",
	"S_BATTLEPOINT1F",
	"S_BATTLEPOINT1G",
	"S_BATTLEPOINT1H",
	"S_BATTLEPOINT1I",

	"S_BATTLEPOINT2A",
	"S_BATTLEPOINT2B",
	"S_BATTLEPOINT2C",
	"S_BATTLEPOINT2D",
	"S_BATTLEPOINT2E",
	"S_BATTLEPOINT2F",
	"S_BATTLEPOINT2G",
	"S_BATTLEPOINT2H",
	"S_BATTLEPOINT2I",

	"S_BATTLEPOINT3A",
	"S_BATTLEPOINT3B",
	"S_BATTLEPOINT3C",
	"S_BATTLEPOINT3D",
	"S_BATTLEPOINT3E",
	"S_BATTLEPOINT3F",
	"S_BATTLEPOINT3G",
	"S_BATTLEPOINT3H",
	"S_BATTLEPOINT3I",

	// Thunder shield use stuff;
	"S_KSPARK1",	// Sparkling Radius
	"S_KSPARK2",
	"S_KSPARK3",
	"S_KSPARK4",
	"S_KSPARK5",
	"S_KSPARK6",
	"S_KSPARK7",
	"S_KSPARK8",
	"S_KSPARK9",
	"S_KSPARK10",
	"S_KSPARK11",
	"S_KSPARK12",
	"S_KSPARK13",	// ... that's an awful lot.

	"S_LZIO11",	// Straight lightning bolt
	"S_LZIO12",
	"S_LZIO13",
	"S_LZIO14",
	"S_LZIO15",
	"S_LZIO16",
	"S_LZIO17",
	"S_LZIO18",
	"S_LZIO19",

	"S_LZIO21",	// Straight lightning bolt (flipped)
	"S_LZIO22",
	"S_LZIO23",
	"S_LZIO24",
	"S_LZIO25",
	"S_LZIO26",
	"S_LZIO27",
	"S_LZIO28",
	"S_LZIO29",

	"S_KLIT1",	// Diagonal lightning. No, it not being straight doesn't make it gay.
	"S_KLIT2",
	"S_KLIT3",
	"S_KLIT4",
	"S_KLIT5",
	"S_KLIT6",
	"S_KLIT7",
	"S_KLIT8",
	"S_KLIT9",
	"S_KLIT10",
	"S_KLIT11",
	"S_KLIT12",

	"S_FZEROSMOKE1", // F-Zero NO CONTEST explosion
	"S_FZEROSMOKE2",
	"S_FZEROSMOKE3",
	"S_FZEROSMOKE4",
	"S_FZEROSMOKE5",

	"S_FZEROBOOM1",
	"S_FZEROBOOM2",
	"S_FZEROBOOM3",
	"S_FZEROBOOM4",
	"S_FZEROBOOM5",
	"S_FZEROBOOM6",
	"S_FZEROBOOM7",
	"S_FZEROBOOM8",
	"S_FZEROBOOM9",
	"S_FZEROBOOM10",
	"S_FZEROBOOM11",
	"S_FZEROBOOM12",

	"S_FZSLOWSMOKE1",
	"S_FZSLOWSMOKE2",
	"S_FZSLOWSMOKE3",
	"S_FZSLOWSMOKE4",
	"S_FZSLOWSMOKE5",

	// Various plants
	"S_SONICBUSH",

	// Marble Zone
	"S_MARBLEFLAMEPARTICLE",
	"S_MARBLETORCH",
	"S_MARBLELIGHT",
	"S_MARBLEBURNER",

	// CD Special Stage
	"S_CDUFO",
	"S_CDUFO_DIE",

	// Rusty Rig
	"S_RUSTYLAMP_ORANGE",
	"S_RUSTYCHAIN",

	// Smokin' & Vapin' (Don't try this at home, kids!)
	"S_PETSMOKE0",
	"S_PETSMOKE1",
	"S_PETSMOKE2",
	"S_PETSMOKE3",
	"S_PETSMOKE4",
	"S_PETSMOKE5",
	"S_VVVAPING0",
	"S_VVVAPING1",
	"S_VVVAPING2",
	"S_VVVAPING3",
	"S_VVVAPING4",
	"S_VVVAPING5",
	"S_VVVAPE",

	// Hill Top Zone
	"S_HTZTREE",
	"S_HTZBUSH",

	// Ports of gardens
	"S_SGVINE1",
	"S_SGVINE2",
	"S_SGVINE3",
	"S_PGTREE",
	"S_PGFLOWER1",
	"S_PGFLOWER2",
	"S_PGFLOWER3",
	"S_PGBUSH",
	"S_DHPILLAR",

	// Midnight Channel stuff:
	"S_SPOTLIGHT",	// Spotlight decoration
	"S_RANDOMSHADOW",	// Random Shadow. They're static and don't do nothing.
	"S_GARU1",
	"S_GARU2",
	"S_GARU3",
	"S_TGARU",
	"S_TGARU1",
	"S_TGARU2",
	"S_TGARU3",	// Wind attack used by Roaming Shadows on Players.
	"S_ROAMINGSHADOW",	// Roaming Shadow (the one that uses above's wind attack or smth)
	"S_MAYONAKAARROW",	// Arrow sign

	// Mementos stuff:
	"S_REAPER_INVIS",		// Reaper waiting for spawning
	"S_REAPER",			// Reaper main frame where its thinker is handled
	"S_MEMENTOSTP",		// Mementos teleporter state. (Used for spawning particles)

	// JackInTheBox
	"S_JITB1",
	"S_JITB2",
	"S_JITB3",
	"S_JITB4",
	"S_JITB5",
	"S_JITB6",

	// Color Drive
	"S_CDMOONSP",
	"S_CDBUSHSP",
	"S_CDTREEASP",
	"S_CDTREEBSP",

	// Daytona Speedway
	"S_PINETREE",
	"S_PINETREE_SIDE",

	// Egg Zeppelin
	"S_EZZPROPELLER",
	"S_EZZPROPELLER_BLADE",

	// Desert Palace
	"S_DP_PALMTREE",

	// Aurora Atoll
	"S_AAZTREE_SEG",
	"S_AAZTREE_COCONUT",
	"S_AAZTREE_LEAF",

	// Barren Badlands
	"S_BBZDUST1", // Dust
	"S_BBZDUST2",
	"S_BBZDUST3",
	"S_BBZDUST4",
	"S_FROGGER", // Frog badniks
	"S_FROGGER_ATTACK",
	"S_FROGGER_JUMP",
	"S_FROGTONGUE",
	"S_FROGTONGUE_JOINT",
	"S_ROBRA", // Black cobra badniks
	"S_ROBRA_HEAD",
	"S_ROBRA_JOINT",
	"S_ROBRASHELL_INSIDE",
	"S_ROBRASHELL_OUTSIDE",
	"S_BLUEROBRA", // Blue cobra badniks
	"S_BLUEROBRA_HEAD",
	"S_BLUEROBRA_JOINT",

	// Eerie Grove
	"S_EERIEFOG1",
	"S_EERIEFOG2",
	"S_EERIEFOG3",
	"S_EERIEFOG4",
	"S_EERIEFOG5",

	// SMK ports
	"S_SMK_PIPE1", // Generic pipes
	"S_SMK_PIPE2",
	"S_SMK_MOLE", // Donut Plains Monty Moles
	"S_SMK_THWOMP", // Bowser Castle Thwomps
	"S_SMK_SNOWBALL", // Vanilla Lake snowballs
	"S_SMK_ICEBLOCK", // Vanilla Lake breakable ice blocks
	"S_SMK_ICEBLOCK2",
	"S_SMK_ICEBLOCK_DEBRIS",
	"S_SMK_ICEBLOCK_DEBRIS2",

	// Ezo's maps
	"S_BLUEFIRE1",
	"S_BLUEFIRE2",
	"S_BLUEFIRE3",
	"S_BLUEFIRE4",
	"S_GREENFIRE1",
	"S_GREENFIRE2",
	"S_GREENFIRE3",
	"S_GREENFIRE4",
	"S_REGALCHEST",
	"S_CHIMERASTATUE",
	"S_DRAGONSTATUE",
	"S_LIZARDMANSTATUE",
	"S_PEGASUSSTATUE",
	"S_ZELDAFIRE1",
	"S_ZELDAFIRE2",
	"S_ZELDAFIRE3",
	"S_ZELDAFIRE4",
	"S_GANBARETHING",
	"S_GANBAREDUCK",
	"S_GANBARETREE",
	"S_MONOIDLE",
	"S_MONOCHASE1",
	"S_MONOCHASE2",
	"S_MONOCHASE3",
	"S_MONOCHASE4",
	"S_MONOPAIN",
	"S_REDZELDAFIRE1",
	"S_REDZELDAFIRE2",
	"S_REDZELDAFIRE3",
	"S_REDZELDAFIRE4",
	"S_BOWLINGPIN",
	"S_BOWLINGHIT1",
	"S_BOWLINGHIT2",
	"S_BOWLINGHIT3",
	"S_BOWLINGHIT4",
	"S_ARIDTOAD",
	"S_TOADHIT1",
	"S_TOADHIT2",
	"S_TOADHIT3",
	"S_TOADHIT4",
	"S_EBARRELIDLE",
	"S_EBARREL1",
	"S_EBARREL2",
	"S_EBARREL3",
	"S_EBARREL4",
	"S_EBARREL5",
	"S_EBARREL6",
	"S_EBARREL7",
	"S_EBARREL8",
	"S_EBARREL9",
	"S_EBARREL10",
	"S_EBARREL11",
	"S_EBARREL12",
	"S_EBARREL13",
	"S_EBARREL14",
	"S_EBARREL15",
	"S_EBARREL16",
	"S_EBARREL17",
	"S_EBARREL18",
	"S_MERRYHORSE",
	"S_BLUEFRUIT",
	"S_ORANGEFRUIT",
	"S_REDFRUIT",
	"S_PINKFRUIT",
	"S_ADVENTURESPIKEA1",
	"S_ADVENTURESPIKEA2",
	"S_ADVENTURESPIKEB1",
	"S_ADVENTURESPIKEB2",
	"S_ADVENTURESPIKEC1",
	"S_ADVENTURESPIKEC2",
	"S_BOOSTPROMPT1",
	"S_BOOSTPROMPT2",
	"S_BOOSTOFF1",
	"S_BOOSTOFF2",
	"S_BOOSTON1",
	"S_BOOSTON2",
	"S_LIZARDMAN",
	"S_LIONMAN",

	"S_KARMAFIREWORK1",
	"S_KARMAFIREWORK2",
	"S_KARMAFIREWORK3",
	"S_KARMAFIREWORK4",
	"S_KARMAFIREWORKTRAIL",

	// Opaque smoke version, to prevent lag
	"S_OPAQUESMOKE1",
	"S_OPAQUESMOKE2",
	"S_OPAQUESMOKE3",
	"S_OPAQUESMOKE4",
	"S_OPAQUESMOKE5",

	"S_FOLLOWERBUBBLE_FRONT",
	"S_FOLLOWERBUBBLE_BACK",

	"S_GCHAOIDLE",
	"S_GCHAOFLY",
	"S_GCHAOSAD1",
	"S_GCHAOSAD2",
	"S_GCHAOSAD3",
	"S_GCHAOSAD4",
	"S_GCHAOHAPPY1",
	"S_GCHAOHAPPY2",
	"S_GCHAOHAPPY3",
	"S_GCHAOHAPPY4",

	"S_CHEESEIDLE",
	"S_CHEESEFLY",
	"S_CHEESESAD1",
	"S_CHEESESAD2",
	"S_CHEESESAD3",
	"S_CHEESESAD4",
	"S_CHEESEHAPPY1",
	"S_CHEESEHAPPY2",
	"S_CHEESEHAPPY3",
	"S_CHEESEHAPPY4",

	"S_RINGDEBT",
	"S_RINGSPARKS1",
	"S_RINGSPARKS2",
	"S_RINGSPARKS3",
	"S_RINGSPARKS4",
	"S_RINGSPARKS5",
	"S_RINGSPARKS6",
	"S_RINGSPARKS7",
	"S_RINGSPARKS8",
	"S_RINGSPARKS9",
	"S_RINGSPARKS10",
	"S_RINGSPARKS11",
	"S_RINGSPARKS12",
	"S_RINGSPARKS13",
	"S_RINGSPARKS14",
	"S_RINGSPARKS15",

	"S_DRAFTDUST1",
	"S_DRAFTDUST2",
	"S_DRAFTDUST3",
	"S_DRAFTDUST4",
	"S_DRAFTDUST5",

	"S_OVERTIME_BULB1",
	"S_OVERTIME_BULB2",
	"S_OVERTIME_LASER",
	"S_OVERTIME_CENTER",

	"S_BATTLECAPSULE_SIDE1",
	"S_BATTLECAPSULE_SIDE2",
	"S_BATTLECAPSULE_TOP",
	"S_BATTLECAPSULE_BUTTON",
	"S_BATTLECAPSULE_SUPPORT",
	"S_BATTLECAPSULE_SUPPORTFLY",

	"S_EGOORB",

	"S_WATERTRAIL1",
	"S_WATERTRAIL2",
	"S_WATERTRAIL3",
	"S_WATERTRAIL4",
	"S_WATERTRAIL5",
	"S_WATERTRAIL6",
	"S_WATERTRAIL7",
	"S_WATERTRAIL8",
	"S_WATERTRAILUNDERLAY1",
	"S_WATERTRAILUNDERLAY2",
	"S_WATERTRAILUNDERLAY3",
	"S_WATERTRAILUNDERLAY4",
	"S_WATERTRAILUNDERLAY5",
	"S_WATERTRAILUNDERLAY6",
	"S_WATERTRAILUNDERLAY7",
	"S_WATERTRAILUNDERLAY8",

	"S_SPINDASHDUST",
	"S_SPINDASHWIND",

#ifdef SEENAMES
	"S_NAMECHECK",
#endif
};

// RegEx to generate this from info.h: ^\tMT_([^,]+), --> \t"MT_\1",
// I am leaving the prefixes solely for clarity to programmers,
// because sadly no one remembers this place while searching for full state names.
static const char *const MOBJTYPE_LIST[] = {  // array length left dynamic for sanity testing later.
	"MT_NULL",
	"MT_UNKNOWN",

	"MT_THOK", // Thok! mobj
	"MT_PLAYER",

	// Enemies
	"MT_BLUECRAWLA", // Crawla (Blue)
	"MT_REDCRAWLA", // Crawla (Red)
	"MT_GFZFISH", // SDURF
	"MT_GOLDBUZZ", // Buzz (Gold)
	"MT_REDBUZZ", // Buzz (Red)
	"MT_JETTBOMBER", // Jetty-Syn Bomber
	"MT_JETTGUNNER", // Jetty-Syn Gunner
	"MT_CRAWLACOMMANDER", // Crawla Commander
	"MT_DETON", // Deton
	"MT_SKIM", // Skim mine dropper
	"MT_TURRET", // Industrial Turret
	"MT_POPUPTURRET", // Pop-Up Turret
	"MT_SPINCUSHION", // Spincushion
	"MT_CRUSHSTACEAN", // Crushstacean
	"MT_CRUSHCLAW", // Big meaty claw
	"MT_CRUSHCHAIN", // Chain
	"MT_BANPYURA", // Banpyura
	"MT_BANPSPRING", // Banpyura spring
	"MT_JETJAW", // Jet Jaw
	"MT_SNAILER", // Snailer
	"MT_VULTURE", // BASH
	"MT_POINTY", // Pointy
	"MT_POINTYBALL", // Pointy Ball
	"MT_ROBOHOOD", // Robo-Hood
	"MT_FACESTABBER", // Castlebot Facestabber
	"MT_FACESTABBERSPEAR", // Castlebot Facestabber spear aura
	"MT_EGGGUARD", // Egg Guard
	"MT_EGGSHIELD", // Egg Guard's shield
	"MT_GSNAPPER", // Green Snapper
	"MT_SNAPPER_LEG", // Green Snapper leg
	"MT_SNAPPER_HEAD", // Green Snapper head
	"MT_MINUS", // Minus
	"MT_MINUSDIRT", // Minus dirt
	"MT_SPRINGSHELL", // Spring Shell
	"MT_YELLOWSHELL", // Spring Shell (yellow)
	"MT_UNIDUS", // Unidus
	"MT_UNIBALL", // Unidus Ball
	"MT_CANARIVORE", // Canarivore
	"MT_CANARIVORE_GAS", // Canarivore gas
	"MT_PYREFLY", // Pyre Fly
	"MT_PYREFLY_FIRE", // Pyre Fly fire
	"MT_PTERABYTESPAWNER", // Pterabyte spawner
	"MT_PTERABYTEWAYPOINT", // Pterabyte waypoint
	"MT_PTERABYTE", // Pterabyte
	"MT_DRAGONBOMBER", // Dragonbomber
	"MT_DRAGONWING", // Dragonbomber wing
	"MT_DRAGONTAIL", // Dragonbomber tail segment
	"MT_DRAGONMINE", // Dragonbomber mine

	// Generic Boss Items
	"MT_BOSSEXPLODE",
	"MT_SONIC3KBOSSEXPLODE",
	"MT_BOSSFLYPOINT",
	"MT_EGGTRAP",
	"MT_BOSS3WAYPOINT",
	"MT_BOSS9GATHERPOINT",
	"MT_BOSSJUNK",

	// Boss 1
	"MT_EGGMOBILE",
	"MT_JETFUME1",
	"MT_EGGMOBILE_BALL",
	"MT_EGGMOBILE_TARGET",
	"MT_EGGMOBILE_FIRE",

	// Boss 2
	"MT_EGGMOBILE2",
	"MT_EGGMOBILE2_POGO",
	"MT_GOOP",
	"MT_GOOPTRAIL",

	// Boss 3
	"MT_EGGMOBILE3",
	"MT_FAKEMOBILE",
	"MT_SHOCKWAVE",

	// Boss 4
	"MT_EGGMOBILE4",
	"MT_EGGMOBILE4_MACE",
	"MT_JETFLAME",
	"MT_EGGROBO1",
	"MT_EGGROBO1JET",

	// Boss 5
	"MT_FANG",
	"MT_BROKENROBOT",
	"MT_VWREF",
	"MT_VWREB",
	"MT_PROJECTORLIGHT",
	"MT_FBOMB",
	"MT_TNTDUST", // also used by barrel
	"MT_FSGNA",
	"MT_FSGNB",
	"MT_FANGWAYPOINT",

	// Black Eggman (Boss 7)
	"MT_BLACKEGGMAN",
	"MT_BLACKEGGMAN_HELPER",
	"MT_BLACKEGGMAN_GOOPFIRE",
	"MT_BLACKEGGMAN_MISSILE",

	// New Very-Last-Minute 2.1 Brak Eggman (Cy-Brak-demon)
	"MT_CYBRAKDEMON",
	"MT_CYBRAKDEMON_ELECTRIC_BARRIER",
	"MT_CYBRAKDEMON_MISSILE",
	"MT_CYBRAKDEMON_FLAMESHOT",
	"MT_CYBRAKDEMON_FLAMEREST",
	"MT_CYBRAKDEMON_TARGET_RETICULE",
	"MT_CYBRAKDEMON_TARGET_DOT",
	"MT_CYBRAKDEMON_NAPALM_BOMB_LARGE",
	"MT_CYBRAKDEMON_NAPALM_BOMB_SMALL",
	"MT_CYBRAKDEMON_NAPALM_FLAMES",
	"MT_CYBRAKDEMON_VILE_EXPLOSION",

	// Metal Sonic (Boss 9)
	"MT_METALSONIC_RACE",
	"MT_METALSONIC_BATTLE",
	"MT_MSSHIELD_FRONT",
	"MT_MSGATHER",

	// Collectible Items
	"MT_RING",
	"MT_FLINGRING", // Lost ring
	"MT_BLUESPHERE",  // Blue sphere for special stages
	"MT_FLINGBLUESPHERE", // Lost blue sphere
	"MT_BOMBSPHERE",
	"MT_REDTEAMRING",  //Rings collectable by red team.
	"MT_BLUETEAMRING", //Rings collectable by blue team.
	"MT_TOKEN", // Special Stage token for special stage
	"MT_REDFLAG", // Red CTF Flag
	"MT_BLUEFLAG", // Blue CTF Flag
	"MT_EMBLEM",
	"MT_EMERALD",
	"MT_EMERALDSPARK",
	"MT_EMERHUNT", // Emerald Hunt
	"MT_EMERALDSPAWN", // Emerald spawner w/ delay

	// Springs and others
	"MT_FAN",
	"MT_STEAM",
	"MT_BUMPER",
	"MT_BALLOON",

	"MT_YELLOWSPRING",
	"MT_REDSPRING",
	"MT_BLUESPRING",
	"MT_GREYSPRING",
	"MT_YELLOWDIAG", // Yellow Diagonal Spring
	"MT_REDDIAG", // Red Diagonal Spring
	"MT_BLUEDIAG", // Blue Diagonal Spring
	"MT_GREYDIAG", // Grey Diagonal Spring
	"MT_YELLOWHORIZ", // Yellow Horizontal Spring
	"MT_REDHORIZ", // Red Horizontal Spring
	"MT_BLUEHORIZ", // Blue Horizontal Spring
	"MT_GREYHORIZ", // Grey Horizontal Spring

	"MT_BOOSTERSEG",
	"MT_BOOSTERROLLER",
	"MT_YELLOWBOOSTER",
	"MT_REDBOOSTER",

	// Interactive Objects
	"MT_BUBBLES", // Bubble source
	"MT_SIGN", // Level end sign
	"MT_SIGN_PIECE",
	"MT_SPIKEBALL", // Spike Ball
	"MT_SPINFIRE",
	"MT_SPIKE",
	"MT_WALLSPIKE",
	"MT_WALLSPIKEBASE",
	"MT_STARPOST",
	"MT_BIGMINE",
	"MT_BLASTEXECUTOR",
	"MT_CANNONLAUNCHER",

	// Monitor miscellany
	"MT_BOXSPARKLE",

	// Monitor boxes -- regular
	"MT_RING_BOX",
	"MT_PITY_BOX",
	"MT_ATTRACT_BOX",
	"MT_FORCE_BOX",
	"MT_ARMAGEDDON_BOX",
	"MT_WHIRLWIND_BOX",
	"MT_ELEMENTAL_BOX",
	"MT_SNEAKERS_BOX",
	"MT_INVULN_BOX",
	"MT_1UP_BOX",
	"MT_EGGMAN_BOX",
	"MT_MIXUP_BOX",
	"MT_MYSTERY_BOX",
	"MT_GRAVITY_BOX",
	"MT_RECYCLER_BOX",
	"MT_SCORE1K_BOX",
	"MT_SCORE10K_BOX",
	"MT_FLAMEAURA_BOX",
	"MT_BUBBLEWRAP_BOX",
	"MT_THUNDERCOIN_BOX",

	// Monitor boxes -- repeating (big) boxes
	"MT_PITY_GOLDBOX",
	"MT_ATTRACT_GOLDBOX",
	"MT_FORCE_GOLDBOX",
	"MT_ARMAGEDDON_GOLDBOX",
	"MT_WHIRLWIND_GOLDBOX",
	"MT_ELEMENTAL_GOLDBOX",
	"MT_SNEAKERS_GOLDBOX",
	"MT_INVULN_GOLDBOX",
	"MT_EGGMAN_GOLDBOX",
	"MT_GRAVITY_GOLDBOX",
	"MT_FLAMEAURA_GOLDBOX",
	"MT_BUBBLEWRAP_GOLDBOX",
	"MT_THUNDERCOIN_GOLDBOX",

	// Monitor boxes -- special
	"MT_RING_REDBOX",
	"MT_RING_BLUEBOX",

	// Monitor icons
	"MT_RING_ICON",
	"MT_PITY_ICON",
	"MT_ATTRACT_ICON",
	"MT_FORCE_ICON",
	"MT_ARMAGEDDON_ICON",
	"MT_WHIRLWIND_ICON",
	"MT_ELEMENTAL_ICON",
	"MT_SNEAKERS_ICON",
	"MT_INVULN_ICON",
	"MT_1UP_ICON",
	"MT_EGGMAN_ICON",
	"MT_MIXUP_ICON",
	"MT_GRAVITY_ICON",
	"MT_RECYCLER_ICON",
	"MT_SCORE1K_ICON",
	"MT_SCORE10K_ICON",
	"MT_FLAMEAURA_ICON",
	"MT_BUBBLEWRAP_ICON",
	"MT_THUNDERCOIN_ICON",

	// Projectiles
	"MT_ROCKET",
	"MT_LASER",
	"MT_TORPEDO",
	"MT_TORPEDO2", // silent
	"MT_ENERGYBALL",
	"MT_MINE", // Skim/Jetty-Syn mine
	"MT_JETTBULLET", // Jetty-Syn Bullet
	"MT_TURRETLASER",
	"MT_CANNONBALL", // Cannonball
	"MT_CANNONBALLDECOR", // Decorative/still cannonball
	"MT_ARROW", // Arrow
	"MT_DEMONFIRE", // Glaregoyle fire

	// The letter
	"MT_LETTER",

	// Greenflower Scenery
	"MT_GFZFLOWER1",
	"MT_GFZFLOWER2",
	"MT_GFZFLOWER3",

	"MT_BLUEBERRYBUSH",
	"MT_BERRYBUSH",
	"MT_BUSH",

	// Trees (both GFZ and misc)
	"MT_GFZTREE",
	"MT_GFZBERRYTREE",
	"MT_GFZCHERRYTREE",
	"MT_CHECKERTREE",
	"MT_CHECKERSUNSETTREE",
	"MT_FHZTREE", // Frozen Hillside
	"MT_FHZPINKTREE",
	"MT_POLYGONTREE",
	"MT_BUSHTREE",
	"MT_BUSHREDTREE",
	"MT_SPRINGTREE",

	// Techno Hill Scenery
	"MT_THZFLOWER1",
	"MT_THZFLOWER2",
	"MT_THZFLOWER3",
	"MT_THZTREE", // Steam whistle tree/bush
	"MT_THZTREEBRANCH", // branch of said tree
	"MT_ALARM",

	// Deep Sea Scenery
	"MT_GARGOYLE", // Deep Sea Gargoyle
	"MT_BIGGARGOYLE", // Deep Sea Gargoyle (Big)
	"MT_SEAWEED", // DSZ Seaweed
	"MT_WATERDRIP", // Dripping Water source
	"MT_WATERDROP", // Water drop from dripping water
	"MT_CORAL1", // Coral
	"MT_CORAL2",
	"MT_CORAL3",
	"MT_CORAL4",
	"MT_CORAL5",
	"MT_BLUECRYSTAL", // Blue Crystal
	"MT_KELP", // Kelp
	"MT_ANIMALGAETOP", // Animated algae top
	"MT_ANIMALGAESEG", // Animated algae segment
	"MT_DSZSTALAGMITE", // Deep Sea 1 Stalagmite
	"MT_DSZ2STALAGMITE", // Deep Sea 2 Stalagmite
	"MT_LIGHTBEAM", // DSZ Light beam

	// Castle Eggman Scenery
	"MT_CHAIN", // CEZ Chain
	"MT_FLAME", // Flame (has corona)
	"MT_FLAMEPARTICLE",
	"MT_EGGSTATUE", // Eggman Statue
	"MT_MACEPOINT", // Mace rotation point
	"MT_CHAINMACEPOINT", // Combination of chains and maces point
	"MT_SPRINGBALLPOINT", // Spring ball point
	"MT_CHAINPOINT", // Mace chain
	"MT_HIDDEN_SLING", // Spin mace chain (activatable)
	"MT_FIREBARPOINT", // Firebar
	"MT_CUSTOMMACEPOINT", // Custom mace
	"MT_SMALLMACECHAIN", // Small Mace Chain
	"MT_BIGMACECHAIN", // Big Mace Chain
	"MT_SMALLMACE", // Small Mace
	"MT_BIGMACE", // Big Mace
	"MT_SMALLGRABCHAIN", // Small Grab Chain
	"MT_BIGGRABCHAIN", // Big Grab Chain
	"MT_YELLOWSPRINGBALL", // Yellow spring on a ball
	"MT_REDSPRINGBALL", // Red spring on a ball
	"MT_SMALLFIREBAR", // Small Firebar
	"MT_BIGFIREBAR", // Big Firebar
	"MT_CEZFLOWER", // Flower
	"MT_CEZPOLE1", // Pole (with red banner)
	"MT_CEZPOLE2", // Pole (with blue banner)
	"MT_CEZBANNER1", // Banner (red)
	"MT_CEZBANNER2", // Banner (blue)
	"MT_PINETREE", // Pine Tree
	"MT_CEZBUSH1", // Bush 1
	"MT_CEZBUSH2", // Bush 2
	"MT_CANDLE", // Candle
	"MT_CANDLEPRICKET", // Candle pricket
	"MT_FLAMEHOLDER", // Flame holder
	"MT_FIRETORCH", // Fire torch
	"MT_WAVINGFLAG1", // Waving flag (red)
	"MT_WAVINGFLAG2", // Waving flag (blue)
	"MT_WAVINGFLAGSEG1", // Waving flag segment (red)
	"MT_WAVINGFLAGSEG2", // Waving flag segment (blue)
	"MT_CRAWLASTATUE", // Crawla statue
	"MT_FACESTABBERSTATUE", // Facestabber statue
	"MT_SUSPICIOUSFACESTABBERSTATUE", // :eggthinking:
	"MT_BRAMBLES", // Brambles

	// Arid Canyon Scenery
	"MT_BIGTUMBLEWEED",
	"MT_LITTLETUMBLEWEED",
	"MT_CACTI1", // Tiny Red Flower Cactus
	"MT_CACTI2", // Small Red Flower Cactus
	"MT_CACTI3", // Tiny Blue Flower Cactus
	"MT_CACTI4", // Small Blue Flower Cactus
	"MT_CACTI5", // Prickly Pear
	"MT_CACTI6", // Barrel Cactus
	"MT_CACTI7", // Tall Barrel Cactus
	"MT_CACTI8", // Armed Cactus
	"MT_CACTI9", // Ball Cactus
	"MT_CACTI10", // Tiny Cactus
	"MT_CACTI11", // Small Cactus
	"MT_CACTITINYSEG", // Tiny Cactus Segment
	"MT_CACTISMALLSEG", // Small Cactus Segment
	"MT_ARIDSIGN_CAUTION", // Caution Sign
	"MT_ARIDSIGN_CACTI", // Cacti Sign
	"MT_ARIDSIGN_SHARPTURN", // Sharp Turn Sign
	"MT_OILLAMP",
	"MT_TNTBARREL",
	"MT_PROXIMITYTNT",
	"MT_DUSTDEVIL",
	"MT_DUSTLAYER",
	"MT_ARIDDUST",
	"MT_MINECART",
	"MT_MINECARTSEG",
	"MT_MINECARTSPAWNER",
	"MT_MINECARTEND",
	"MT_MINECARTENDSOLID",
	"MT_MINECARTSIDEMARK",
	"MT_MINECARTSPARK",
	"MT_SALOONDOOR",
	"MT_SALOONDOORCENTER",
	"MT_TRAINCAMEOSPAWNER",
	"MT_TRAINSEG",
	"MT_TRAINDUSTSPAWNER",
	"MT_TRAINSTEAMSPAWNER",
	"MT_MINECARTSWITCHPOINT",

	// Red Volcano Scenery
	"MT_FLAMEJET",
	"MT_VERTICALFLAMEJET",
	"MT_FLAMEJETFLAME",

	"MT_FJSPINAXISA", // Counter-clockwise
	"MT_FJSPINAXISB", // Clockwise

	"MT_FLAMEJETFLAMEB", // Blade's flame

	"MT_LAVAFALL",
	"MT_LAVAFALL_LAVA",
	"MT_LAVAFALLROCK",

	"MT_ROLLOUTSPAWN",
	"MT_ROLLOUTROCK",

	"MT_BIGFERNLEAF",
	"MT_BIGFERN",
	"MT_JUNGLEPALM",
	"MT_TORCHFLOWER",
	"MT_WALLVINE_LONG",
	"MT_WALLVINE_SHORT",

	// Dark City Scenery

	// Egg Rock Scenery

	// Azure Temple Scenery
	"MT_GLAREGOYLE",
	"MT_GLAREGOYLEUP",
	"MT_GLAREGOYLEDOWN",
	"MT_GLAREGOYLELONG",
	"MT_TARGET", // AKA Red Crystal
	"MT_GREENFLAME",
	"MT_BLUEGARGOYLE",

	// Stalagmites
	"MT_STALAGMITE0",
	"MT_STALAGMITE1",
	"MT_STALAGMITE2",
	"MT_STALAGMITE3",
	"MT_STALAGMITE4",
	"MT_STALAGMITE5",
	"MT_STALAGMITE6",
	"MT_STALAGMITE7",
	"MT_STALAGMITE8",
	"MT_STALAGMITE9",

	// Christmas Scenery
	"MT_XMASPOLE",
	"MT_CANDYCANE",
	"MT_SNOWMAN",    // normal
	"MT_SNOWMANHAT", // with hat + scarf
	"MT_LAMPPOST1",  // normal
	"MT_LAMPPOST2",  // with snow
	"MT_HANGSTAR",
	"MT_MISTLETOE",
	// Xmas GFZ bushes
	"MT_XMASBLUEBERRYBUSH",
	"MT_XMASBERRYBUSH",
	"MT_XMASBUSH",
	// FHZ
	"MT_FHZICE1",
	"MT_FHZICE2",
	"MT_ROSY",
	"MT_CDLHRT",

	// Halloween Scenery
	// Pumpkins
	"MT_JACKO1",
	"MT_JACKO2",
	"MT_JACKO3",
	// Dr Seuss Trees
	"MT_HHZTREE_TOP",
	"MT_HHZTREE_PART",
	// Misc
	"MT_HHZSHROOM",
	"MT_HHZGRASS",
	"MT_HHZTENTACLE1",
	"MT_HHZTENTACLE2",
	"MT_HHZSTALAGMITE_TALL",
	"MT_HHZSTALAGMITE_SHORT",

	// Botanic Serenity scenery
	"MT_BSZTALLFLOWER_RED",
	"MT_BSZTALLFLOWER_PURPLE",
	"MT_BSZTALLFLOWER_BLUE",
	"MT_BSZTALLFLOWER_CYAN",
	"MT_BSZTALLFLOWER_YELLOW",
	"MT_BSZTALLFLOWER_ORANGE",
	"MT_BSZFLOWER_RED",
	"MT_BSZFLOWER_PURPLE",
	"MT_BSZFLOWER_BLUE",
	"MT_BSZFLOWER_CYAN",
	"MT_BSZFLOWER_YELLOW",
	"MT_BSZFLOWER_ORANGE",
	"MT_BSZSHORTFLOWER_RED",
	"MT_BSZSHORTFLOWER_PURPLE",
	"MT_BSZSHORTFLOWER_BLUE",
	"MT_BSZSHORTFLOWER_CYAN",
	"MT_BSZSHORTFLOWER_YELLOW",
	"MT_BSZSHORTFLOWER_ORANGE",
	"MT_BSZTULIP_RED",
	"MT_BSZTULIP_PURPLE",
	"MT_BSZTULIP_BLUE",
	"MT_BSZTULIP_CYAN",
	"MT_BSZTULIP_YELLOW",
	"MT_BSZTULIP_ORANGE",
	"MT_BSZCLUSTER_RED",
	"MT_BSZCLUSTER_PURPLE",
	"MT_BSZCLUSTER_BLUE",
	"MT_BSZCLUSTER_CYAN",
	"MT_BSZCLUSTER_YELLOW",
	"MT_BSZCLUSTER_ORANGE",
	"MT_BSZBUSH_RED",
	"MT_BSZBUSH_PURPLE",
	"MT_BSZBUSH_BLUE",
	"MT_BSZBUSH_CYAN",
	"MT_BSZBUSH_YELLOW",
	"MT_BSZBUSH_ORANGE",
	"MT_BSZVINE_RED",
	"MT_BSZVINE_PURPLE",
	"MT_BSZVINE_BLUE",
	"MT_BSZVINE_CYAN",
	"MT_BSZVINE_YELLOW",
	"MT_BSZVINE_ORANGE",
	"MT_BSZSHRUB",
	"MT_BSZCLOVER",
	"MT_BIG_PALMTREE_TRUNK",
	"MT_BIG_PALMTREE_TOP",
	"MT_PALMTREE_TRUNK",
	"MT_PALMTREE_TOP",

	// Misc scenery
	"MT_DBALL",
	"MT_EGGSTATUE2",

	// Powerup Indicators
	"MT_ELEMENTAL_ORB", // Elemental shield mobj
	"MT_ATTRACT_ORB", // Attract shield mobj
	"MT_FORCE_ORB", // Force shield mobj
	"MT_ARMAGEDDON_ORB", // Armageddon shield mobj
	"MT_WHIRLWIND_ORB", // Whirlwind shield mobj
	"MT_PITY_ORB", // Pity shield mobj
	"MT_FLAMEAURA_ORB", // Flame shield mobj
	"MT_BUBBLEWRAP_ORB", // Bubble shield mobj
	"MT_THUNDERCOIN_ORB", // Thunder shield mobj
	"MT_THUNDERCOIN_SPARK", // Thunder spark
	"MT_IVSP", // Invincibility sparkles
	"MT_SUPERSPARK", // Super Sonic Spark

	// Flickies
	"MT_FLICKY_01", // Bluebird
	"MT_FLICKY_01_CENTER",
	"MT_FLICKY_02", // Rabbit
	"MT_FLICKY_02_CENTER",
	"MT_FLICKY_03", // Chicken
	"MT_FLICKY_03_CENTER",
	"MT_FLICKY_04", // Seal
	"MT_FLICKY_04_CENTER",
	"MT_FLICKY_05", // Pig
	"MT_FLICKY_05_CENTER",
	"MT_FLICKY_06", // Chipmunk
	"MT_FLICKY_06_CENTER",
	"MT_FLICKY_07", // Penguin
	"MT_FLICKY_07_CENTER",
	"MT_FLICKY_08", // Fish
	"MT_FLICKY_08_CENTER",
	"MT_FLICKY_09", // Ram
	"MT_FLICKY_09_CENTER",
	"MT_FLICKY_10", // Puffin
	"MT_FLICKY_10_CENTER",
	"MT_FLICKY_11", // Cow
	"MT_FLICKY_11_CENTER",
	"MT_FLICKY_12", // Rat
	"MT_FLICKY_12_CENTER",
	"MT_FLICKY_13", // Bear
	"MT_FLICKY_13_CENTER",
	"MT_FLICKY_14", // Dove
	"MT_FLICKY_14_CENTER",
	"MT_FLICKY_15", // Cat
	"MT_FLICKY_15_CENTER",
	"MT_FLICKY_16", // Canary
	"MT_FLICKY_16_CENTER",
	"MT_SECRETFLICKY_01", // Spider
	"MT_SECRETFLICKY_01_CENTER",
	"MT_SECRETFLICKY_02", // Bat
	"MT_SECRETFLICKY_02_CENTER",
	"MT_SEED",

	// Environmental Effects
	"MT_RAIN", // Rain
	"MT_SNOWFLAKE", // Snowflake
	"MT_BLIZZARDSNOW", // Blizzard Snowball
	"MT_SPLISH", // Water splish!
	"MT_LAVASPLISH", // Lava splish!
	"MT_SMOKE",
	"MT_SMALLBUBBLE", // small bubble
	"MT_MEDIUMBUBBLE", // medium bubble
	"MT_EXTRALARGEBUBBLE", // extra large bubble
	"MT_WATERZAP",
	"MT_SPINDUST", // Spindash dust
	"MT_TFOG",
	"MT_PARTICLE",
	"MT_PARTICLEGEN", // For fans, etc.

	// Game Indicators
	"MT_SCORE", // score logo
	"MT_DROWNNUMBERS", // Drowning Timer
	"MT_GOTEMERALD", // Chaos Emerald (intangible)
	"MT_LOCKON", // Target
	"MT_LOCKONINF", // In-level Target
	"MT_TAG", // Tag Sign
	"MT_GOTFLAG", // Got Flag sign
	"MT_FINISHFLAG", // Finish flag

	// Ambient Sounds
	"MT_AWATERA", // Ambient Water Sound 1
	"MT_AWATERB", // Ambient Water Sound 2
	"MT_AWATERC", // Ambient Water Sound 3
	"MT_AWATERD", // Ambient Water Sound 4
	"MT_AWATERE", // Ambient Water Sound 5
	"MT_AWATERF", // Ambient Water Sound 6
	"MT_AWATERG", // Ambient Water Sound 7
	"MT_AWATERH", // Ambient Water Sound 8
	"MT_RANDOMAMBIENT",
	"MT_RANDOMAMBIENT2",
	"MT_MACHINEAMBIENCE",

	"MT_CORK",
	"MT_LHRT",

	// Ring Weapons
	"MT_REDRING",
	"MT_BOUNCERING",
	"MT_RAILRING",
	"MT_INFINITYRING",
	"MT_AUTOMATICRING",
	"MT_EXPLOSIONRING",
	"MT_SCATTERRING",
	"MT_GRENADERING",

	"MT_BOUNCEPICKUP",
	"MT_RAILPICKUP",
	"MT_AUTOPICKUP",
	"MT_EXPLODEPICKUP",
	"MT_SCATTERPICKUP",
	"MT_GRENADEPICKUP",

	"MT_THROWNBOUNCE",
	"MT_THROWNINFINITY",
	"MT_THROWNAUTOMATIC",
	"MT_THROWNSCATTER",
	"MT_THROWNEXPLOSION",
	"MT_THROWNGRENADE",

	// Mario-specific stuff
	"MT_COIN",
	"MT_FLINGCOIN",
	"MT_GOOMBA",
	"MT_BLUEGOOMBA",
	"MT_FIREFLOWER",
	"MT_FIREBALL",
	"MT_FIREBALLTRAIL",
	"MT_SHELL",
	"MT_PUMA",
	"MT_PUMATRAIL",
	"MT_HAMMER",
	"MT_KOOPA",
	"MT_KOOPAFLAME",
	"MT_AXE",
	"MT_MARIOBUSH1",
	"MT_MARIOBUSH2",
	"MT_TOAD",

	// NiGHTS Stuff
	"MT_AXIS",
	"MT_AXISTRANSFER",
	"MT_AXISTRANSFERLINE",
	"MT_NIGHTSDRONE",
	"MT_NIGHTSDRONE_MAN",
	"MT_NIGHTSDRONE_SPARKLING",
	"MT_NIGHTSDRONE_GOAL",
	"MT_NIGHTSPARKLE",
	"MT_NIGHTSLOOPHELPER",
	"MT_NIGHTSBUMPER", // NiGHTS Bumper
	"MT_HOOP",
	"MT_HOOPCOLLIDE", // Collision detection for NiGHTS hoops
	"MT_HOOPCENTER", // Center of a hoop
	"MT_NIGHTSCORE",
	"MT_NIGHTSCHIP", // NiGHTS Chip
	"MT_FLINGNIGHTSCHIP", // Lost NiGHTS Chip
	"MT_NIGHTSSTAR", // NiGHTS Star
	"MT_FLINGNIGHTSSTAR", // Lost NiGHTS Star
	"MT_NIGHTSSUPERLOOP",
	"MT_NIGHTSDRILLREFILL",
	"MT_NIGHTSHELPER",
	"MT_NIGHTSEXTRATIME",
	"MT_NIGHTSLINKFREEZE",
	"MT_EGGCAPSULE",
	"MT_IDEYAANCHOR",
	"MT_NIGHTOPIANHELPER", // the actual helper object that orbits you
	"MT_PIAN", // decorative singing friend
	"MT_SHLEEP", // almost-decorative sleeping enemy

	// Secret badniks and hazards, shhhh
	"MT_PENGUINATOR",
	"MT_POPHAT",
	"MT_POPSHOT",
	"MT_POPSHOT_TRAIL",

	"MT_HIVEELEMENTAL",
	"MT_BUMBLEBORE",

	"MT_BUGGLE",

	"MT_SMASHINGSPIKEBALL",
	"MT_CACOLANTERN",
	"MT_CACOSHARD",
	"MT_CACOFIRE",
	"MT_SPINBOBERT",
	"MT_SPINBOBERT_FIRE1",
	"MT_SPINBOBERT_FIRE2",
	"MT_HANGSTER",

	// Utility Objects
	"MT_TELEPORTMAN",
	"MT_ALTVIEWMAN",
	"MT_CRUMBLEOBJ", // Sound generator for crumbling platform
	"MT_TUBEWAYPOINT",
	"MT_PUSH",
	"MT_PULL",
	"MT_GHOST",
	"MT_OVERLAY",
	"MT_ANGLEMAN",
	"MT_POLYANCHOR",
	"MT_POLYSPAWN",

	// Skybox objects
	"MT_SKYBOX",

	// Debris
	"MT_SPARK", //spark
	"MT_EXPLODE", // Robot Explosion
	"MT_UWEXPLODE", // Underwater Explosion
	"MT_DUST",
	"MT_ROCKSPAWNER",
	"MT_FALLINGROCK",
	"MT_ROCKCRUMBLE1",
	"MT_ROCKCRUMBLE2",
	"MT_ROCKCRUMBLE3",
	"MT_ROCKCRUMBLE4",
	"MT_ROCKCRUMBLE5",
	"MT_ROCKCRUMBLE6",
	"MT_ROCKCRUMBLE7",
	"MT_ROCKCRUMBLE8",
	"MT_ROCKCRUMBLE9",
	"MT_ROCKCRUMBLE10",
	"MT_ROCKCRUMBLE11",
	"MT_ROCKCRUMBLE12",
	"MT_ROCKCRUMBLE13",
	"MT_ROCKCRUMBLE14",
	"MT_ROCKCRUMBLE15",
	"MT_ROCKCRUMBLE16",

	// Level debris
	"MT_GFZDEBRIS",
	"MT_BRICKDEBRIS",
	"MT_WOODDEBRIS",
	"MT_REDBRICKDEBRIS",
	"MT_BLUEBRICKDEBRIS",
	"MT_YELLOWBRICKDEBRIS",

	// SRB2kart
	"MT_RANDOMITEM",
	"MT_RANDOMITEMPOP",
	"MT_FLOATINGITEM",

	"MT_SIGNSPARKLE",

	"MT_FASTLINE",
	"MT_FASTDUST",
	"MT_DRIFTEXPLODE",
	"MT_DRIFTCLIP",
	"MT_DRIFTCLIPSPARK",
	"MT_BOOSTFLAME",
	"MT_BOOSTSMOKE",
	"MT_SNEAKERTRAIL",
	"MT_AIZDRIFTSTRAT",
	"MT_SPARKLETRAIL",
	"MT_INVULNFLASH",
	"MT_WIPEOUTTRAIL",
	"MT_DRIFTSPARK",
	"MT_BRAKEDRIFT",
	"MT_DRIFTDUST",

	"MT_ROCKETSNEAKER", // Rocket sneakers

	"MT_EGGMANITEM", // Eggman items
	"MT_EGGMANITEM_SHIELD",

	"MT_BANANA", // Banana Stuff
	"MT_BANANA_SHIELD",

	"MT_ORBINAUT", // Orbinaut stuff
	"MT_ORBINAUT_SHIELD",

	"MT_JAWZ", // Jawz stuff
	"MT_JAWZ_DUD",
	"MT_JAWZ_SHIELD",

	"MT_PLAYERRETICULE", // Jawz reticule

	"MT_SSMINE_SHIELD", // Special Stage Mine stuff
	"MT_SSMINE",
	"MT_MINEEXPLOSION",
	"MT_MINEEXPLOSIONSOUND",

	"MT_SMOLDERING", // New explosion
	"MT_BOOMEXPLODE",
	"MT_BOOMPARTICLE",

	"MT_LANDMINE", // Land Mine

	"MT_BALLHOG", // Ballhog
	"MT_BALLHOGBOOM",

	"MT_SPB", // Self-Propelled Bomb
	"MT_SPBEXPLOSION",

	"MT_THUNDERSHIELD", // Shields
	"MT_BUBBLESHIELD",
	"MT_FLAMESHIELD",
	"MT_FLAMESHIELDUNDERLAY",
	"MT_FLAMESHIELDPAPER",
	"MT_BUBBLESHIELDTRAP",

	"MT_SINK", // Kitchen Sink Stuff
	"MT_SINK_SHIELD",
	"MT_SINKTRAIL",

	"MT_BATTLEBUMPER", // Battle Mode bumper
	"MT_BATTLEBUMPER_DEBRIS",
	"MT_BATTLEBUMPER_BLAST",

	"MT_DEZLASER",

	"MT_WAYPOINT",
	"MT_WAYPOINT_RISER",
	"MT_WAYPOINT_ANCHOR",

	"MT_BOTHINT",

	"MT_RANDOMAUDIENCE",

	"MT_FLAYM",
	"MT_DEVIL",
	"MT_ANGEL",
	"MT_PALMTREE",
	"MT_FLAG",
	"MT_HEDGEHOG",
	"MT_BUSH1",
	"MT_TWEE",
	"MT_HYDRANT",

	"MT_BIGPUMA",
	"MT_APPLE",

	"MT_DOOD_FLOWER1",
	"MT_DOOD_FLOWER2",
	"MT_DOOD_FLOWER3",
	"MT_DOOD_FLOWER4",
	"MT_DOOD_BOX",
	"MT_DOOD_BALLOON",
	"MT_BIGRING",

	"MT_SNES_DONUTBUSH1",
	"MT_SNES_DONUTBUSH2",
	"MT_SNES_DONUTBUSH3",

	"MT_GBA_BOO",

	"MT_BUZZBOMBER",
	"MT_CHOMPER",
	"MT_PALMTREE2",
	"MT_PURPLEFLOWER1",
	"MT_PURPLEFLOWER2",
	"MT_YELLOWFLOWER1",
	"MT_YELLOWFLOWER2",
	"MT_PLANT2",
	"MT_PLANT3",
	"MT_PLANT4",

	"MT_SKULL",
	"MT_PHANTREE",
	"MT_FLYINGGARG",
	"MT_LAMPPOST",
	"MT_MOSSYTREE",

	"MT_BUMP",

	"MT_FLINGENERGY",

	"MT_ITEMCLASH",

	"MT_FIREDITEM",

	"MT_INSTASHIELDA",
	"MT_INSTASHIELDB",

	"MT_PLAYERARROW",
	"MT_PLAYERWANTED",

	"MT_KARMAHITBOX",
	"MT_KARMAWHEEL",

	"MT_BATTLEPOINT",

	"MT_FZEROBOOM",

	// Various plants
	"MT_SONICBUSH",

	// Marble Zone
	"MT_MARBLEFLAMEPARTICLE",
	"MT_MARBLETORCH",
	"MT_MARBLELIGHT",
	"MT_MARBLEBURNER",

	// CD Special Stage
	"MT_CDUFO",

	// Rusty Rig
	"MT_RUSTYLAMP_ORANGE",
	"MT_RUSTYCHAIN",

	// Smokin' & Vapin' (Don't try this at home, kids!)
	"MT_PETSMOKER",
	"MT_PETSMOKE",
	"MT_VVVAPE",

	// Hill Top Zone
	"MT_HTZTREE",
	"MT_HTZBUSH",

	// Ports of gardens
	"MT_SGVINE1",
	"MT_SGVINE2",
	"MT_SGVINE3",
	"MT_PGTREE",
	"MT_PGFLOWER1",
	"MT_PGFLOWER2",
	"MT_PGFLOWER3",
	"MT_PGBUSH",
	"MT_DHPILLAR",

	// Midnight Channel stuff:
	"MT_SPOTLIGHT",		// Spotlight Object
	"MT_RANDOMSHADOW",	// Random static Shadows.
	"MT_ROAMINGSHADOW",	// Roaming Shadows.
	"MT_MAYONAKAARROW",	// Arrow static signs for Mayonaka

	// Mementos stuff
	"MT_REAPERWAYPOINT",
	"MT_REAPER",
	"MT_MEMENTOSTP",
	"MT_MEMENTOSPARTICLE",

	"MT_JACKINTHEBOX",

	// Color Drive:
	"MT_CDMOON",
	"MT_CDBUSH",
	"MT_CDTREEA",
	"MT_CDTREEB",

	// Daytona Speedway
	"MT_DAYTONAPINETREE",
	"MT_DAYTONAPINETREE_SIDE",

	// Egg Zeppelin
	"MT_EZZPROPELLER",
	"MT_EZZPROPELLER_BLADE",

	// Desert Palace
	"MT_DP_PALMTREE",

	// Aurora Atoll
	"MT_AAZTREE_HELPER",
	"MT_AAZTREE_SEG",
	"MT_AAZTREE_COCONUT",
	"MT_AAZTREE_LEAF",

	// Barren Badlands
	"MT_BBZDUST",
	"MT_FROGGER",
	"MT_FROGTONGUE",
	"MT_FROGTONGUE_JOINT",
	"MT_ROBRA",
	"MT_ROBRA_HEAD",
	"MT_ROBRA_JOINT",
	"MT_BLUEROBRA",
	"MT_BLUEROBRA_HEAD",
	"MT_BLUEROBRA_JOINT",

	// Eerie Grove
	"MT_EERIEFOG",
	"MT_EERIEFOGGEN",

	// SMK ports
	"MT_SMK_PIPE",
	"MT_SMK_MOLESPAWNER",
	"MT_SMK_MOLE",
	"MT_SMK_THWOMP",
	"MT_SMK_SNOWBALL",
	"MT_SMK_ICEBLOCK",
	"MT_SMK_ICEBLOCK_SIDE",
	"MT_SMK_ICEBLOCK_DEBRIS",

	// Ezo's maps
	"MT_BLUEFIRE",
	"MT_GREENFIRE",
	"MT_REGALCHEST",
	"MT_CHIMERASTATUE",
	"MT_DRAGONSTATUE",
	"MT_LIZARDMANSTATUE",
	"MT_PEGASUSSTATUE",
	"MT_ZELDAFIRE",
	"MT_GANBARETHING",
	"MT_GANBAREDUCK",
	"MT_GANBARETREE",
	"MT_MONOKUMA",
	"MT_REDZELDAFIRE",
	"MT_BOWLINGPIN",
	"MT_MERRYAMBIENCE",
	"MT_TWINKLECARTAMBIENCE",
	"MT_EXPLODINGBARREL",
	"MT_MERRYHORSE",
	"MT_BLUEFRUIT",
	"MT_ORANGEFRUIT",
	"MT_REDFRUIT",
	"MT_PINKFRUIT",
	"MT_ADVENTURESPIKEA",
	"MT_ADVENTURESPIKEB",
	"MT_ADVENTURESPIKEC",
	"MT_BOOSTPROMPT",
	"MT_BOOSTOFF",
	"MT_BOOSTON",
	"MT_ARIDTOAD",
	"MT_LIZARDMAN",
	"MT_LIONMAN",

	"MT_KARMAFIREWORK",
	"MT_RINGSPARKS",
	"MT_DRAFTDUST",
	"MT_SPBDUST",

	"MT_OVERTIME_PARTICLE",
	"MT_OVERTIME_CENTER",

	"MT_BATTLECAPSULE",
	"MT_BATTLECAPSULE_PIECE",

	"MT_FOLLOWER",
	"MT_FOLLOWERBUBBLE_FRONT",
	"MT_FOLLOWERBUBBLE_BACK",

	"MT_WATERTRAIL",
	"MT_WATERTRAILUNDERLAY",

	"MT_SPINDASHDUST",
	"MT_SPINDASHWIND",

	"MT_PAPERITEMSPOT",

#ifdef SEENAMES
	"MT_NAMECHECK",
#endif
};

static const char *const MOBJFLAG_LIST[] = {
	"SPECIAL",
	"SOLID",
	"SHOOTABLE",
	"NOSECTOR",
	"NOBLOCKMAP",
	"PAPERCOLLISION",
	"PUSHABLE",
	"BOSS",
	"SPAWNCEILING",
	"NOGRAVITY",
	"AMBIENT",
	"SLIDEME",
	"NOCLIP",
	"FLOAT",
	"BOXICON",
	"MISSILE",
	"SPRING",
	"MONITOR",
	"NOTHINK",
	"NOCLIPHEIGHT",
	"ENEMY",
	"SCENERY",
	"PAIN",
	"STICKY",
	"NIGHTSITEM",
	"NOCLIPTHING",
	"GRENADEBOUNCE",
	"RUNSPAWNFUNC",
	"DONTENCOREMAP",
	NULL
};

// \tMF2_(\S+).*// (.+) --> \t"\1", // \2
static const char *const MOBJFLAG2_LIST[] = {
	"AXIS",			  // It's a NiGHTS axis! (For faster checking)
	"TWOD",			  // Moves like it's in a 2D level
	"DONTRESPAWN",	  // Don't respawn this object!
	"\x01",			  // free: 1<<3 (name un-matchable)
	"AUTOMATIC",	  // Thrown ring has automatic properties
	"RAILRING",		  // Thrown ring has rail properties
	"BOUNCERING",	  // Thrown ring has bounce properties
	"EXPLOSION",	  // Thrown ring has explosive properties
	"SCATTER",		  // Thrown ring has scatter properties
	"BEYONDTHEGRAVE", // Source of this missile has died and has since respawned.
	"SLIDEPUSH",	  // MF_PUSHABLE that pushes continuously.
	"CLASSICPUSH",    // Drops straight down when object has negative momz.
	"INVERTAIMABLE",  // Flips whether it's targetable by A_LookForEnemies (enemies no, decoys yes)
	"INFLOAT",		  // Floating to a height for a move, don't auto float to target's height.
	"DEBRIS",		  // Splash ring from explosion ring
	"NIGHTSPULL",	  // Attracted from a paraloop
	"JUSTATTACKED",	  // can be pushed by other moving mobjs
	"FIRING",		  // turret fire
	"SUPERFIRE",	  // Firing something with Super Sonic-stopping properties. Or, if mobj has MF_MISSILE, this is the actual fire from it.
	"\x01",			  // free: 1<<20 (name un-matchable)
	"STRONGBOX",	  // Flag used for "strong" random monitors.
	"OBJECTFLIP",	  // Flag for objects that always have flipped gravity.
	"SKULLFLY",		  // Special handling: skull in flight.
	"FRET",			  // Flashing from a previous hit
	"BOSSNOTRAP",	  // No Egg Trap after boss
	"BOSSFLEE",		  // Boss is fleeing!
	"BOSSDEAD",		  // Boss is dead! (Not necessarily fleeing, if a fleeing point doesn't exist.)
	"AMBUSH",         // Alternate behaviour typically set by MTF_AMBUSH
	"LINKDRAW",       // Draw vissprite of mobj immediately before/after tracer's vissprite (dependent on dispoffset and position)
	"SHIELD",         // Thinker calls P_AddShield/P_ShieldLook (must be partnered with MF_SCENERY to use)
	NULL
};

static const char *const MOBJEFLAG_LIST[] = {
	"ONGROUND", // The mobj stands on solid floor (not on another mobj or in air)
	"JUSTHITFLOOR", // The mobj just hit the floor while falling, this is cleared on next frame
	"TOUCHWATER", // The mobj stands in a sector with water, and touches the surface
	"UNDERWATER", // The mobj stands in a sector with water, and his waist is BELOW the water surface
	"JUSTSTEPPEDDOWN", // used for ramp sectors
	"VERTICALFLIP", // Vertically flip sprite/allow upside-down physics
	"GOOWATER", // Goo water
	"TOUCHLAVA", // The mobj is touching a lava block
	"PUSHED", // Mobj was already pushed this tic
	"SPRUNG", // Mobj was already sprung this tic
	"APPLYPMOMZ", // Platform movement
	"TRACERANGLE", // Compute and trigger on mobj angle relative to tracer
	"JUSTBOUNCEDWALL",
	NULL
};

static const char *const MAPTHINGFLAG_LIST[4] = {
	"EXTRA", // Extra flag for objects.
	"OBJECTFLIP", // Reverse gravity flag for objects.
	"OBJECTSPECIAL", // Special flag used with certain objects.
	"AMBUSH" // Deaf monsters/do not react to sound.
};

static const char *const PLAYERFLAG_LIST[] = {
	"FAULT",
	"ANALOGMODE", // Analog mode?
	"DIRECTIONCHAR", // Directional character sprites?
	"AUTOBRAKE", // Autobrake?

	// Cheats
	"GODMODE",
	"NOCLIP",
	"INVIS",

	// True if button down last tic.
	"ATTACKDOWN",
	"SPINDOWN",
	"JUMPDOWN",
	"WPNDOWN",

	// Unmoving states
	"STASIS", // Player is not allowed to move
	"JUMPSTASIS", // and that includes jumping.
	// (we don't include FULLSTASIS here I guess because it's just those two together...?)

	// Did you get a time-over?
	"TIMEOVER",

	// SRB2Kart: spectator that wants to join
	"WANTSTOJOIN",

	// Character action status
	"STARTJUMP",
	"JUMPED",
	"NOJUMPDAMAGE",

	"SPINNING",
	"STARTDASH",

	"THOKKED",
	"SHIELDABILITY",
	"GLIDING",
	"BOUNCING",

	// Sliding (usually in water) like Labyrinth/Oil Ocean
	"SLIDING",

	// NiGHTS stuff
	"TRANSFERTOCLOSEST",
	"DRILLING",

	// Gametype-specific stuff
	"GAMETYPEOVER", // Race time over, or H&S out-of-game
	"TAGIT", // The player is it! For Tag Mode

	/*** misc ***/
	"FORCESTRAFE", // Translate turn inputs into strafe inputs
	"HITFINISHLINE", // Already hit the finish line this tic
	"FINISHED",

	NULL // stop loop here.
};

static const char *const GAMETYPERULE_LIST[] = {
	"CAMPAIGN",
	"RINGSLINGER",
	"SPECTATORS",
	"LIVES",
	"TEAMS",
	"FIRSTPERSON",
	"POWERSTONES",
	"TEAMFLAGS",
	"FRIENDLY",
	"SPECIALSTAGES",
	"EMERALDTOKENS",
	"EMERALDHUNT",
	"RACE",
	"TAG",
	"POINTLIMIT",
	"TIMELIMIT",
	"OVERTIME",
	"HURTMESSAGES",
	"FRIENDLYFIRE",
	"STARTCOUNTDOWN",
	"HIDEFROZEN",
	"BLINDFOLDED",
	"RESPAWNDELAY",
	"PITYSHIELD",
	"DEATHPENALTY",
	"NOSPECTATORSPAWN",
	"DEATHMATCHSTARTS",
	"SPAWNINVUL",
	"SPAWNENEMIES",
	"ALLOWEXIT",
	"NOTITLECARD",
	"CUTSCENES",
	NULL
};

// Linedef flags
static const char *const ML_LIST[16] = {
	"IMPASSABLE",
	"BLOCKPLAYERS",
	"TWOSIDED",
	"DONTPEGTOP",
	"DONTPEGBOTTOM",
	"EFFECT1",
	"NOCLIMB",
	"EFFECT2",
	"EFFECT3",
	"EFFECT4",
	"EFFECT5",
	"NOSONIC",
	"NOTAILS",
	"NOKNUX",
	"BOUNCY",
	"TFERLINE"
};

// This DOES differ from r_draw's Color_Names, unfortunately.
// Also includes Super colors
static const char *COLOR_ENUMS[] = { // Rejigged for Kart.
	"NONE",			// SKINCOLOR_NONE
	"WHITE",		// SKINCOLOR_WHITE
	"SILVER",		// SKINCOLOR_SILVER
	"GREY",			// SKINCOLOR_GREY
	"NICKEL",		// SKINCOLOR_NICKEL
	"BLACK",		// SKINCOLOR_BLACK
	"SKUNK",		// SKINCOLOR_SKUNK
	"FAIRY",		// SKINCOLOR_FAIRY
	"POPCORN",		// SKINCOLOR_POPCORN
	"ARTICHOKE",	// SKINCOLOR_ARTICHOKE
	"PIGEON",		// SKINCOLOR_PIGEON
	"SEPIA",		// SKINCOLOR_SEPIA
	"BEIGE",		// SKINCOLOR_BEIGE
	"CARAMEL",		// SKINCOLOR_CARAMEL
	"PEACH",		// SKINCOLOR_PEACH
	"BROWN",		// SKINCOLOR_BROWN
	"LEATHER",		// SKINCOLOR_LEATHER
	"PINK",			// SKINCOLOR_PINK
	"ROSE",			// SKINCOLOR_ROSE
	"CINNAMON",		// SKINCOLOR_CINNAMON
	"RUBY",			// SKINCOLOR_RUBY
	"RASPBERRY",	// SKINCOLOR_RASPBERRY
	"RED",			// SKINCOLOR_RED
	"CRIMSON",		// SKINCOLOR_CRIMSON
	"MAROON",		// SKINCOLOR_MAROON
	"LEMONADE",		// SKINCOLOR_LEMONADE
	"SCARLET",		// SKINCOLOR_SCARLET
	"KETCHUP",		// SKINCOLOR_KETCHUP
	"DAWN",			// SKINCOLOR_DAWN
	"SUNSLAM",		// SKINCOLOR_SUNSLAM
	"CREAMSICLE",	// SKINCOLOR_CREAMSICLE
	"ORANGE",		// SKINCOLOR_ORANGE
	"ROSEWOOD",		// SKINCOLOR_ROSEWOOD
	"TANGERINE",	// SKINCOLOR_TANGERINE
	"TAN",			// SKINCOLOR_TAN
	"CREAM",		// SKINCOLOR_CREAM
	"GOLD",			// SKINCOLOR_GOLD
	"ROYAL",		// SKINCOLOR_ROYAL
	"BRONZE",		// SKINCOLOR_BRONZE
	"COPPER",		// SKINCOLOR_COPPER
	"YELLOW",		// SKINCOLOR_YELLOW
	"MUSTARD",		// SKINCOLOR_MUSTARD
	"BANANA",		// SKINCOLOR_BANANA
	"OLIVE",		// SKINCOLOR_OLIVE
	"CROCODILE",	// SKINCOLOR_CROCODILE
	"PERIDOT",		// SKINCOLOR_PERIDOT
	"VOMIT",		// SKINCOLOR_VOMIT
	"GARDEN",		// SKINCOLOR_GARDEN
	"LIME",			// SKINCOLOR_LIME
	"HANDHELD",		// SKINCOLOR_HANDHELD
	"TEA",			// SKINCOLOR_TEA
	"PISTACHIO",	// SKINCOLOR_PISTACHIO
	"MOSS",			// SKINCOLOR_MOSS
	"CAMOUFLAGE",	// SKINCOLOR_CAMOUFLAGE
	"MINT",			// SKINCOLOR_MINT
	"GREEN",		// SKINCOLOR_GREEN
	"PINETREE",		// SKINCOLOR_PINETREE
	"TURTLE",		// SKINCOLOR_TURTLE
	"SWAMP",		// SKINCOLOR_SWAMP
	"DREAM",		// SKINCOLOR_DREAM
	"PLAGUE",		// SKINCOLOR_PLAGUE
	"EMERALD",		// SKINCOLOR_EMERALD
	"ALGAE",		// SKINCOLOR_ALGAE
	"AQUAMARINE",	// SKINCOLOR_AQUAMARINE
	"TURQUOISE",	// SKINCOLOR_TURQUOISE
	"TEAL",			// SKINCOLOR_TEAL
	"ROBIN",		// SKINCOLOR_ROBIN
	"CYAN",			// SKINCOLOR_CYAN
	"JAWZ",			// SKINCOLOR_JAWZ
	"CERULEAN",		// SKINCOLOR_CERULEAN
	"NAVY",			// SKINCOLOR_NAVY
	"PLATINUM",		// SKINCOLOR_PLATINUM
	"SLATE",		// SKINCOLOR_SLATE
	"STEEL",		// SKINCOLOR_STEEL
	"THUNDER",		// SKINCOLOR_THUNDER
	"NOVA",			// SKINCOLOR_NOVA
	"RUST",			// SKINCOLOR_RUST
	"WRISTWATCH",	// SKINCOLOR_WRISTWATCH
	"JET",			// SKINCOLOR_JET
	"SAPPHIRE",		// SKINCOLOR_SAPPHIRE
	"ULTRAMARINE",	// SKINCOLOR_ULTRAMARINE
	"PERIWINKLE",	// SKINCOLOR_PERIWINKLE
	"BLUE",			// SKINCOLOR_BLUE
	"MIDNIGHT",		// SKINCOLOR_MIDNIGHT
	"BLUEBERRY",	// SKINCOLOR_BLUEBERRY
	"THISTLE",		// SKINCOLOR_THISTLE
	"PURPLE",		// SKINCOLOR_PURPLE
	"PASTEL",		// SKINCOLOR_PASTEL
	"MOONSET",		// SKINCOLOR_MOONSET
	"DUSK",			// SKINCOLOR_DUSK
	"VIOLET",		// SKINCOLOR_VIOLET
	"MAGENTA",		// SKINCOLOR_MAGENTA
	"FUCHSIA",		// SKINCOLOR_FUCHSIA
	"TOXIC",		// SKINCOLOR_TOXIC
	"MAUVE",		// SKINCOLOR_MAUVE
	"LAVENDER",		// SKINCOLOR_LAVENDER
	"BYZANTIUM",	// SKINCOLOR_BYZANTIUM
	"POMEGRANATE",	// SKINCOLOR_POMEGRANATE
	"LILAC",		// SKINCOLOR_LILAC
	"BLOSSOM",		// SKINCOLOR_BLOSSOM
	"TAFFY",		// SKINCOLOR_TAFFY

	// Special super colors
	// Super Sonic Yellow
	"SUPERSILVER1",
	"SUPERSILVER2",
	"SUPERSILVER3",
	"SUPERSILVER4",
	"SUPERSILVER5",

	"SUPERRED1",
	"SUPERRED2",
	"SUPERRED3",
	"SUPERRED4",
	"SUPERRED5",

	"SUPERORANGE1",
	"SUPERORANGE2",
	"SUPERORANGE3",
	"SUPERORANGE4",
	"SUPERORANGE5",

	"SUPERGOLD1",
	"SUPERGOLD2",
	"SUPERGOLD3",
	"SUPERGOLD4",
	"SUPERGOLD5",

	"SUPERPERIDOT1",
	"SUPERPERIDOT2",
	"SUPERPERIDOT3",
	"SUPERPERIDOT4",
	"SUPERPERIDOT5",

	"SUPERSKY1",
	"SUPERSKY2",
	"SUPERSKY3",
	"SUPERSKY4",
	"SUPERSKY5",

	"SUPERPURPLE1",
	"SUPERPURPLE2",
	"SUPERPURPLE3",
	"SUPERPURPLE4",
	"SUPERPURPLE5",

	"SUPERRUST1",
	"SUPERRUST2",
	"SUPERRUST3",
	"SUPERRUST4",
	"SUPERRUST5",

	"SUPERTAN1",
	"SUPERTAN2",
	"SUPERTAN3",
	"SUPERTAN4",
	"SUPERTAN5",

	"CHAOSEMERALD1",
	"CHAOSEMERALD2",
	"CHAOSEMERALD3",
	"CHAOSEMERALD4",
	"CHAOSEMERALD5",
	"CHAOSEMERALD6",
	"CHAOSEMERALD7"
};

static const char *const POWERS_LIST[] = {
	"INVULNERABILITY",
	"SNEAKERS",
	"FLASHING",
	"SHIELD",
	"CARRY",
	"TAILSFLY", // tails flying
	"UNDERWATER", // underwater timer
	"SPACETIME", // In space, no one can hear you spin!
	"EXTRALIFE", // Extra Life timer
	"PUSHING",
	"JUSTSPRUNG",
	"NOAUTOBRAKE",

	"SUPER", // Are you super?
	"GRAVITYBOOTS", // gravity boots

	// Weapon ammunition
	"INFINITYRING",
	"AUTOMATICRING",
	"BOUNCERING",
	"SCATTERRING",
	"GRENADERING",
	"EXPLOSIONRING",
	"RAILRING",

	// Power Stones
	"EMERALDS", // stored like global 'emeralds' variable

	// NiGHTS powerups
	"NIGHTS_SUPERLOOP",
	"NIGHTS_HELPER",
	"NIGHTS_LINKFREEZE",

	//for linedef exec 427
	"NOCONTROL",

	//for dyes
	"DYE",

	"JUSTLAUNCHED",

	"IGNORELATCH"
};

static const char *const KARTSTUFF_LIST[] = {
	"POSITION",
	"OLDPOSITION",
	"POSITIONDELAY",

	"THROWDIR",
	"INSTASHIELD",

	"FLOORBOOST",
	"SPINOUTTYPE",

	"DRIFT",
	"DRIFTEND",
	"DRIFTCHARGE",
	"DRIFTBOOST",
	"BOOSTCHARGE",
	"STARTBOOST",
	"RINGS",
	"PICKUPRINGS",
	"USERINGS",
	"RINGDELAY",
	"RINGBOOST",
	"RINGLOCK",
	"SPARKLEANIM",
	"JMP",
	"OFFROAD",
	"POGOSPRING",
	"POGOSIDEMOVE",
	"SPINDASH",
	"SPINDASHSPEED",
	"SPINDASHBOOST",
	"WATERSKIP",
	"DASHPADCOOLDOWN",
	"NUMBOOSTS",
	"BOOSTPOWER",
	"SPEEDBOOST",
	"ACCELBOOST",
	"HANDLEBOOST",
	"DRAFTPOWER",
	"DRAFTLEEWAY",
	"LASTDRAFT",
	"BOOSTANGLE",
	"AIZDRIFTSTRAT",
	"BRAKEDRIFT",

	"ITEMROULETTE",
	"ROULETTETYPE",

	"ITEMTYPE",
	"ITEMAMOUNT",
	"ITEMHELD",
	"HOLDREADY",

	"CURSHIELD",
	"HYUDOROTIMER",
	"STEALINGTIMER",
	"STOLENTIMER",
	"SUPERRING",
	"SNEAKERTIMER",
	"NUMSNEAKERS",
	"GROWSHRINKTIMER",
	"SQUISHEDTIMER",
	"ROCKETSNEAKERTIMER",
	"INVINCIBILITYTIMER",
	"BUBBLECOOL",
	"BUBBLEBLOWUP",
	"FLAMEDASH",
	"FLAMEMETER",
	"FLAMELENGTH",
	"EGGMANHELD",
	"EGGMANEXPLODE",
	"EGGMANBLAME",
	"LASTJAWZTARGET",
	"BANANADRAG",
	"SPINOUTTIMER",
	"WIPEOUTSLOW",
	"JUSTBUMPED",
	"COMEBACKTIMER",
	"SADTIMER",

	"BUMPER",
	"COMEBACKPOINTS",
	"COMEBACKMODE",
	"WANTED",

	"GETSPARKS",
	"JAWZTARGETDELAY",
	"SPECTATEWAIT",
	"SPRINGSTARS",
	"SPRINGCOLOR",
	"KILLFIELD",
	"WRONGWAY"
};

static const char *const HUDITEMS_LIST[] = {
	"LIVES",

	"RINGS",
	"RINGSNUM",
	"RINGSNUMTICS",

	"SCORE",
	"SCORENUM",

	"TIME",
	"MINUTES",
	"TIMECOLON",
	"SECONDS",
	"TIMETICCOLON",
	"TICS",

	"SS_TOTALRINGS",

	"GETRINGS",
	"GETRINGSNUM",
	"TIMELEFT",
	"TIMELEFTNUM",
	"TIMEUP",
	"HUNTPICS",
	"POWERUPS"
};

static const char *const MENUTYPES_LIST[] = {
	"NONE",

	"MAIN",

	// Single Player
	"SP_MAIN",

	"SP_LOAD",
	"SP_PLAYER",

	"SP_LEVELSELECT",
	"SP_LEVELSTATS",

	"SP_TIMEATTACK",
	"SP_TIMEATTACK_LEVELSELECT",
	"SP_GUESTREPLAY",
	"SP_REPLAY",
	"SP_GHOST",

	"SP_NIGHTSATTACK",
	"SP_NIGHTS_LEVELSELECT",
	"SP_NIGHTS_GUESTREPLAY",
	"SP_NIGHTS_REPLAY",
	"SP_NIGHTS_GHOST",

	// Multiplayer
	"MP_MAIN",
	"MP_SPLITSCREEN", // SplitServer
	"MP_SERVER",
	"MP_CONNECT",
	"MP_ROOM",
	"MP_PLAYERSETUP", // MP_PlayerSetupDef shared with SPLITSCREEN if #defined NONET
	"MP_SERVER_OPTIONS",

	// Options
	"OP_MAIN",

	"OP_P1CONTROLS",
	"OP_CHANGECONTROLS", // OP_ChangeControlsDef shared with P2
	"OP_P1MOUSE",
	"OP_P1JOYSTICK",
	"OP_JOYSTICKSET", // OP_JoystickSetDef shared with P2
	"OP_P1CAMERA",

	"OP_P2CONTROLS",
	"OP_P2MOUSE",
	"OP_P2JOYSTICK",
	"OP_P2CAMERA",

	"OP_PLAYSTYLE",

	"OP_VIDEO",
	"OP_VIDEOMODE",
	"OP_COLOR",
	"OP_OPENGL",
	"OP_OPENGL_LIGHTING",

	"OP_SOUND",

	"OP_SERVER",
	"OP_MONITORTOGGLE",

	"OP_DATA",
	"OP_ADDONS",
	"OP_SCREENSHOTS",
	"OP_ERASEDATA",

	// Extras
	"SR_MAIN",
	"SR_PANDORA",
	"SR_LEVELSELECT",
	"SR_UNLOCKCHECKLIST",
	"SR_EMBLEMHINT",
	"SR_PLAYER",
	"SR_SOUNDTEST",

	// Addons (Part of MISC, but let's make it our own)
	"AD_MAIN",

	// MISC
	// "MESSAGE",
	// "SPAUSE",

	// "MPAUSE",
	// "SCRAMBLETEAM",
	// "CHANGETEAM",
	// "CHANGELEVEL",

	// "MAPAUSE",
	// "HELP",

	"SPECIAL"
};

struct {
	const char *n;
	// has to be able to hold both fixed_t and angle_t, so drastic measure!!
	lua_Integer v;
} const INT_CONST[] = {
	// If a mod removes some variables here,
	// please leave the names in-tact and just set
	// the value to 0 or something.

	// integer type limits, from doomtype.h
	// INT64 and UINT64 limits not included, they're too big for most purposes anyway
	// signed
	{"INT8_MIN",INT8_MIN},
	{"INT16_MIN",INT16_MIN},
	{"INT32_MIN",INT32_MIN},
	{"INT8_MAX",INT8_MAX},
	{"INT16_MAX",INT16_MAX},
	{"INT32_MAX",INT32_MAX},
	// unsigned
	{"UINT8_MAX",UINT8_MAX},
	{"UINT16_MAX",UINT16_MAX},
	{"UINT32_MAX",UINT32_MAX},

	// fixed_t constants, from m_fixed.h
	{"FRACUNIT",FRACUNIT},
	{"FRACBITS",FRACBITS},

	// doomdef.h constants
	{"TICRATE",TICRATE},
	{"MUSICRATE",MUSICRATE},
	{"RING_DIST",RING_DIST},
	{"PUSHACCEL",PUSHACCEL},
	{"MODID",MODID}, // I don't know, I just thought it would be cool for a wad to potentially know what mod it was loaded into.
	{"MODVERSION",MODVERSION}, // or what version of the mod this is.
	{"CODEBASE",CODEBASE}, // or what release of SRB2 this is.
	{"NEWTICRATE",NEWTICRATE}, // TICRATE*NEWTICRATERATIO
	{"NEWTICRATERATIO",NEWTICRATERATIO},

	// Special linedef executor tag numbers!
	{"LE_PINCHPHASE",LE_PINCHPHASE}, // A boss entered pinch phase (and, in most cases, is preparing their pinch phase attack!)
	{"LE_ALLBOSSESDEAD",LE_ALLBOSSESDEAD}, // All bosses in the map are dead (Egg capsule raise)
	{"LE_BOSSDEAD",LE_BOSSDEAD}, // A boss in the map died (Chaos mode boss tally)
	{"LE_BOSS4DROP",LE_BOSS4DROP}, // CEZ boss dropped its cage
	{"LE_BRAKVILEATACK",LE_BRAKVILEATACK}, // Brak's doing his LOS attack, oh noes
	{"LE_TURRET",LE_TURRET}, // THZ turret
	{"LE_BRAKPLATFORM",LE_BRAKPLATFORM}, // v2.0 Black Eggman destroys platform
	{"LE_CAPSULE2",LE_CAPSULE2}, // Egg Capsule
	{"LE_CAPSULE1",LE_CAPSULE1}, // Egg Capsule
	{"LE_CAPSULE0",LE_CAPSULE0}, // Egg Capsule
	{"LE_KOOPA",LE_KOOPA}, // Distant cousin to Gay Bowser
	{"LE_AXE",LE_AXE}, // MKB Axe object
	{"LE_PARAMWIDTH",LE_PARAMWIDTH},  // If an object that calls LinedefExecute has a nonzero parameter value, this times the parameter will be subtracted. (Mostly for the purpose of coexisting bosses...)

	/// \todo Get all this stuff into its own sections, maybe. Maybe.

	// Frame settings
	{"FF_FRAMEMASK",FF_FRAMEMASK},
	{"FF_SPR2SUPER",FF_SPR2SUPER},
	{"FF_SPR2ENDSTATE",FF_SPR2ENDSTATE},
	{"FF_SPR2MIDSTART",FF_SPR2MIDSTART},
	{"FF_ANIMATE",FF_ANIMATE},
	{"FF_RANDOMANIM",FF_RANDOMANIM},
	{"FF_GLOBALANIM",FF_GLOBALANIM},
	{"FF_FULLBRIGHT",FF_FULLBRIGHT},
	{"FF_SEMIBRIGHT",FF_SEMIBRIGHT},
	{"FF_PAPERSPRITE",FF_PAPERSPRITE},
	{"FF_VERTICALFLIP",FF_VERTICALFLIP},
	{"FF_HORIZONTALFLIP",FF_HORIZONTALFLIP},
	{"FF_PAPERSPRITE",FF_PAPERSPRITE},
	{"FF_TRANSMASK",FF_TRANSMASK},
	{"FF_TRANSSHIFT",FF_TRANSSHIFT},
	// new preshifted translucency (used in source)
	{"FF_TRANS10",FF_TRANS10},
	{"FF_TRANS20",FF_TRANS20},
	{"FF_TRANS30",FF_TRANS30},
	{"FF_TRANS40",FF_TRANS40},
	{"FF_TRANS50",FF_TRANS50},
	{"FF_TRANS60",FF_TRANS60},
	{"FF_TRANS70",FF_TRANS70},
	{"FF_TRANS80",FF_TRANS80},
	{"FF_TRANS90",FF_TRANS90},
	{"FF_TRANSADD",FF_TRANSADD},
	{"FF_TRANSSUB",FF_TRANSSUB},
	// compatibility
	// Transparency for SOCs is pre-shifted
	{"TR_TRANS10",tr_trans10<<FF_TRANSSHIFT},
	{"TR_TRANS20",tr_trans20<<FF_TRANSSHIFT},
	{"TR_TRANS30",tr_trans30<<FF_TRANSSHIFT},
	{"TR_TRANS40",tr_trans40<<FF_TRANSSHIFT},
	{"TR_TRANS50",tr_trans50<<FF_TRANSSHIFT},
	{"TR_TRANS60",tr_trans60<<FF_TRANSSHIFT},
	{"TR_TRANS70",tr_trans70<<FF_TRANSSHIFT},
	{"TR_TRANS80",tr_trans80<<FF_TRANSSHIFT},
	{"TR_TRANS90",tr_trans90<<FF_TRANSSHIFT},
	{"TR_TRANSADD",tr_transadd<<FF_TRANSSHIFT},
	{"TR_TRANSSUB",tr_transsub<<FF_TRANSSHIFT},
	// Transparency for Lua is not, unless capitalized as above.
	{"tr_trans10",tr_trans10},
	{"tr_trans20",tr_trans20},
	{"tr_trans30",tr_trans30},
	{"tr_trans40",tr_trans40},
	{"tr_trans50",tr_trans50},
	{"tr_trans60",tr_trans60},
	{"tr_trans70",tr_trans70},
	{"tr_trans80",tr_trans80},
	{"tr_trans90",tr_trans90},
	{"NUMTRANSMAPS",NUMTRANSMAPS},
	{"tr_transadd",tr_transadd},
	{"tr_transsub",tr_transsub},
	{"NUMEFFECTMAPS",NUMEFFECTMAPS},

	// Type of levels
	{"TOL_RACE",TOL_RACE},
	{"TOL_BATTLE",TOL_BATTLE},
	{"TOL_TV",TOL_TV},

	// Level flags
	{"LF_SCRIPTISFILE",LF_SCRIPTISFILE},
	{"LF_NOZONE",LF_NOZONE},
	{"LF_SECTIONRACE",LF_SECTIONRACE},
	// And map flags
	{"LF2_HIDEINMENU",LF2_HIDEINMENU},
	{"LF2_HIDEINSTATS",LF2_HIDEINSTATS},
	{"LF2_TIMEATTACK",LF2_TIMEATTACK},
	{"LF2_VISITNEEDED",LF2_VISITNEEDED},

	// Emeralds
	{"EMERALD_CHAOS1",EMERALD_CHAOS1},
	{"EMERALD_CHAOS2",EMERALD_CHAOS2},
	{"EMERALD_CHAOS3",EMERALD_CHAOS3},
	{"EMERALD_CHAOS4",EMERALD_CHAOS4},
	{"EMERALD_CHAOS5",EMERALD_CHAOS5},
	{"EMERALD_CHAOS6",EMERALD_CHAOS6},
	{"EMERALD_CHAOS7",EMERALD_CHAOS7},
	{"EMERALD_ALLCHAOS",EMERALD_ALLCHAOS},
	{"EMERALD_SUPER1",EMERALD_SUPER1},
	{"EMERALD_SUPER2",EMERALD_SUPER2},
	{"EMERALD_SUPER3",EMERALD_SUPER3},
	{"EMERALD_SUPER4",EMERALD_SUPER4},
	{"EMERALD_SUPER5",EMERALD_SUPER5},
	{"EMERALD_SUPER6",EMERALD_SUPER6},
	{"EMERALD_SUPER7",EMERALD_SUPER7},
	{"EMERALD_ALLSUPER",EMERALD_ALLSUPER},
	{"EMERALD_ALL",EMERALD_ALL},

	// SKINCOLOR_ doesn't include these..!
	{"MAXSKINCOLORS",MAXSKINCOLORS},
	{"FIRSTSUPERCOLOR",FIRSTSUPERCOLOR},
	{"NUMSUPERCOLORS",NUMSUPERCOLORS},

	// Precipitation
	{"PRECIP_NONE",PRECIP_NONE},
	{"PRECIP_RAIN",PRECIP_RAIN},
	{"PRECIP_SNOW",PRECIP_SNOW},
	{"PRECIP_BLIZZARD",PRECIP_BLIZZARD},
	{"PRECIP_STORM",PRECIP_STORM},
	{"PRECIP_STORM_NORAIN",PRECIP_STORM_NORAIN},
	{"PRECIP_STORM_NOSTRIKES",PRECIP_STORM_NOSTRIKES},

	// Shields
	{"SH_NONE",SH_NONE},
	// Shield flags
	{"SH_PROTECTFIRE",SH_PROTECTFIRE},
	{"SH_PROTECTWATER",SH_PROTECTWATER},
	{"SH_PROTECTELECTRIC",SH_PROTECTELECTRIC},
	{"SH_PROTECTSPIKE",SH_PROTECTSPIKE},
	// Indivisible shields
	{"SH_PITY",SH_PITY},
	{"SH_WHIRLWIND",SH_WHIRLWIND},
	{"SH_ARMAGEDDON",SH_ARMAGEDDON},
	{"SH_PINK",SH_PINK},
	// normal shields that use flags
	{"SH_ATTRACT",SH_ATTRACT},
	{"SH_ELEMENTAL",SH_ELEMENTAL},
	// Sonic 3 shields
	{"SH_FLAMEAURA",SH_FLAMEAURA},
	{"SH_BUBBLEWRAP",SH_BUBBLEWRAP},
	{"SH_THUNDERCOIN",SH_THUNDERCOIN},
	// The force shield uses the lower 8 bits to count how many extra hits are left.
	{"SH_FORCE",SH_FORCE},
	{"SH_FORCEHP",SH_FORCEHP}, // to be used as a bitmask only
	// Mostly for use with Mario mode.
	{"SH_FIREFLOWER",SH_FIREFLOWER},
	{"SH_STACK",SH_STACK},
	{"SH_NOSTACK",SH_NOSTACK},

	// Carrying
	{"CR_NONE",CR_NONE},
	{"CR_ZOOMTUBE",CR_ZOOMTUBE},

	// Character flags (skinflags_t)
	{"SF_HIRES",SF_HIRES},
	{"SF_MACHINE",SF_MACHINE},

	// Dashmode constants
	{"DASHMODE_THRESHOLD",DASHMODE_THRESHOLD},
	{"DASHMODE_MAX",DASHMODE_MAX},

	// Sound flags
	{"SF_TOTALLYSINGLE",SF_TOTALLYSINGLE},
	{"SF_NOMULTIPLESOUND",SF_NOMULTIPLESOUND},
	{"SF_OUTSIDESOUND",SF_OUTSIDESOUND},
	{"SF_X4AWAYSOUND",SF_X4AWAYSOUND},
	{"SF_X8AWAYSOUND",SF_X8AWAYSOUND},
	{"SF_NOINTERRUPT",SF_NOINTERRUPT},
	{"SF_X2AWAYSOUND",SF_X2AWAYSOUND},

	// Global emblem var flags

	// Map emblem var flags
	{"ME_ENCORE",ME_ENCORE},

	// p_local.h constants
	{"FLOATSPEED",FLOATSPEED},
	{"MAXSTEPMOVE",MAXSTEPMOVE},
	{"USERANGE",USERANGE},
	{"MELEERANGE",MELEERANGE},
	{"MISSILERANGE",MISSILERANGE},
	{"ONFLOORZ",ONFLOORZ}, // INT32_MIN
	{"ONCEILINGZ",ONCEILINGZ}, //INT32_MAX
	// for P_FlashPal
	{"PAL_WHITE",PAL_WHITE},
	{"PAL_MIXUP",PAL_MIXUP},
	{"PAL_RECYCLE",PAL_RECYCLE},
	{"PAL_NUKE",PAL_NUKE},

	// Gametypes, for use with global var "gametype"
	{"GT_RACE",GT_RACE},
	{"GT_BATTLE",GT_BATTLE},

	// for P_DamageMobj
	//// Damage types
	{"DMG_NORMAL",DMG_NORMAL},
	{"DMG_WIPEOUT",DMG_WIPEOUT},
	{"DMG_EXPLODE",DMG_EXPLODE},
	{"DMG_SQUISH",DMG_SQUISH},
	{"DMG_STING",DMG_STING},
	{"DMG_KARMA",DMG_KARMA},
	//// Death types
	{"DMG_INSTAKILL",DMG_INSTAKILL},
	{"DMG_DEATHPIT",DMG_DEATHPIT},
	{"DMG_CRUSHED",DMG_CRUSHED},
	{"DMG_SPECTATOR",DMG_SPECTATOR},
	{"DMG_TIMEOVER",DMG_TIMEOVER},
	//// Masks
	{"DMG_STEAL",DMG_CANTHURTSELF},
	{"DMG_CANTHURTSELF",DMG_CANTHURTSELF},
	{"DMG_DEATHMASK",DMG_DEATHMASK},
	{"DMG_TYPEMASK",DMG_TYPEMASK},

	// Intermission types
	{"int_none",int_none},
	{"int_race",int_race},
	{"int_battle",int_battle},
	{"int_timeattack",int_timeattack},

	// Jingles (jingletype_t)
	{"JT_NONE",JT_NONE},
	{"JT_OTHER",JT_OTHER},
	{"JT_MASTER",JT_MASTER},
	{"JT_1UP",JT_1UP},
	{"JT_SHOES",JT_SHOES},
	{"JT_INV",JT_INV},
	{"JT_MINV",JT_MINV},
	{"JT_DROWN",JT_DROWN},
	{"JT_SUPER",JT_SUPER},
	{"JT_GOVER",JT_GOVER},
	{"JT_NIGHTSTIMEOUT",JT_NIGHTSTIMEOUT},
	{"JT_SSTIMEOUT",JT_SSTIMEOUT},
	// {"JT_LCLEAR",JT_LCLEAR},
	// {"JT_RACENT",JT_RACENT},
	// {"JT_CONTSC",JT_CONTSC},

	// Player state (playerstate_t)
	{"PST_LIVE",PST_LIVE}, // Playing or camping.
	{"PST_DEAD",PST_DEAD}, // Dead on the ground, view follows killer.
	{"PST_REBORN",PST_REBORN}, // Ready to restart/respawn???

	// Player animation (panim_t)
	{"PA_ETC",PA_ETC},
	{"PA_IDLE",PA_IDLE},
	{"PA_EDGE",PA_EDGE},
	{"PA_WALK",PA_WALK},
	{"PA_RUN",PA_RUN},
	{"PA_DASH",PA_DASH},
	{"PA_PAIN",PA_PAIN},
	{"PA_ROLL",PA_ROLL},
	{"PA_JUMP",PA_JUMP},
	{"PA_SPRING",PA_SPRING},
	{"PA_FALL",PA_FALL},
	{"PA_ABILITY",PA_ABILITY},
	{"PA_ABILITY2",PA_ABILITY2},
	{"PA_RIDE",PA_RIDE},

	// Value for infinite lives
	{"INFLIVES",INFLIVES},

	// Got Flags, for player->gotflag!
	// Used to be MF_ for some stupid reason, now they're GF_ to stop them looking like mobjflags
	{"GF_REDFLAG",GF_REDFLAG},
	{"GF_BLUEFLAG",GF_BLUEFLAG},

	// Customisable sounds for Skins, from sounds.h
	{"SKSSPIN",SKSSPIN},
	{"SKSPUTPUT",SKSPUTPUT},
	{"SKSPUDPUD",SKSPUDPUD},
	{"SKSPLPAN1",SKSPLPAN1}, // Ouchies
	{"SKSPLPAN2",SKSPLPAN2},
	{"SKSPLPAN3",SKSPLPAN3},
	{"SKSPLPAN4",SKSPLPAN4},
	{"SKSPLDET1",SKSPLDET1}, // Deaths
	{"SKSPLDET2",SKSPLDET2},
	{"SKSPLDET3",SKSPLDET3},
	{"SKSPLDET4",SKSPLDET4},
	{"SKSPLVCT1",SKSPLVCT1}, // Victories
	{"SKSPLVCT2",SKSPLVCT2},
	{"SKSPLVCT3",SKSPLVCT3},
	{"SKSPLVCT4",SKSPLVCT4},
	{"SKSTHOK",SKSTHOK},
	{"SKSSPNDSH",SKSSPNDSH},
	{"SKSZOOM",SKSZOOM},
	{"SKSSKID",SKSSKID},
	{"SKSGASP",SKSGASP},
	{"SKSJUMP",SKSJUMP},
	// SRB2kart
	{"SKSKWIN",SKSKWIN}, // Win quote
	{"SKSKLOSE",SKSKLOSE}, // Lose quote
	{"SKSKPAN1",SKSKPAN1}, // Pain
	{"SKSKPAN2",SKSKPAN2},
	{"SKSKATK1",SKSKATK1}, // Offense item taunt
	{"SKSKATK2",SKSKATK2},
	{"SKSKBST1",SKSKBST1}, // Boost item taunt
	{"SKSKBST2",SKSKBST2},
	{"SKSKSLOW",SKSKSLOW}, // Overtake taunt
	{"SKSKHITM",SKSKHITM}, // Hit confirm taunt
	{"SKSKPOWR",SKSKPOWR}, // Power item taunt

	// 3D Floor/Fake Floor/FOF/whatever flags
	{"FF_EXISTS",FF_EXISTS},                   ///< Always set, to check for validity.
	{"FF_BLOCKPLAYER",FF_BLOCKPLAYER},         ///< Solid to player, but nothing else
	{"FF_BLOCKOTHERS",FF_BLOCKOTHERS},         ///< Solid to everything but player
	{"FF_SOLID",FF_SOLID},                     ///< Clips things.
	{"FF_RENDERSIDES",FF_RENDERSIDES},         ///< Renders the sides.
	{"FF_RENDERPLANES",FF_RENDERPLANES},       ///< Renders the floor/ceiling.
	{"FF_RENDERALL",FF_RENDERALL},             ///< Renders everything.
	{"FF_SWIMMABLE",FF_SWIMMABLE},             ///< Is a water block.
	{"FF_NOSHADE",FF_NOSHADE},                 ///< Messes with the lighting?
	{"FF_CUTSOLIDS",FF_CUTSOLIDS},             ///< Cuts out hidden solid pixels.
	{"FF_CUTEXTRA",FF_CUTEXTRA},               ///< Cuts out hidden translucent pixels.
	{"FF_CUTLEVEL",FF_CUTLEVEL},               ///< Cuts out all hidden pixels.
	{"FF_CUTSPRITES",FF_CUTSPRITES},           ///< Final step in making 3D water.
	{"FF_BOTHPLANES",FF_BOTHPLANES},           ///< Render inside and outside planes.
	{"FF_EXTRA",FF_EXTRA},                     ///< Gets cut by ::FF_CUTEXTRA.
	{"FF_TRANSLUCENT",FF_TRANSLUCENT},         ///< See through!
	{"FF_FOG",FF_FOG},                         ///< Fog "brush."
	{"FF_INVERTPLANES",FF_INVERTPLANES},       ///< Only render inside planes.
	{"FF_ALLSIDES",FF_ALLSIDES},               ///< Render inside and outside sides.
	{"FF_INVERTSIDES",FF_INVERTSIDES},         ///< Only render inside sides.
	{"FF_DOUBLESHADOW",FF_DOUBLESHADOW},       ///< Make two lightlist entries to reset light?
	{"FF_FLOATBOB",FF_FLOATBOB},               ///< Floats on water and bobs if you step on it.
	{"FF_NORETURN",FF_NORETURN},               ///< Used with ::FF_CRUMBLE. Will not return to its original position after falling.
	{"FF_CRUMBLE",FF_CRUMBLE},                 ///< Falls 2 seconds after being stepped on, and randomly brings all touching crumbling 3dfloors down with it, providing their master sectors share the same tag (allows crumble platforms above or below, to also exist).
	{"FF_SHATTERBOTTOM",FF_SHATTERBOTTOM},     ///< Used with ::FF_BUSTUP. Like FF_SHATTER, but only breaks from the bottom. Good for springing up through rubble.
	{"FF_MARIO",FF_MARIO},                     ///< Acts like a question block when hit from underneath. Goodie spawned at top is determined by master sector.
	{"FF_BUSTUP",FF_BUSTUP},                   ///< You can spin through/punch this block and it will crumble!
	{"FF_QUICKSAND",FF_QUICKSAND},             ///< Quicksand!
	{"FF_PLATFORM",FF_PLATFORM},               ///< You can jump up through this to the top.
	{"FF_REVERSEPLATFORM",FF_REVERSEPLATFORM}, ///< A fall-through floor in normal gravity, a platform in reverse gravity.
	{"FF_INTANGIBLEFLATS",FF_INTANGIBLEFLATS}, ///< Both flats are intangible, but the sides are still solid.
	{"FF_INTANGABLEFLATS",FF_INTANGIBLEFLATS}, ///< Both flats are intangable, but the sides are still solid.
	{"FF_SHATTER",FF_SHATTER},                 ///< Used with ::FF_BUSTUP. Bustable on mere touch.
	{"FF_SPINBUST",FF_SPINBUST},               ///< Used with ::FF_BUSTUP. Also bustable if you're in your spinning frames.
	{"FF_STRONGBUST",FF_STRONGBUST},           ///< Used with ::FF_BUSTUP. Only bustable by "strong" characters (Knuckles) and abilities (bouncing, twinspin, melee).
	{"FF_RIPPLE",FF_RIPPLE},                   ///< Ripple the flats
	{"FF_COLORMAPONLY",FF_COLORMAPONLY},       ///< Only copy the colormap, not the lightlevel
	{"FF_GOOWATER",FF_GOOWATER},               ///< Used with ::FF_SWIMMABLE. Makes thick bouncey goop.

	// PolyObject flags
	{"POF_CLIPLINES",POF_CLIPLINES},               ///< Test against lines for collision
	{"POF_CLIPPLANES",POF_CLIPPLANES},             ///< Test against tops and bottoms for collision
	{"POF_SOLID",POF_SOLID},                       ///< Clips things.
	{"POF_TESTHEIGHT",POF_TESTHEIGHT},             ///< Test line collision with heights
	{"POF_RENDERSIDES",POF_RENDERSIDES},           ///< Renders the sides.
	{"POF_RENDERTOP",POF_RENDERTOP},               ///< Renders the top.
	{"POF_RENDERBOTTOM",POF_RENDERBOTTOM},         ///< Renders the bottom.
	{"POF_RENDERPLANES",POF_RENDERPLANES},         ///< Renders top and bottom.
	{"POF_RENDERALL",POF_RENDERALL},               ///< Renders everything.
	{"POF_INVERT",POF_INVERT},                     ///< Inverts collision (like a cage).
	{"POF_INVERTPLANES",POF_INVERTPLANES},         ///< Render inside planes.
	{"POF_INVERTPLANESONLY",POF_INVERTPLANESONLY}, ///< Only render inside planes.
	{"POF_PUSHABLESTOP",POF_PUSHABLESTOP},         ///< Pushables will stop movement.
	{"POF_LDEXEC",POF_LDEXEC},                     ///< This PO triggers a linedef executor.
	{"POF_ONESIDE",POF_ONESIDE},                   ///< Only use the first side of the linedef.
	{"POF_NOSPECIALS",POF_NOSPECIALS},             ///< Don't apply sector specials.
	{"POF_SPLAT",POF_SPLAT},                       ///< Use splat flat renderer (treat cyan pixels as invisible).

#ifdef HAVE_LUA_SEGS
	// Node flags
	{"NF_SUBSECTOR",NF_SUBSECTOR}, // Indicate a leaf.
#endif

	// Slope flags
	{"SL_NOPHYSICS",SL_NOPHYSICS},
	{"SL_DYNAMIC",SL_DYNAMIC},

	// Angles
	{"ANG1",ANG1},
	{"ANG2",ANG2},
	{"ANG10",ANG10},
	{"ANG15",ANG15},
	{"ANG20",ANG20},
	{"ANG30",ANG30},
	{"ANG60",ANG60},
	{"ANG64h",ANG64h},
	{"ANG105",ANG105},
	{"ANG210",ANG210},
	{"ANG255",ANG255},
	{"ANG340",ANG340},
	{"ANG350",ANG350},
	{"ANGLE_11hh",ANGLE_11hh},
	{"ANGLE_22h",ANGLE_22h},
	{"ANGLE_45",ANGLE_45},
	{"ANGLE_67h",ANGLE_67h},
	{"ANGLE_90",ANGLE_90},
	{"ANGLE_112h",ANGLE_112h},
	{"ANGLE_135",ANGLE_135},
	{"ANGLE_157h",ANGLE_157h},
	{"ANGLE_180",ANGLE_180},
	{"ANGLE_202h",ANGLE_202h},
	{"ANGLE_225",ANGLE_225},
	{"ANGLE_247h",ANGLE_247h},
	{"ANGLE_270",ANGLE_270},
	{"ANGLE_292h",ANGLE_292h},
	{"ANGLE_315",ANGLE_315},
	{"ANGLE_337h",ANGLE_337h},
	{"ANGLE_MAX",ANGLE_MAX},

	// P_Chase directions (dirtype_t)
	{"DI_NODIR",DI_NODIR},
	{"DI_EAST",DI_EAST},
	{"DI_NORTHEAST",DI_NORTHEAST},
	{"DI_NORTH",DI_NORTH},
	{"DI_NORTHWEST",DI_NORTHWEST},
	{"DI_WEST",DI_WEST},
	{"DI_SOUTHWEST",DI_SOUTHWEST},
	{"DI_SOUTH",DI_SOUTH},
	{"DI_SOUTHEAST",DI_SOUTHEAST},
	{"NUMDIRS",NUMDIRS},

	// Buttons (ticcmd_t)	// SRB2kart
	{"BT_ACCELERATE",BT_ACCELERATE},
	{"BT_DRIFT",BT_DRIFT},
	{"BT_BRAKE",BT_BRAKE},
	{"BT_ATTACK",BT_ATTACK},
	{"BT_FORWARD",BT_FORWARD},
	{"BT_BACKWARD",BT_BACKWARD},
	{"BT_CUSTOM1",BT_CUSTOM1}, // Lua customizable
	{"BT_CUSTOM2",BT_CUSTOM2}, // Lua customizable
	{"BT_CUSTOM3",BT_CUSTOM3}, // Lua customizable

	// Lua command registration flags
	{"COM_ADMIN",COM_ADMIN},
	{"COM_LOCAL",COM_LOCAL},
	{"COM_PLAYER2",COM_PLAYER2},
	{"COM_PLAYER3",COM_PLAYER3},
	{"COM_PLAYER4",COM_PLAYER4},
	{"COM_SPLITSCREEN",COM_SPLITSCREEN},
	{"COM_SSSHIFT",COM_SSSHIFT},

	// cvflags_t
	{"CV_SAVE",CV_SAVE},
	{"CV_CALL",CV_CALL},
	{"CV_NETVAR",CV_NETVAR},
	{"CV_NOINIT",CV_NOINIT},
	{"CV_FLOAT",CV_FLOAT},
	{"CV_NOTINNET",CV_NOTINNET},
	{"CV_MODIFIED",CV_MODIFIED},
	{"CV_SHOWMODIF",CV_SHOWMODIF},
	{"CV_SHOWMODIFONETIME",CV_SHOWMODIFONETIME},
	{"CV_NOSHOWHELP",CV_NOSHOWHELP},
	{"CV_HIDEN",CV_HIDEN},
	{"CV_HIDDEN",CV_HIDEN},
	{"CV_CHEAT",CV_CHEAT},
	{"CV_NOLUA",CV_NOLUA},

	// v_video flags
	{"V_NOSCALEPATCH",V_NOSCALEPATCH},
	{"V_SMALLSCALEPATCH",V_SMALLSCALEPATCH},
	{"V_MEDSCALEPATCH",V_MEDSCALEPATCH},
	{"V_6WIDTHSPACE",V_6WIDTHSPACE},
	{"V_OLDSPACING",V_OLDSPACING},
	{"V_MONOSPACE",V_MONOSPACE},

	{"V_PURPLEMAP",V_PURPLEMAP},
	{"V_YELLOWMAP",V_YELLOWMAP},
	{"V_GREENMAP",V_GREENMAP},
	{"V_BLUEMAP",V_BLUEMAP},
	{"V_REDMAP",V_REDMAP},
	{"V_GRAYMAP",V_GRAYMAP},
	{"V_ORANGEMAP",V_ORANGEMAP},
	{"V_SKYMAP",V_SKYMAP},
	{"V_LAVENDERMAP",V_LAVENDERMAP},
	{"V_GOLDMAP",V_GOLDMAP},
	{"V_AQUAMAP",V_AQUAMAP},
	{"V_MAGENTAMAP",V_MAGENTAMAP},
	{"V_PINKMAP",V_PINKMAP},
	{"V_BROWNMAP",V_BROWNMAP},
	{"V_TANMAP",V_TANMAP},

	{"V_TRANSLUCENT",V_TRANSLUCENT},
	{"V_10TRANS",V_10TRANS},
	{"V_20TRANS",V_20TRANS},
	{"V_30TRANS",V_30TRANS},
	{"V_40TRANS",V_40TRANS},
	{"V_50TRANS",V_TRANSLUCENT}, // alias
	{"V_60TRANS",V_60TRANS},
	{"V_70TRANS",V_70TRANS},
	{"V_80TRANS",V_80TRANS},
	{"V_90TRANS",V_90TRANS},
	{"V_HUDTRANSHALF",V_HUDTRANSHALF},
	{"V_HUDTRANS",V_HUDTRANS},
	{"V_RETURN8",V_RETURN8},
	{"V_OFFSET",V_OFFSET},
	{"V_ALLOWLOWERCASE",V_ALLOWLOWERCASE},
	{"V_FLIP",V_FLIP},
	{"V_SNAPTOTOP",V_SNAPTOTOP},
	{"V_SNAPTOBOTTOM",V_SNAPTOBOTTOM},
	{"V_SNAPTOLEFT",V_SNAPTOLEFT},
	{"V_SNAPTORIGHT",V_SNAPTORIGHT},
	{"V_WRAPX",V_WRAPX},
	{"V_WRAPY",V_WRAPY},
	{"V_NOSCALESTART",V_NOSCALESTART},
	{"V_SPLITSCREEN",V_SPLITSCREEN},
	{"V_SLIDEIN",V_SLIDEIN},

	{"V_PARAMMASK",V_PARAMMASK},
	{"V_SCALEPATCHMASK",V_SCALEPATCHMASK},
	{"V_SPACINGMASK",V_SPACINGMASK},
	{"V_CHARCOLORMASK",V_CHARCOLORMASK},
	{"V_ALPHAMASK",V_ALPHAMASK},

	{"V_CHARCOLORSHIFT",V_CHARCOLORSHIFT},
	{"V_ALPHASHIFT",V_ALPHASHIFT},

	//Kick Reasons
	{"KR_KICK",KR_KICK},
	{"KR_PINGLIMIT",KR_PINGLIMIT},
	{"KR_SYNCH",KR_SYNCH},
	{"KR_TIMEOUT",KR_TIMEOUT},
	{"KR_BAN",KR_BAN},
	{"KR_LEAVE",KR_LEAVE},

	// translation colormaps
	{"TC_DEFAULT",TC_DEFAULT},
	{"TC_BOSS",TC_BOSS},
	{"TC_METALSONIC",TC_METALSONIC},
	{"TC_ALLWHITE",TC_ALLWHITE},
	{"TC_RAINBOW",TC_RAINBOW},
	{"TC_BLINK",TC_BLINK},
	{"TC_DASHMODE",TC_DASHMODE},

	// marathonmode flags
	{"MA_INIT",MA_INIT},
	{"MA_RUNNING",MA_RUNNING},
	{"MA_NOCUTSCENES",MA_NOCUTSCENES},
	{"MA_INGAME",MA_INGAME},

	// SRB2Kart
	// kartitems_t
#define FOREACH( name, n ) { #name, KITEM_ ## name }
	KART_ITEM_ITERATOR, // Actual items (can be set for k_itemtype)
#undef  FOREACH
	{"NUMKARTITEMS",NUMKARTITEMS},
	{"KRITEM_DUALSNEAKER",KRITEM_DUALSNEAKER}, // Additional roulette IDs (not usable for much in Lua besides K_GetItemPatch)
	{"KRITEM_TRIPLESNEAKER",KRITEM_TRIPLESNEAKER},
	{"KRITEM_TRIPLEBANANA",KRITEM_TRIPLEBANANA},
	{"KRITEM_TENFOLDBANANA",KRITEM_TENFOLDBANANA},
	{"KRITEM_TRIPLEORBINAUT",KRITEM_TRIPLEORBINAUT},
	{"KRITEM_QUADORBINAUT",KRITEM_QUADORBINAUT},
	{"KRITEM_DUALJAWZ",KRITEM_DUALJAWZ},
	{"NUMKARTRESULTS",NUMKARTRESULTS},

	// kartshields_t
	{"KSHIELD_NONE",KSHIELD_NONE},
	{"KSHIELD_THUNDER",KSHIELD_THUNDER},
	{"KSHIELD_BUBBLE",KSHIELD_BUBBLE},
	{"KSHIELD_FLAME",KSHIELD_FLAME},
	{"NUMKARTSHIELDS",NUMKARTSHIELDS},

	// kartspinoutflags_t
	{"KSPIN_THRUST",KSPIN_THRUST},
	{"KSPIN_IFRAMES",KSPIN_IFRAMES},
	{"KSPIN_AIRTIMER",KSPIN_AIRTIMER},

	{"KSPIN_TYPEBIT",KSPIN_TYPEBIT},
	{"KSPIN_TYPEMASK",KSPIN_TYPEMASK},

	{"KSPIN_SPINOUT",KSPIN_SPINOUT},
	{"KSPIN_WIPEOUT",KSPIN_WIPEOUT},
	{"KSPIN_STUNG",KSPIN_STUNG},
	{"KSPIN_EXPLOSION",KSPIN_EXPLOSION},

	// translation colormaps
	{"TC_DEFAULT",TC_DEFAULT},
	{"TC_BOSS",TC_BOSS},
	{"TC_METALSONIC",TC_METALSONIC},
	{"TC_ALLWHITE",TC_ALLWHITE},
	{"TC_RAINBOW",TC_RAINBOW},
	{"TC_BLINK",TC_BLINK},

	// MFD_ draw flag enum
	{"MFD_DONTDRAWP1",MFD_DONTDRAWP1},
	{"MFD_DONTDRAWP2",MFD_DONTDRAWP2},
	{"MFD_DONTDRAWP3",MFD_DONTDRAWP3},
	{"MFD_DONTDRAWP4",MFD_DONTDRAWP4},
	{"MFD_TRANS10",MFD_TRANS10},
	{"MFD_TRANS20",MFD_TRANS20},
	{"MFD_TRANS30",MFD_TRANS30},
	{"MFD_TRANS40",MFD_TRANS40},
	{"MFD_TRANS50",MFD_TRANS50},
	{"MFD_TRANS60",MFD_TRANS60},
	{"MFD_TRANS70",MFD_TRANS70},
	{"MFD_TRANS80",MFD_TRANS80},
	{"MFD_TRANS90",MFD_TRANS90},
	{"MFD_TRANSADD",MFD_TRANSADD},
	{"MFD_TRANSSUB",MFD_TRANSSUB},
	{"MFD_TRANSMASK",MFD_TRANSMASK},
	{"MFD_FULLBRIGHT",MFD_FULLBRIGHT},
	{"MFD_SEMIBRIGHT",MFD_SEMIBRIGHT},
	{"MFD_NOBRIGHT",MFD_NOBRIGHT},
	{"MFD_BRIGHTMASK",MFD_BRIGHTMASK},
	{"MFD_DONTDRAW",MFD_DONTDRAW},
	{"MFD_SHADOW",MFD_SHADOW},
	{"MFD_TRANSSHIFT",MFD_TRANSSHIFT},

	{NULL,0}
};

static mobjtype_t get_mobjtype(const char *word)
{ // Returns the value of MT_ enumerations
	mobjtype_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("MT_",word,3))
		word += 3; // take off the MT_
	for (i = 0; i < NUMMOBJFREESLOTS; i++) {
		if (!FREE_MOBJS[i])
			break;
		if (fastcmp(word, FREE_MOBJS[i]))
			return MT_FIRSTFREESLOT+i;
	}
	for (i = 0; i < MT_FIRSTFREESLOT; i++)
		if (fastcmp(word, MOBJTYPE_LIST[i]+3))
			return i;
	deh_warning("Couldn't find mobjtype named 'MT_%s'",word);
	return MT_NULL;
}

static statenum_t get_state(const char *word)
{ // Returns the value of S_ enumerations
	statenum_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("S_",word,2))
		word += 2; // take off the S_
	for (i = 0; i < NUMSTATEFREESLOTS; i++) {
		if (!FREE_STATES[i])
			break;
		if (fastcmp(word, FREE_STATES[i]))
			return S_FIRSTFREESLOT+i;
	}
	for (i = 0; i < S_FIRSTFREESLOT; i++)
		if (fastcmp(word, STATE_LIST[i]+2))
			return i;
	deh_warning("Couldn't find state named 'S_%s'",word);
	return S_NULL;
}

skincolornum_t get_skincolor(const char *word)
{ // Returns the value of SKINCOLOR_ enumerations
	skincolornum_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("SKINCOLOR_",word,10))
		word += 10; // take off the SKINCOLOR_
	for (i = 0; i < NUMCOLORFREESLOTS; i++) {
		if (!FREE_SKINCOLORS[i])
			break;
		if (fastcmp(word, FREE_SKINCOLORS[i]))
			return SKINCOLOR_FIRSTFREESLOT+i;
	}
	for (i = 0; i < SKINCOLOR_FIRSTFREESLOT; i++)
		if (fastcmp(word, COLOR_ENUMS[i]))
			return i;
	deh_warning("Couldn't find skincolor named 'SKINCOLOR_%s'",word);
	return SKINCOLOR_GREEN;
}

static spritenum_t get_sprite(const char *word)
{ // Returns the value of SPR_ enumerations
	spritenum_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("SPR_",word,4))
		word += 4; // take off the SPR_
	for (i = 0; i < NUMSPRITES; i++)
		if (!sprnames[i][4] && memcmp(word,sprnames[i],4)==0)
			return i;
	deh_warning("Couldn't find sprite named 'SPR_%s'",word);
	return SPR_NULL;
}

static playersprite_t get_sprite2(const char *word)
{ // Returns the value of SPR2_ enumerations
	playersprite_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("SPR2_",word,5))
		word += 5; // take off the SPR2_
	for (i = 0; i < NUMPLAYERSPRITES; i++)
		if (!spr2names[i][4] && memcmp(word,spr2names[i],4)==0)
			return i;
	deh_warning("Couldn't find sprite named 'SPR2_%s'",word);
	return SPR2_STIL;
}

static sfxenum_t get_sfx(const char *word)
{ // Returns the value of SFX_ enumerations
	sfxenum_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("SFX_",word,4))
		word += 4; // take off the SFX_
	else if (fastncmp("DS",word,2))
		word += 2; // take off the DS
	for (i = 0; i < NUMSFX; i++)
		if (S_sfx[i].name && fasticmp(word, S_sfx[i].name))
			return i;
	deh_warning("Couldn't find sfx named 'SFX_%s'",word);
	return sfx_None;
}

#ifdef MUSICSLOT_COMPATIBILITY
static UINT16 get_mus(const char *word, UINT8 dehacked_mode)
{ // Returns the value of MUS_ enumerations
	UINT16 i;
	char lumptmp[4];

	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (!word[2] && toupper(word[0]) >= 'A' && toupper(word[0]) <= 'Z')
		return (UINT16)M_MapNumber(word[0], word[1]);

	if (fastncmp("MUS_",word,4))
		word += 4; // take off the MUS_
	else if (fastncmp("O_",word,2) || fastncmp("D_",word,2))
		word += 2; // take off the O_ or D_

	strncpy(lumptmp, word, 4);
	lumptmp[3] = 0;
	if (fasticmp("MAP",lumptmp))
	{
		word += 3;
		if (toupper(word[0]) >= 'A' && toupper(word[0]) <= 'Z')
			return (UINT16)M_MapNumber(word[0], word[1]);
		else if ((i = atoi(word)))
			return i;

		word -= 3;
		if (dehacked_mode)
			deh_warning("Couldn't find music named 'MUS_%s'",word);
		return 0;
	}
	for (i = 0; compat_special_music_slots[i][0]; ++i)
		if (fasticmp(word, compat_special_music_slots[i]))
			return i + 1036;
	if (dehacked_mode)
		deh_warning("Couldn't find music named 'MUS_%s'",word);
	return 0;
}
#endif

static hudnum_t get_huditem(const char *word)
{ // Returns the value of HUD_ enumerations
	hudnum_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("HUD_",word,4))
		word += 4; // take off the HUD_
	for (i = 0; i < NUMHUDITEMS; i++)
		if (fastcmp(word, HUDITEMS_LIST[i]))
			return i;
	deh_warning("Couldn't find huditem named 'HUD_%s'",word);
	return HUD_LIVES;
}

static menutype_t get_menutype(const char *word)
{ // Returns the value of MN_ enumerations
	menutype_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("MN_",word,3))
		word += 3; // take off the MN_
	for (i = 0; i < NUMMENUTYPES; i++)
		if (fastcmp(word, MENUTYPES_LIST[i]))
			return i;
	deh_warning("Couldn't find menutype named 'MN_%s'",word);
	return MN_NONE;
}

/*
static INT16 get_gametype(const char *word)
{ // Returns the value of GT_ enumerations
	INT16 i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("GT_",word,3))
		word += 3; // take off the GT_
	for (i = 0; i < NUMGAMETYPES; i++)
		if (fastcmp(word, Gametype_ConstantNames[i]+3))
			return i;
	deh_warning("Couldn't find gametype named 'GT_%s'",word);
	return GT_COOP;
}

static powertype_t get_power(const char *word)
{ // Returns the value of pw_ enumerations
	powertype_t i;
	if (*word >= '0' && *word <= '9')
		return atoi(word);
	if (fastncmp("PW_",word,3))
		word += 3; // take off the pw_
	for (i = 0; i < NUMPOWERS; i++)
		if (fastcmp(word, POWERS_LIST[i]))
			return i;
	deh_warning("Couldn't find power named 'pw_%s'",word);
	return pw_invulnerability;
}
*/

/// \todo Make ANY of this completely over-the-top math craziness obey the order of operations.
static fixed_t op_mul(fixed_t a, fixed_t b) { return a*b; }
static fixed_t op_div(fixed_t a, fixed_t b) { return a/b; }
static fixed_t op_add(fixed_t a, fixed_t b) { return a+b; }
static fixed_t op_sub(fixed_t a, fixed_t b) { return a-b; }
static fixed_t op_or(fixed_t a, fixed_t b) { return a|b; }
static fixed_t op_and(fixed_t a, fixed_t b) { return a&b; }
static fixed_t op_lshift(fixed_t a, fixed_t b) { return a<<b; }
static fixed_t op_rshift(fixed_t a, fixed_t b) { return a>>b; }

struct {
	const char c;
	fixed_t (*v)(fixed_t,fixed_t);
} OPERATIONS[] = {
	{'*',op_mul},
	{'/',op_div},
	{'+',op_add},
	{'-',op_sub},
	{'|',op_or},
	{'&',op_and},
	{'<',op_lshift},
	{'>',op_rshift},
	{0,NULL}
};

// Returns the full word, cut at the first symbol or whitespace
/*
static char *read_word(const char *line)
{
	// Part 1: You got the start of the word, now find the end.
  const char *p;
	INT32 i;
	for (p = line+1; *p; p++) {
		if (*p == ' ' || *p == '\t')
			break;
		for (i = 0; OPERATIONS[i].c; i++)
			if (*p == OPERATIONS[i].c) {
				i = -1;
				break;
			}
		if (i == -1)
			break;
	}

	// Part 2: Make a copy of the word and return it.
	{
		size_t len = (p-line);
		char *word = malloc(len+1);
		M_Memcpy(word,line,len);
		word[len] = '\0';
		return word;
	}
}

static INT32 operation_pad(const char **word)
{ // Brings word the next operation and returns the operation number.
	INT32 i;
	for (; **word; (*word)++) {
		if (**word == ' ' || **word == '\t')
			continue;
		for (i = 0; OPERATIONS[i].c; i++)
			if (**word == OPERATIONS[i].c)
			{
				if ((**word == '<' && *(*word+1) == '<') || (**word == '>' && *(*word+1) == '>')) (*word)++; // These operations are two characters long.
				else if (**word == '<' || **word == '>') continue; // ... do not accept one character long.
				(*word)++;
				return i;
			}
		deh_warning("Unknown operation '%c'",**word);
		return -1;
	}
	return -1;
}

static void const_warning(const char *type, const char *word)
{
	deh_warning("Couldn't find %s named '%s'",type,word);
}

static fixed_t find_const(const char **rword)
{ // Finds the value of constants and returns it, bringing word to the next operation.
	INT32 i;
	fixed_t r;
	char *word = read_word(*rword);
	*rword += strlen(word);
	if ((*word >= '0' && *word <= '9') || *word == '-') { // Parse a number
		r = atoi(word);
		free(word);
		return r;
	}
	if (!*(word+1) && // Turn a single A-z symbol into numbers, like sprite frames.
	 ((*word >= 'A' && *word <= 'Z') || (*word >= 'a' && *word <= 'z'))) {
		r = R_Char2Frame(*word);
		free(word);
		return r;
	}
	if (fastncmp("MF_", word, 3)) {
		char *p = word+3;
		for (i = 0; MOBJFLAG_LIST[i]; i++)
			if (fastcmp(p, MOBJFLAG_LIST[i])) {
				free(word);
				return (1<<i);
			}

		// Not found error
		const_warning("mobj flag",word);
		free(word);
		return 0;
	}
	else if (fastncmp("MF2_", word, 4)) {
		char *p = word+4;
		for (i = 0; MOBJFLAG2_LIST[i]; i++)
			if (fastcmp(p, MOBJFLAG2_LIST[i])) {
				free(word);
				return (1<<i);
			}

		// Not found error
		const_warning("mobj flag2",word);
		free(word);
		return 0;
	}
	else if (fastncmp("MFE_", word, 4)) {
		char *p = word+4;
		for (i = 0; MOBJEFLAG_LIST[i]; i++)
			if (fastcmp(p, MOBJEFLAG_LIST[i])) {
				free(word);
				return (1<<i);
			}

		// Not found error
		const_warning("mobj eflag",word);
		free(word);
		return 0;
	}
	else if (fastncmp("PF_", word, 3)) {
		char *p = word+3;
		for (i = 0; PLAYERFLAG_LIST[i]; i++)
			if (fastcmp(p, PLAYERFLAG_LIST[i])) {
				free(word);
				return (1<<i);
			}
		if (fastcmp(p, "FULLSTASIS"))
			return PF_FULLSTASIS;

		// Not found error
		const_warning("player flag",word);
		free(word);
		return 0;
	}
	else if (fastncmp("S_",word,2)) {
		r = get_state(word);
		free(word);
		return r;
	}
	else if (fastncmp("SKINCOLOR_",word,10)) {
		r = get_skincolor(word);
		free(word);
		return r;
	}
	else if (fastncmp("MT_",word,3)) {
		r = get_mobjtype(word);
		free(word);
		return r;
	}
	else if (fastncmp("SPR_",word,4)) {
		r = get_sprite(word);
		free(word);
		return r;
	}
	else if (fastncmp("SFX_",word,4) || fastncmp("DS",word,2)) {
		r = get_sfx(word);
		free(word);
		return r;
	}
#ifdef MUSICSLOT_COMPATIBILITY
	else if (fastncmp("MUS_",word,4) || fastncmp("O_",word,2)) {
		r = get_mus(word, true);
		free(word);
		return r;
	}
#endif
	else if (fastncmp("PW_",word,3)) {
		r = get_power(word);
		free(word);
		return r;
	}
	else if (fastncmp("MN_",word,3)) {
		r = get_menutype(word);
		free(word);
		return r;
	}
	else if (fastncmp("GT_",word,3)) {
		r = get_gametype(word);
		free(word);
		return r;
	}
	else if (fastncmp("GTR_", word, 4)) {
		char *p = word+4;
		for (i = 0; GAMETYPERULE_LIST[i]; i++)
			if (fastcmp(p, GAMETYPERULE_LIST[i])) {
				free(word);
				return (1<<i);
			}

		// Not found error
		const_warning("game type rule",word);
		free(word);
		return 0;
	}
	else if (fastncmp("TOL_", word, 4)) {
		char *p = word+4;
		for (i = 0; TYPEOFLEVEL[i].name; i++)
			if (fastcmp(p, TYPEOFLEVEL[i].name)) {
				free(word);
				return TYPEOFLEVEL[i].flag;
			}

		// Not found error
		const_warning("typeoflevel",word);
		free(word);
		return 0;
	}
	else if (fastncmp("HUD_",word,4)) {
		r = get_huditem(word);
		free(word);
		return r;
	}
	else if (fastncmp("GRADE_",word,6))
	{
		char *p = word+6;
		for (i = 0; NIGHTSGRADE_LIST[i]; i++)
			if (*p == NIGHTSGRADE_LIST[i])
			{
				free(word);
				return i;
			}
		const_warning("NiGHTS grade",word);
		free(word);
		return 0;
	}
	for (i = 0; INT_CONST[i].n; i++)
		if (fastcmp(word,INT_CONST[i].n)) {
			free(word);
			return INT_CONST[i].v;
		}

	// Not found error.
	const_warning("constant",word);
	free(word);
	return 0;
}
*/

// Loops through every constant and operation in word and performs its calculations, returning the final value.
fixed_t get_number(const char *word)
{
	return LUA_EvalMath(word);

	/*// DESPERATELY NEEDED: Order of operations support! :x
	fixed_t i = find_const(&word);
	INT32 o;
	while(*word) {
		o = operation_pad(&word);
		if (o != -1)
			i = OPERATIONS[o].v(i,find_const(&word));
		else
			break;
	}
	return i;*/
}

void DEH_Check(void)
{
//#if defined(_DEBUG) || defined(PARANOIA)
	const size_t dehstates = sizeof(STATE_LIST)/sizeof(const char*);
	const size_t dehmobjs  = sizeof(MOBJTYPE_LIST)/sizeof(const char*);
	const size_t dehpowers = sizeof(POWERS_LIST)/sizeof(const char*);
	const size_t dehkartstuff = sizeof(KARTSTUFF_LIST)/sizeof(const char*);
	const size_t dehcolors = sizeof(COLOR_ENUMS)/sizeof(const char*);

	if (dehstates != S_FIRSTFREESLOT)
		I_Error("You forgot to update the Dehacked states list, you dolt!\n(%d states defined, versus %s in the Dehacked list)\n", S_FIRSTFREESLOT, sizeu1(dehstates));

	if (dehmobjs != MT_FIRSTFREESLOT)
		I_Error("You forgot to update the Dehacked mobjtype list, you dolt!\n(%d mobj types defined, versus %s in the Dehacked list)\n", MT_FIRSTFREESLOT, sizeu1(dehmobjs));

	if (dehpowers != NUMPOWERS)
		I_Error("You forgot to update the Dehacked powers list, you dolt!\n(%d powers defined, versus %s in the Dehacked list)\n", NUMPOWERS, sizeu1(dehpowers));

	if (dehkartstuff != NUMKARTSTUFF)
		I_Error("You forgot to update the Dehacked powers list, you dolt!\n(%d kartstuff defined, versus %s in the Dehacked list)\n", NUMKARTSTUFF, sizeu1(dehkartstuff));

	if (dehcolors != SKINCOLOR_FIRSTFREESLOT)
		I_Error("You forgot to update the Dehacked colors list, you dolt!\n(%d colors defined, versus %s in the Dehacked list)\n", SKINCOLOR_FIRSTFREESLOT, sizeu1(dehcolors));
}

#include "lua_script.h"
#include "lua_libs.h"

// freeslot takes a name (string only!)
// and allocates it to the appropriate free slot.
// Returns the slot number allocated for it or nil if failed.
// ex. freeslot("MT_MYTHING","S_MYSTATE1","S_MYSTATE2")
// TODO: Error checking! @.@; There's currently no way to know which ones failed and why!
//
static inline int lib_freeslot(lua_State *L)
{
	int n = lua_gettop(L);
	int r = 0; // args returned
	char *s, *type,*word;

	if (!lua_lumploading)
		return luaL_error(L, "This function cannot be called from within a hook or coroutine!");

	while (n-- > 0)
	{
		s = Z_StrDup(luaL_checkstring(L,1));
		type = strtok(s, "_");
		if (type)
			strupr(type);
		else {
			Z_Free(s);
			return luaL_error(L, "Unknown enum type in '%s'\n", luaL_checkstring(L, 1));
		}

		word = strtok(NULL, "\n");
		if (word)
			strupr(word);
		else {
			Z_Free(s);
			return luaL_error(L, "Missing enum name in '%s'\n", luaL_checkstring(L, 1));
		}
		if (fastcmp(type, "SFX")) {
			sfxenum_t sfx;
			strlwr(word);
			CONS_Printf("Sound sfx_%s allocated.\n",word);
			sfx = S_AddSoundFx(word, false, 0, false);
			if (sfx != sfx_None) {
				lua_pushinteger(L, sfx);
				r++;
			} else
				CONS_Alert(CONS_WARNING, "Ran out of free SFX slots!\n");
		}
		else if (fastcmp(type, "SPR"))
		{
			char wad;
			spritenum_t j;
			lua_getfield(L, LUA_REGISTRYINDEX, "WAD");
			wad = (char)lua_tointeger(L, -1);
			lua_pop(L, 1);
			for (j = SPR_FIRSTFREESLOT; j <= SPR_LASTFREESLOT; j++)
			{
				if (used_spr[(j-SPR_FIRSTFREESLOT)/8] & (1<<(j%8)))
				{
					if (!sprnames[j][4] && memcmp(sprnames[j],word,4)==0)
						sprnames[j][4] = wad;
					continue; // Already allocated, next.
				}
				// Found a free slot!
				CONS_Printf("Sprite SPR_%s allocated.\n",word);
				strncpy(sprnames[j],word,4);
				//sprnames[j][4] = 0;
				used_spr[(j-SPR_FIRSTFREESLOT)/8] |= 1<<(j%8); // Okay, this sprite slot has been named now.
				lua_pushinteger(L, j);
				r++;
				break;
			}
			if (j > SPR_LASTFREESLOT)
				CONS_Alert(CONS_WARNING, "Ran out of free sprite slots!\n");
		}
		else if (fastcmp(type, "S"))
		{
			statenum_t i;
			for (i = 0; i < NUMSTATEFREESLOTS; i++)
				if (!FREE_STATES[i]) {
					CONS_Printf("State S_%s allocated.\n",word);
					FREE_STATES[i] = Z_Malloc(strlen(word)+1, PU_STATIC, NULL);
					strcpy(FREE_STATES[i],word);
					freeslotusage[0][0]++;
					lua_pushinteger(L, S_FIRSTFREESLOT + i);
					r++;
					break;
				}
			if (i == NUMSTATEFREESLOTS)
				CONS_Alert(CONS_WARNING, "Ran out of free State slots!\n");
		}
		else if (fastcmp(type, "MT"))
		{
			mobjtype_t i;
			for (i = 0; i < NUMMOBJFREESLOTS; i++)
				if (!FREE_MOBJS[i]) {
					CONS_Printf("MobjType MT_%s allocated.\n",word);
					FREE_MOBJS[i] = Z_Malloc(strlen(word)+1, PU_STATIC, NULL);
					strcpy(FREE_MOBJS[i],word);
					freeslotusage[1][0]++;
					lua_pushinteger(L, MT_FIRSTFREESLOT + i);
					r++;
					break;
				}
			if (i == NUMMOBJFREESLOTS)
				CONS_Alert(CONS_WARNING, "Ran out of free MobjType slots!\n");
		}
		else if (fastcmp(type, "SKINCOLOR"))
		{
			skincolornum_t i;
			for (i = 0; i < NUMCOLORFREESLOTS; i++)
				if (!FREE_SKINCOLORS[i]) {
					CONS_Printf("Skincolor SKINCOLOR_%s allocated.\n",word);
					FREE_SKINCOLORS[i] = Z_Malloc(strlen(word)+1, PU_STATIC, NULL);
					strcpy(FREE_SKINCOLORS[i],word);
					M_AddMenuColor(numskincolors++);
					lua_pushinteger(L, SKINCOLOR_FIRSTFREESLOT + i);
					r++;
					break;
				}
			if (i == NUMCOLORFREESLOTS)
				CONS_Alert(CONS_WARNING, "Ran out of free skincolor slots!\n");
		}
		else if (fastcmp(type, "SPR2"))
		{
			// Search if we already have an SPR2 by that name...
			playersprite_t i;
			for (i = SPR2_FIRSTFREESLOT; i < free_spr2; i++)
				if (memcmp(spr2names[i],word,4) == 0)
					break;
			// We don't, so allocate a new one.
			if (i >= free_spr2) {
				if (free_spr2 < NUMPLAYERSPRITES)
				{
					CONS_Printf("Sprite SPR2_%s allocated.\n",word);
					strncpy(spr2names[free_spr2],word,4);
					spr2defaults[free_spr2] = 0;
					lua_pushinteger(L, free_spr2);
					r++;
					spr2names[free_spr2++][4] = 0;
				} else
					CONS_Alert(CONS_WARNING, "Ran out of free SPR2 slots!\n");
			}
		}
		else if (fastcmp(type, "TOL"))
		{
			// Search if we already have a typeoflevel by that name...
			int i;
			for (i = 0; TYPEOFLEVEL[i].name; i++)
				if (fastcmp(word, TYPEOFLEVEL[i].name))
					break;

			// We don't, so allocate a new one.
			if (TYPEOFLEVEL[i].name == NULL) {
				if (lastcustomtol == (UINT32)MAXTOL) // Unless you have way too many, since they're flags.
					CONS_Alert(CONS_WARNING, "Ran out of free typeoflevel slots!\n");
				else {
					CONS_Printf("TypeOfLevel TOL_%s allocated.\n",word);
					G_AddTOL(lastcustomtol, word);
					lua_pushinteger(L, lastcustomtol);
					lastcustomtol <<= 1;
					r++;
				}
			}
		}
		Z_Free(s);
		lua_remove(L, 1);
		continue;
	}
	return r;
}

// Wrapper for ALL A_Action functions.
// Arguments: mobj_t actor, int var1, int var2
static int action_call(lua_State *L)
{
	//actionf_t *action = lua_touserdata(L,lua_upvalueindex(1));
	actionf_t *action = *((actionf_t **)luaL_checkudata(L, 1, META_ACTION));
	mobj_t *actor = *((mobj_t **)luaL_checkudata(L, 2, META_MOBJ));
	var1 = (INT32)luaL_optinteger(L, 3, 0);
	var2 = (INT32)luaL_optinteger(L, 4, 0);
	if (!actor)
		return LUA_ErrInvalid(L, "mobj_t");
	action->acp1(actor);
	return 0;
}

// Hardcoded A_Action name to call for super() or NULL if super() would be invalid.
// Set in lua_infolib.
const char *superactions[MAXRECURSION];
UINT8 superstack = 0;

static int lib_dummysuper(lua_State *L)
{
	return luaL_error(L, "Can't call super() outside of hardcode-replacing A_Action functions being called by state changes!"); // convoluted, I know. @_@;;
}

static inline int lib_getenum(lua_State *L)
{
	const char *word, *p;
	fixed_t i;
	boolean mathlib = lua_toboolean(L, lua_upvalueindex(1));
	if (lua_type(L,2) != LUA_TSTRING)
		return 0;
	word = lua_tostring(L,2);
	if (strlen(word) == 1) { // Assume sprite frame if length 1.
		if (*word >= 'A' && *word <= '~')
		{
			lua_pushinteger(L, *word-'A');
			return 1;
		}
		if (mathlib) return luaL_error(L, "constant '%s' could not be parsed.\n", word);
		return 0;
	}
	else if (fastncmp("MF_", word, 3)) {
		p = word+3;
		for (i = 0; MOBJFLAG_LIST[i]; i++)
			if (fastcmp(p, MOBJFLAG_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (mathlib) return luaL_error(L, "mobjflag '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("MF2_", word, 4)) {
		p = word+4;
		for (i = 0; MOBJFLAG2_LIST[i]; i++)
			if (fastcmp(p, MOBJFLAG2_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (mathlib) return luaL_error(L, "mobjflag2 '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("MFE_", word, 4)) {
		p = word+4;
		for (i = 0; MOBJEFLAG_LIST[i]; i++)
			if (fastcmp(p, MOBJEFLAG_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (mathlib) return luaL_error(L, "mobjeflag '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("MTF_", word, 4)) {
		p = word+4;
		for (i = 0; i < 4; i++)
			if (MAPTHINGFLAG_LIST[i] && fastcmp(p, MAPTHINGFLAG_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (mathlib) return luaL_error(L, "mapthingflag '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("PF_", word, 3)) {
		p = word+3;
		for (i = 0; PLAYERFLAG_LIST[i]; i++)
			if (fastcmp(p, PLAYERFLAG_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (fastcmp(p, "FULLSTASIS"))
		{
			lua_pushinteger(L, (lua_Integer)PF_FULLSTASIS);
			return 1;
		}
		else if (fastcmp(p, "USEDOWN")) // Remove case when 2.3 nears release...
		{
			lua_pushinteger(L, (lua_Integer)PF_SPINDOWN);
			return 1;
		}
		if (mathlib) return luaL_error(L, "playerflag '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("GT_", word, 3)) {
		p = word;
		for (i = 0; Gametype_ConstantNames[i]; i++)
			if (fastcmp(p, Gametype_ConstantNames[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		if (mathlib) return luaL_error(L, "gametype '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("GTR_", word, 4)) {
		p = word+4;
		for (i = 0; GAMETYPERULE_LIST[i]; i++)
			if (fastcmp(p, GAMETYPERULE_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (mathlib) return luaL_error(L, "game type rule '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("TOL_", word, 4)) {
		p = word+4;
		for (i = 0; TYPEOFLEVEL[i].name; i++)
			if (fastcmp(p, TYPEOFLEVEL[i].name)) {
				lua_pushinteger(L, TYPEOFLEVEL[i].flag);
				return 1;
			}
		if (mathlib) return luaL_error(L, "typeoflevel '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("ML_", word, 3)) {
		p = word+3;
		for (i = 0; i < 16; i++)
			if (ML_LIST[i] && fastcmp(p, ML_LIST[i])) {
				lua_pushinteger(L, ((lua_Integer)1<<i));
				return 1;
			}
		if (mathlib) return luaL_error(L, "linedef flag '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("S_",word,2)) {
		p = word+2;
		for (i = 0; i < NUMSTATEFREESLOTS; i++) {
			if (!FREE_STATES[i])
				break;
			if (fastcmp(p, FREE_STATES[i])) {
				lua_pushinteger(L, S_FIRSTFREESLOT+i);
				return 1;
			}
		}
		for (i = 0; i < S_FIRSTFREESLOT; i++)
			if (fastcmp(p, STATE_LIST[i]+2)) {
				lua_pushinteger(L, i);
				return 1;
			}
		return luaL_error(L, "state '%s' does not exist.\n", word);
	}
	else if (fastncmp("MT_",word,3)) {
		p = word+3;
		for (i = 0; i < NUMMOBJFREESLOTS; i++) {
			if (!FREE_MOBJS[i])
				break;
			if (fastcmp(p, FREE_MOBJS[i])) {
				lua_pushinteger(L, MT_FIRSTFREESLOT+i);
				return 1;
			}
		}
		for (i = 0; i < MT_FIRSTFREESLOT; i++)
			if (fastcmp(p, MOBJTYPE_LIST[i]+3)) {
				lua_pushinteger(L, i);
				return 1;
			}
		return luaL_error(L, "mobjtype '%s' does not exist.\n", word);
	}
	else if (fastncmp("SPR_",word,4)) {
		p = word+4;
		for (i = 0; i < NUMSPRITES; i++)
			if (!sprnames[i][4] && fastncmp(p,sprnames[i],4)) {
				lua_pushinteger(L, i);
				return 1;
			}
		if (mathlib) return luaL_error(L, "sprite '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("SPR2_",word,5)) {
		p = word+5;
		for (i = 0; i < (fixed_t)free_spr2; i++)
			if (!spr2names[i][4])
			{
				// special 3-char cases, e.g. SPR2_RUN
				// the spr2names entry will have "_" on the end, as in "RUN_"
				if (spr2names[i][3] == '_' && !p[3]) {
					if (fastncmp(p,spr2names[i],3)) {
						lua_pushinteger(L, i);
						return 1;
					}
				}
				else if (fastncmp(p,spr2names[i],4)) {
					lua_pushinteger(L, i);
					return 1;
				}
			}
		if (mathlib) return luaL_error(L, "player sprite '%s' could not be found.\n", word);
		return 0;
	}
	else if (!mathlib && fastncmp("sfx_",word,4)) {
		p = word+4;
		for (i = 0; i < NUMSFX; i++)
			if (S_sfx[i].name && fastcmp(p, S_sfx[i].name)) {
				lua_pushinteger(L, i);
				return 1;
			}
		return 0;
	}
	else if (mathlib && fastncmp("SFX_",word,4)) { // SOCs are ALL CAPS!
		p = word+4;
		for (i = 0; i < NUMSFX; i++)
			if (S_sfx[i].name && fasticmp(p, S_sfx[i].name)) {
				lua_pushinteger(L, i);
				return 1;
			}
		return luaL_error(L, "sfx '%s' could not be found.\n", word);
	}
	else if (mathlib && fastncmp("DS",word,2)) {
		p = word+2;
		for (i = 0; i < NUMSFX; i++)
			if (S_sfx[i].name && fasticmp(p, S_sfx[i].name)) {
				lua_pushinteger(L, i);
				return 1;
			}
		if (mathlib) return luaL_error(L, "sfx '%s' could not be found.\n", word);
		return 0;
	}
#ifdef MUSICSLOT_COMPATIBILITY
	else if (!mathlib && fastncmp("mus_",word,4)) {
		p = word+4;
		if ((i = get_mus(p, false)) == 0)
			return 0;
		lua_pushinteger(L, i);
		return 1;
	}
	else if (mathlib && fastncmp("MUS_",word,4)) { // SOCs are ALL CAPS!
		p = word+4;
		if ((i = get_mus(p, false)) == 0)
			return luaL_error(L, "music '%s' could not be found.\n", word);
		lua_pushinteger(L, i);
		return 1;
	}
	else if (mathlib && (fastncmp("O_",word,2) || fastncmp("D_",word,2))) {
		p = word+2;
		if ((i = get_mus(p, false)) == 0)
			return luaL_error(L, "music '%s' could not be found.\n", word);
		lua_pushinteger(L, i);
		return 1;
	}
#endif
	else if (!mathlib && fastncmp("pw_",word,3)) {
		p = word+3;
		for (i = 0; i < NUMPOWERS; i++)
			if (fasticmp(p, POWERS_LIST[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		return 0;
	}
	else if (mathlib && fastncmp("PW_",word,3)) { // SOCs are ALL CAPS!
		p = word+3;
		for (i = 0; i < NUMPOWERS; i++)
			if (fastcmp(p, POWERS_LIST[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		return luaL_error(L, "power '%s' could not be found.\n", word);
	}
	else if (!mathlib && fastncmp("k_",word,2)) {
		p = word+2;
		for (i = 0; i < NUMKARTSTUFF; i++)
			if (fasticmp(p, KARTSTUFF_LIST[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		return 0;
	}
	else if (mathlib && fastncmp("K_",word,2)) { // SOCs are ALL CAPS!
		p = word+2;
		for (i = 0; i < NUMKARTSTUFF; i++)
			if (fastcmp(p, KARTSTUFF_LIST[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		return luaL_error(L, "kartstuff '%s' could not be found.\n", word);
	}
	else if (fastncmp("HUD_",word,4)) {
		p = word+4;
		for (i = 0; i < NUMHUDITEMS; i++)
			if (fastcmp(p, HUDITEMS_LIST[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		if (mathlib) return luaL_error(L, "huditem '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("SKINCOLOR_",word,10)) {
		p = word+10;
		for (i = 0; i < NUMCOLORFREESLOTS; i++) {
			if (!FREE_SKINCOLORS[i])
				break;
			if (fastcmp(p, FREE_SKINCOLORS[i])) {
				lua_pushinteger(L, SKINCOLOR_FIRSTFREESLOT+i);
				return 1;
			}
		}
		for (i = 0; i < SKINCOLOR_FIRSTFREESLOT; i++)
			if (fastcmp(p, COLOR_ENUMS[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		return luaL_error(L, "skincolor '%s' could not be found.\n", word);
	}
	else if (fastncmp("GRADE_",word,6))
	{
		p = word+6;
		for (i = 0; NIGHTSGRADE_LIST[i]; i++)
			if (*p == NIGHTSGRADE_LIST[i])
			{
				lua_pushinteger(L, i);
				return 1;
			}
		if (mathlib) return luaL_error(L, "NiGHTS grade '%s' could not be found.\n", word);
		return 0;
	}
	else if (fastncmp("MN_",word,3)) {
		p = word+3;
		for (i = 0; i < NUMMENUTYPES; i++)
			if (fastcmp(p, MENUTYPES_LIST[i])) {
				lua_pushinteger(L, i);
				return 1;
			}
		if (mathlib) return luaL_error(L, "menutype '%s' could not be found.\n", word);
		return 0;
	}
	else if (!mathlib && fastncmp("A_",word,2)) {
		char *caps;
		// Try to get a Lua action first.
		/// \todo Push a closure that sets superactions[] and superstack.
		lua_getfield(L, LUA_REGISTRYINDEX, LREG_ACTIONS);
		// actions are stored in all uppercase.
		caps = Z_StrDup(word);
		strupr(caps);
		lua_getfield(L, -1, caps);
		Z_Free(caps);
		if (!lua_isnil(L, -1))
			return 1; // Success! :D That was easy.
		// Welp, that failed.
		lua_pop(L, 2); // pop nil and LREG_ACTIONS

		// Hardcoded actions as callable Lua functions!
		// Retrieving them from this metatable allows them to be case-insensitive!
		for (i = 0; actionpointers[i].name; i++)
			if (fasticmp(word, actionpointers[i].name)) {
				// We push the actionf_t* itself as userdata!
				LUA_PushUserdata(L, &actionpointers[i].action, META_ACTION);
				return 1;
			}
		return 0;
	}
	else if (!mathlib && fastcmp("super",word))
	{
		if (!superstack)
		{
			lua_pushcfunction(L, lib_dummysuper);
			return 1;
		}
		for (i = 0; actionpointers[i].name; i++)
			if (fasticmp(superactions[superstack-1], actionpointers[i].name)) {
				LUA_PushUserdata(L, &actionpointers[i].action, META_ACTION);
				return 1;
			}
		return 0;
	}

	for (i = 0; INT_CONST[i].n; i++)
		if (fastcmp(word,INT_CONST[i].n)) {
			lua_pushinteger(L, INT_CONST[i].v);
			return 1;
		}

	if (mathlib) return luaL_error(L, "constant '%s' could not be parsed.\n", word);

	// DYNAMIC variables too!!
	return LUA_PushGlobals(L, word);
}

int LUA_EnumLib(lua_State *L)
{
	if (lua_gettop(L) == 0)
		lua_pushboolean(L, 0);

	// Set the global metatable
	lua_createtable(L, 0, 1);
	lua_pushvalue(L, 1); // boolean passed to LUA_EnumLib as first argument.
	lua_pushcclosure(L, lib_getenum, 1);
	lua_setfield(L, -2, "__index");
	lua_setmetatable(L, LUA_GLOBALSINDEX);
	return 0;
}

// getActionName(action) -> return action's string name
static int lib_getActionName(lua_State *L)
{
	if (lua_isuserdata(L, 1)) // arg 1 is built-in action, expect action userdata
	{
		actionf_t *action = *((actionf_t **)luaL_checkudata(L, 1, META_ACTION));
		const char *name = NULL;
		if (!action)
			return luaL_error(L, "not a valid action?");
		name = LUA_GetActionName(action);
		if (!name) // that can't be right?
			return luaL_error(L, "no name string could be found for this action");
		lua_pushstring(L, name);
		return 1;
	}
	else if (lua_isfunction(L, 1)) // arg 1 is a function (either C or Lua)
	{
		lua_settop(L, 1); // set top of stack to 1 (removing any extra args, which there shouldn't be)
		// get the name for this action, if possible.
		lua_getfield(L, LUA_REGISTRYINDEX, LREG_ACTIONS);
		lua_pushnil(L);
		// Lua stack at this point:
		//  1   ...       -2              -1
		// arg  ...   LREG_ACTIONS        nil
		while (lua_next(L, -2))
		{
			// Lua stack at this point:
			//  1   ...       -3              -2           -1
			// arg  ...   LREG_ACTIONS    "A_ACTION"    function
			if (lua_rawequal(L, -1, 1)) // is this the same as the arg?
			{
				// make sure the key (i.e. "A_ACTION") is a string first
				// (note: we don't use lua_isstring because it also returns true for numbers)
				if (lua_type(L, -2) == LUA_TSTRING)
				{
					lua_pushvalue(L, -2); // push "A_ACTION" string to top of stack
					return 1;
				}
				lua_pop(L, 2); // pop the name and function
				break; // probably should have succeeded but we didn't, so end the loop
			}
			lua_pop(L, 1);
		}
		lua_pop(L, 1); // pop LREG_ACTIONS
		return 0; // return nothing (don't error)
	}

	return luaL_typerror(L, 1, "action userdata or Lua function");
}



int LUA_SOCLib(lua_State *L)
{
	lua_register(L,"freeslot",lib_freeslot);
	lua_register(L,"getActionName",lib_getActionName);

	luaL_newmetatable(L, META_ACTION);
		lua_pushcfunction(L, action_call);
		lua_setfield(L, -2, "__call");
	lua_pop(L, 1);

	return 0;
}

const char *LUA_GetActionName(void *action)
{
	actionf_t *act = (actionf_t *)action;
	size_t z;
	for (z = 0; actionpointers[z].name; z++)
	{
		if (actionpointers[z].action.acv == act->acv)
			return actionpointers[z].name;
	}
	return NULL;
}

void LUA_SetActionByName(void *state, const char *actiontocompare)
{
	state_t *st = (state_t *)state;
	size_t z;
	for (z = 0; actionpointers[z].name; z++)
	{
		if (fasticmp(actiontocompare, actionpointers[z].name))
		{
			st->action = actionpointers[z].action;
			st->action.acv = actionpointers[z].action.acv; // assign
			st->action.acp1 = actionpointers[z].action.acp1;
			return;
		}
	}
}
