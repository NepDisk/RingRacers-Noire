// RadioRacers
//-----------------------------------------------------------------------------
// Copyright (C) 2024 by $HOME
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file radioracers/gameplay/rr_controllerrumble.c
/// \brief Check for more gameplay conditions to vibrate the player's controller too (e.g. rings)

#include "../rr_cvar.h"
#include "../rr_controller.h"
#include "../../doomtype.h"
#include "../../p_local.h"
#include "../../p_mobj.h"
#include "../../d_player.h"
#include "../../k_kart.h"
#include "../../s_sound.h"

int16_t localPlayerWavedashClickTimer = 0;
boolean localPlayerJustBootyBounced = false;

boolean RR_ShouldUseMoreRumbleEvents(void)
{
    return (cv_morerumbleevents.value);
}

rumbleevent_e RR_GetRumbleEvent(const player_t* player)
{
    if (RR_RumbleJustBumpedWall(player) || RR_RumbleJustFastFallBounced(player))
    {
        return EVT_WALL;
    }
    else if (RR_RumbleJustDrifted(player))
    {
        return EVT_DRIFT;
    }
    else if (RR_RumbleIsSpindashingGently(player))
    {
        return EVT_SPINDASH_GENTLE;
    }
    else if (RR_RumbleIsSpindashingViolently(player))
    {
        return EVT_SPINDASH_VIOLENT;
    }
    else if (RR_RumbleOverchargingTailWhip(player))
    {
        return EVT_WHIP_OVERCHARGING;
    }
    else if (RR_RumbleChargingTailWhip(player))
    {
        return EVT_WHIP_CHARGING;
    }
    else if (RR_RumbleJustWaveDashed(player))
    {
        return EVT_WAVEDASHED;
    }
    else if (RR_RumbleRingCollected(player) || RR_RumbleRingConsumed(player))
    {
        return EVT_RING;
    }

    return 0;
}

uint16_t RR_GetRumbleStrength(const rumbleevent_e event)
{
    switch (event)
    {
        case EVT_WALL:
            return (65536 / 2);
        case EVT_WHIP_CHARGING:
        case EVT_SPINDASH_GENTLE:
            return (65536 / 25);
        case EVT_WHIP_OVERCHARGING:
        case EVT_SPINDASH_VIOLENT:
            return (65536 / 10);
        case EVT_WAVEDASHED:
            return (65536 / 3);
        case EVT_DRIFT:
            return (65536 / 100);
        case EVT_RING:
            return (65536 / 160); // Way more subtle than stairjank rumble, cos rings can stack
        default:
            return 0xFFFF;
    }
}

boolean RR_RumbleJustBumpedWall(const player_t *player)
{
    if (!cv_rr_rumble_wall_bump.value)
        return false;
    
    return (player->mo->eflags & MFE_JUSTBOUNCEDWALL);
}

boolean RR_RumbleIsSpindashingGently(const player_t *player)
{
    if (!cv_rr_rumble_spindash.value)
        return false;
    
    return (
            P_IsObjectOnGround(player->mo) &&
            player-> spindash && 
            player-> spindash < K_GetSpindashChargeTime(player) 
    );
}

boolean RR_RumbleIsSpindashingViolently(const player_t *player)
{
    if (!cv_rr_rumble_spindash.value)
        return false;

    return (
            P_IsObjectOnGround(player->mo) &&
            player-> spindash && 
            player-> spindash > K_GetSpindashChargeTime(player) 
    );
}

boolean RR_RumbleRingCollected(const player_t *player)
{
    if (!cv_rr_rumble_rings.value)
        return false;
    return (
        player->mo && 
        player->rings < 20 &&
        player->pickuprings > 0 && player->pickuprings > player->pickuprings-1 && player->pickuprings < player->pickuprings+1
    );
}

boolean RR_RumbleChargingTailWhip(const player_t *player)
{
    if (!cv_rr_rumble_tailwhip.value)
        return false;
    return (
        (player->mo) &&
        player->instaWhipCharge != 0 &&
        player->instaWhipCharge < INSTAWHIP_CHARGETIME
    );
}

boolean RR_RumbleOverchargingTailWhip(const player_t *player)
{
    if (!cv_rr_rumble_tailwhip.value)
        return false;   
    return (
        (player->mo) &&
        player->instaWhipCharge >= INSTAWHIP_CHARGETIME
    );
}

boolean RR_RumbleJustDrifted(const player_t *player)
{
    if (!cv_rr_rumble_drift.value)
        return false;
    
    // Charlie, Charlie! Have I told you how much I LOVE repeated code?!
    const INT32 dsone = K_GetKartDriftSparkValueForStage(player, 1);
    const INT32 dstwo = K_GetKartDriftSparkValueForStage(player, 2);
    const INT32 dsthree = K_GetKartDriftSparkValueForStage(player, 3);
    const INT32 dsfour = K_GetKartDriftSparkValueForStage(player, 4);

    const boolean yellowSparkDrifts = (player->driftcharge >= (dsone - (32*2)) && player-> driftcharge < dsone);
    const boolean redSparkDrifts = (player->driftcharge >= (dstwo - (32*2)) && player->driftcharge < dstwo);
    const boolean blueSparkDrifts = (player->driftcharge >= (dsthree - (32*2)) && player->driftcharge < dsthree);
    const boolean rainbowSparkDrifts = (player->driftcharge >= (dsfour - (32*2)) && player->driftcharge < dsfour);
    
    return (
        player->mo &&
        player->drift &&
        (yellowSparkDrifts || redSparkDrifts || blueSparkDrifts || rainbowSparkDrifts)
    );
}

boolean RR_RumbleRingConsumed(const player_t *player)
{
    if (!cv_rr_rumble_rings.value)
        return false;
    
    const ticcmd_t *cmd = &player->cmd;
    return (
        player->mo &&
        (
            (cmd->buttons & BT_ATTACK) && 
            
            (player->rings > 0 && 
            player->rings > player->rings-1 && 
            player->rings < player->rings+1)
        )
    );
}

/**
 * These checks are kind of hacky. 
 * But I want the rumble to happen the FRAME that the ringboost timer has started.
 * That way, the player gets feedback immediately and knows when to start adjusting lines and the sort.
 */

boolean RR_RumbleJustFastFallBounced(const player_t *player)
{
    if (!cv_rr_rumble_fastfall_bounce.value)
        return false;
    return (
        (player-> mo) &&
        player->fastfall !=0 &&
        P_IsObjectOnGround(player->mo) && 
        localPlayerJustBootyBounced
    );
}

boolean RR_RumbleJustWaveDashed(const player_t *player)
{
    if (!cv_rr_rumble_wavedash.value)
        return false;

    return (
        (player->mo) &&
        P_IsObjectOnGround(player->mo) &&
        localPlayerWavedashClickTimer > 0
    );
}