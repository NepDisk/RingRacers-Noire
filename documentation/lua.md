# Noire Lua Additions

## Bugfixes
The huddisable drawlist has been fixed so now disabling hud elements should properly work.

## Player Userdata
These are values that were added to the player userdata.

### pogospring
Value type: UINT8

The same as the value from SRB2Kart.
1 is non speedcapped, 2 is speedcapped.

### pogosidemove
Value type: SINT8

Used to store the emulated sidemove for pogospring usage.

### brakestop
Value type: UINT8

The same as the value from SRB2Kart.
Stores a counter that increments when braking with the SRB2Kart movement functionality cvar.

### growcancel
Value type: INT16

The same as the value from SRB2Kart.
Stores a counter that increments when canceling grow with the Kart Grow functionality cvar.

### boostcharge
Value type: UINT8

The same as the value from SRB2Kart.
Stores a counter that increments when doing a legacy race start charge boost.

### cheatchecktime
Value type: tic_t

The same as the value from SRB2Kart under the name starposttime.
The time it was when you hit the last Starpost/Cheatcheck. Used for legacy checkpoint support

### prevcheck
Value type: INT32

The same as the value from SRB2Kart.
Stores the distance from the Previous Legacy Checkpoint.

### nextcheck
Value type: INT32

The same as the value from SRB2Kart.
Stores the distance to the Next Legacy Checkpoint.
