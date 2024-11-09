// RadioRacers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by $HOME
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file radioracers/rr_menu.h
/// \brief RadioRacers Menu Options

#ifndef __RR_MENU__
#define __RR_MENU__

#include "../k_menu.h"
#include "../m_cond.h"
#include "../command.h"
#include "../console.h"

#include "rr_cvar.h"
#include "rr_controller.h"

#ifdef __cplusplus
extern "C" {
#endif

extern menuitem_t OPTIONS_RadioRacersHud[];
extern menu_t OPTIONS_RadioRacersHudDef;

extern menuitem_t OPTIONS_RadioRacersMenu[];
extern menu_t OPTIONS_RadioRacersMenuDef;

#ifdef __cplusplus
} // extern "C"
#endif

#endif
