# Noire Cvars

## Client Side Cvars:

### Video Menu

#### gr_paletterendering
Default Value: "Off"

Choices: "Off", "On"

Sets if the a recreation of software's palette rendering will be used.

#### gr_palettedepth

Default Value: "16 bits"

Choices: "16 bits", "24 bits"

Decides what how many bits should be used in palette rendering. "16 bits" is what software looks like in game and "24 bits" is what software looks like in screenshots.

### Noire Menu

#### holdscoreboardbutton
Default Value: "Off"

Choices: "Off", "On"

Sets if you are required to hold the rankings button to keep it open just like SRB2Kart.

#### colorizedhud
Default Value: "On"

Choices: "Off", "On"

Sets if the colorized hud is enabled.

#### colorizeditembox
Default Value: "On"

Choices: "Off", "On"

Sets if the itembox should be colored in the hud.

#### darkitembox
Default Value: "On"

Choices: "Off", "On"

Sets if the itembox should darken with certain items.

#### colorizedhudcolor
Default Value: "Skin Color"

Sets what color to set the colorized hud to.

#### oldinputdisplay
Default Value: "On"

Choices: "Off", "On"

Sets if the old SRB2Kart input display should be used.

#### oldpositiondisplay
Default Value: "On"

Choices: "Off", "On"

Sets if the old SRB2Kart position number should be used.

#### extendedspeedometer
Default Value: "Off"

Choices: "Off", "On"

When enabled and not using the percent speedometer, it will draw the precent speedometer next to your current speedometer.

#### highresportrait
Default Value: "Off"

Choices: "Off", "On"

When enabled the hud will use the higher resoultion face picture in some spots.

#### saltyhop
Default Value: "Off"

Choices: "Off", "On"

When enabled the player will visually hop when driting. This hop is only visual.

#### menumusic
Default Value: ""

Set this value to the lump name of an audio track to use that for the title screen music.

#### spectatormusic
Default Value: "Off"

Choices: "Off", "On"

#### spectatormusiclump
Default Value: ""

Set this value to the lump name of an audio track to use that for the spectator music.

#### postracemusic
Default Value: "Off"

Choices: "Off", "On"

When enabled, the game will play music after races just like SRB2Kart

#### postracejingles
Default Value: "Off"

Choices: "Off", "On"

When enabled, the game will jingles after races

#### postracevoices
Default Value: "Off"

Choices: "Off", "On"

When enabled, the game will play a charcter voice line just like SRB2Kart

#### cheatywarning
Default Value: "Off"

Choices: "Off", "On"

Sets if the Cheats menu should show a warning or not.

#### flipcam - flipcam4
Default Value: "Off"

Choices: "Off", "On"

sets if flipcam is enabled, this is profile tied so its probably better to set there

#### flipcam - flipcam4
Default Value: "Off"

Choices: "Off", "On"

sets if flipcam is enabled, this is profile tied so its probably better to set there

#### oldtricks - oldtricks4
Default Value: "Off"

Choices: "Off", "On"

sets if oldtricks is enabled, this is profile tied so its probably better to set there

## Gameplay Cvars
These are Cvars the affect the actual gameplay. These values are netsynched but not saved.

### Rings
Cvars here all relate to rings.

#### ng_rings
Default Value: "On"

Choices: "Off", "On"

This Cvar is a master toggle for all things ring-related. Setting this to off will disable most ring related options.

#### ng_ringcap
Default Value: 20

The total amount of rings a player is allowed to have at one time.

#### ng_spillcap
Default Value: 20

Choices: "Off", "On"

The maximum amount of rings to be dropped at once.

#### ng_ringdebt
Default Value: "On"

Choices: "Off", "On"

Decides if the player should be allowed to go into ring debt.

#### ng_ringsting
Default Value: "On"

Choices: "Off", "On"

Decides if the player should be spun-out if they touch players with negative rings.

#### ng_ringdeathmark
Default Value: -20

The Minimum amount of rings needed before the player dies from damage.

#### ng_maprings
Default Value: "On"

Choices: "Off", "On"

Sets if rings can spawn on map start.

#### ng_mapringcapsules
Default Value: "On"

Choices: "Off", "On"

Sets if ring capsules can spawn on map start.

#### ng_mapringboxes
Default Value: "On"

Choices: "Off", "On"

Sets if map-placed ringboxes can spawn on map start.

#### ng_ringboxtransform
Default Value: "On"

Choices: "Off", "On"

Sets if itemboxes turn into ringboxes on pickup.

#### Items
Cvars here all relate to items

#### ng_forceoldboxscale
Default Value: "Off"

Choices: "Off", "On"

A toggle that allows the player to force smaller itemboxes regardless of mapheaders. Useful for testing maps without editing mapheaders.

#### ng_capsules
Default Value: "On"

Choices: "Off", "On"

Sets if capsules should spawn on race start.

#### ng_oldorbinaut
Default Value: "Ring Racers"

Choices: "Ring Racers", "SRB2Kart"

Decides behaviour of Orbinauts.

SRB2Kart Orbinauts go backwards slowly and do not orbit in a circle.

#### ng_oldjawz
Default Value: "Ring Racers"

Choices: "Ring Racers", "SRB2Kart"

Decides behaviour of Jawz.

SRB2Kart Jawz go backwards slowly and don't home-in on the person who threw it.

#### ng_oldhyudoro
Default Value: "Ring Racers"

Choices: "Ring Racers", "SRB2Kart"

Decides behaviour of Hyudoro.

SRB2Kart Hyudoro instantly steal player items and makes the stealer a ghost.

#### ng_oldballhog
Default Value: "Ring Racers"

Choices: "Ring Racers", "SRB2Kart"

Decides behaviour of the Ballhog.

SRB2Kart Ballhog throws all ammo at once and bounces off walls.

#### ng_oldspb
Default Value: "Ring Racers"

Choices: "Ring Racers", "SRB2Kart"

Decides behaviour of the SPB.

SRB2Kart SPB instantly targets first place and flies to them rather then driving the track beforehand.

#### ng_oldinvincibility
Default Value: "Ring Racers"

Choices: "Ring Racers", "SRB2Kart"

Decides behaviour of Invincibility.

SRB2Kart Invincibility does not stack the duration when used multiple times and does not go faster on larger duration.

#### ng_oldgrow
Default Value: "Ring Racers"

Choices: "Ring Racers", "SRB2Kart"

Decides behaviour of Grow.

SRB2Kart Grow makes its user's itembox held hostage and prevents item pickups and ring usage. The player also now gets a 20% speed boost but can no longer sliptide with it. Duration also does not stack when used mulitple times.

#### ng_oldshrink
Default Value: "Ring Racers"

Choices: "Ring Racers", "SRB2Kart"

Decides behaviour of Shrink.

SRB2Kart Shrink causes players ahead of the user to shrink down in size instantly on use and makes them drop their  current items.

#### ng_oldeggman
Default Value: "Ring Racers"

Choices: "Ring Racers", "SRB2Kart"

Decides behaviour of Shrink.

SRB2Kart Shrink causes players ahead of the user to shrink down in size instantly on use and makes them drop their  current items.

#### ng_nerfdroptarget
Default Value: "Off"

Choices: "Off", "On"

Reduces the droptargets applied knockback to be more fair.

#### ng_nerfflameshield
Default Value: "Off"

Choices: "Off", "On"

Applies a diminishing speed equation onto the flameshield to to try to make it more fair.

#### ng_flameshield_diminish
Default Value: "1.2"

Value used in diminishing speed equation used on flameshield.

#### ng_airtimerdrain
Default Value: "Off"

Sets if being in the air will drain timer items.

Grow and Invincibility timers freeze while in the air, this makes it count down again.

#### ng_itemodds
Default Value: "Ring Racers"

Choices: "Ring Racers", "Noire", "Custom"

Allows setting what odds table is used for item rolls.

Noire is more balanced around the old item toggles like removing Hyudoro from first for example.

To use custom odds please use the itemodds.cfg console script in serverscripts inside the repo. This makes it way easier to set odds up.

#### ng_forceautoroulette
Default Value: "Off"

Choices: "Off", "On"

Decides if Auto Roulette should be forced on everyone. This activates it regardless of current profile settings. Can be useful for custom server balance.

#### ng_itemoddsdistvar
Default Value: 2048

Magic distance value that is used in the item odds system calculations. For a good idea of what this does enable debugitemodds and look what it does.

### Mechanics
Cvars here all related to general game mechanics.

#### ng_fastfall
Default Value: "On"

Choices: "Off", "On"

Sets if the player should be allowed to fast fall.

#### ng_fastfallbounce
Default Value: "On"

Choices: "Off", "Bubble Shield". "On"

Decides if the fast fall bounce should work. "Bubble Shield" Makes it only bounce when the Bubble Shield is active.

#### ng_draft
Default Value: "On"

Choices: "Off", "On"

Sets if the player should be allowed to draft off other players.

#### ng_wavedash
Default Value: "On"

Choices: "Off", "On"

Sets if the player should be allowed to wavedash.

#### ng_tumble
Default Value: "On"

Choices: "Off", "On"

Sets if the player should be affected by tumble.

If tumble is disabled, Invincibility will spinout instead.

#### ng_stumble
Default Value: "On"

Choices: "Off", "On"

Sets if the player should be affected by stumble.

If stumble is disabled, Grow will spinout instead.

#### ng_hitlag
Default Value: "On"

Choices: "Off", "On"

Sets if the player should be affected by hitlag.

#### ng_combo
Default Value: "On"

Choices: "Off", "On"

Sets if the player should be allowed to be combo'd by damage sources.

#### ng_mapanger
Default Value: 2

Value used to determine how many times a map needs to get ignored before it votes for itself.

Setting this value to 0 disables this.

#### ng_tripwires
Default Value: "On"

Choices: "Off", "On"

Determines if tripwires should be on the map during map load.

#### ng_forcenoposition
Default Value: "On"

Choices: "Off", "On"

A toggle that allows the player to force legacy start regardless of mapheaders. Useful for testing maps without editing mapheaders.

#### ng_dospecialstage
Default Value: "On"

Choices: "Off", "On"

Decides if special stages occur in GrandPrix.

#### ng_lives
Default Value: "On"

Choices: "Off", "On"

Decides if Grandprix should use lives.

#### ng_continuesrank
Default Value: "On"

Choices: "Off", "On"

Decides if lives affect ranking in GrandPrix.

#### ng_instawhip
Default Value: "On"

Choices: "Off", "On"

Sets if the player should be allowed to use the instawhip.

#### ng_instawhipcharge
Default Value: 75

Sets the amount of time needed to instawhip.

#### ng_instawhiplockout
Default Value: 70

Sets the length of the timeout after using the instawhip

#### ng_instawhipdrain
Default Value: "On"

Choices: "Off", "On"

Sets if the player's instawhip drains rings.

#### ng_spindash
Default Value: "On"

Choices: "Off", "On"

Sets if the player should be allowed to spindash.

#### ng_spindashthreshold
Default Value 6

Value used to determine if the player can spindash. If player speed is lower then this value, then you can spindash.

#### ng_spindashcharge
Default Value: 0

Amount of tics it takes for the spindash to fully charge.
Setting 0 goes back to default game calculation.

#### ng_spindashoverheat
Default Value: "On"

Choices: "Off", "On"

Sets if overcharging the spindash hurts the player.

#### ng_slopephysics
Default Value: "On"

Choices: "Off", "On"

Sets if slope physics will used for going up and down slopes.

#### ng_sloperesistance
Default Value: "On"

Choices: "Off", "On"

When disabled this makes it slighty easier to climb slopes.

#### ng_slopehelper
Default Value: "Off"

Choices: "Off", "On"

This is a hardcoded version of Indev's SlopeHelper script.
When enabled this give the player a boost when going up slopes to make them less harsh.
Good for when you want the downward slopeboost but not the slowdown from going up.
Also good for ringless setups as it makes it possible to climb steep slopes without boosts.

#### ng_slopehelper_speedboost
Default Value: "1.2"

Speed value used for sloehelper speedboost calculation.

#### ng_slopehelper_accelboost
Default Value: "1.25"

Accel value used for sloehelper speedboost calculation.

#### ng_stairjank
Default Value: "All"

Choices: "None", "Bumpy roads only", "All"

Sets if how starjank should affect the player. "Bumpy roads only" only does it on terrain and not stairs.

#### ng_turnstyle
Default Value: "Vanilla"

Choices:  "V1", "V2 Indev", "Tweaked", "Vanilla"

Sets what the current style of turning used is.
"V1" is how turning is in SRB2Kart, "V2 Indev" is how turning was in SRB2Kart v2 indev builds, "Tweaked" is a modifed version of the Vanilla Ring Racers turning.

#### ng_underwaterhandling
Default Value: "On"

Choices: "Off", "On"

Sets if underwater should control differently then on land.

#### ng_oldpogooverride
Default Value: "Off"

Choices: "Off", "On"

Sets if springs and spring terrain act like pogosprings. This is mostly to use as testing less so seriously.

#### ng_nophysicsflag
Default Value: "Off"

Choices: "Off", "On"

When enabled, when on a sector marked as nophysics you will not ramp off it.

#### ng_neptuneboostpower
Default Value: "Off"

Choices: "Off", "On"

When enabled, the game's boostpower calculations will use speed values inspired by SRB2Kart Neptune's stacking system.

#### ng_srb2kartmovement
Default Value: "Off"

Choices: "Off", "On"

When enabled, the game will use SRB2Karts ground speed cap.
This makes offroad stronger, makes grounded thrusts weaker, and makes the garden top only go your top speed. This may break setpieces in maps.

#### ng_airspeedcap
Default Value: 50

Sets what the maximum airspeed cap for players is. SRB2Kart uses 75. When this value is set to 0 it turns off the cap.

#### ng_botrubberbandboost
Default Value: "On"

Choices: "Off", "On"

Decides if bots should do rubberbanding with top speed.

#### ng_charsetrivals
Default Value: "On"

Choices: "Off", "On"

Sets if rivals are random, or the ones defined inside the character.

#### ng_rivals
Default Value: "On"

Choices: "Off", "On"

Decides if rivals are enabled in GrandPrix.

#### ng_rivaltopspeed
Default Value: 11

Sets a rival's top speed. 10 is similar to what regular bots use.

#### ng_rivalringpower
Default value 20

Sets a rival's ring speed. 10 is similar to what regular bots use.

#### ng_rivalfrantic
Default Value: "On"

Choices: "Off", "On"

Decides if bots should roll crazier items or not.

#### ng_rivaldraft
Default Value: "On"

Choices: "Off", "On"

Decides if rivals should have 2x drafting power
