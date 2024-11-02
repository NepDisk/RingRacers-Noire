# Noire Terrain Additions

## Terrain_t Additions

### pogoSpring
A recreation of the SRB2Kart pogospring sector special.

A value of 1 is uncapped speed and a value of 2 is capped speed.

### pogoMinSpeed / pogoSpringMinSpeed
Allows the user to set minimum speed this pogospring sets you to when touched. Useful for large gaps that need speed to cross.

Default value is 24

The calculation for the speed obtained is "value * mapobjectscale"

### pogoMaxSpeed / pogoSpringMaxSpeed
Allows the user to set a capped speed this pogospring sets you to when touched. Useful for preventing players from going too fast off some jumps.

Default value is 28

The calculation for the speed obtained is "value * mapobjectscale"
