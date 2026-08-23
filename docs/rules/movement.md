# M1 movement behavior

Movement points use the original engine's road-rate units: the stock `alphax.txt` road movement rate
is three, so an ordinary one-speed move costs three and a road/river move costs one. Chassis speed is
converted to movement points when a unit is created and restored at `EndTurn`.

The M1 cost order follows OpenSMACX `hex_cost` (`src/veh.cpp`, recovered original offset `0x593510`):

1. Sea and air domain handling.
2. Connected mag tubes or bases cost zero.
3. Connected roads or bases cost one.
4. Native life and Xenoempathy movement into fungus costs one.
5. A diagonal move along connected rivers costs one.
6. Hovertanks and antigravity units ignore ordinary terrain penalties.
7. Ordinary terrain costs the road rate, with another road rate for rocky terrain or forest and two
   more road rates for conventional movement into fungus.

Sea units are restricted to ocean and land units require a friendly transport with free capacity to
enter ocean. Embarked passengers follow their carrier and can disembark onto adjacent land. Air units
can enter either terrain domain. A destination occupied by a hostile unit is rejected because combat
is outside M1; friendly stacking remains legal. Conventional land units cannot move directly from
one enemy zone-of-control tile to another.

Horizontal wrapping and polar rejection come from `WorldMap`, so commands and Dijkstra previews use
the same topology. The pathfinder calls the same unit-aware move evaluator as command execution and
uses a stable coordinate tie-breaker.

Deliberate M1 limits: diplomacy is not modeled, so different faction IDs are hostile; bases do not
yet have owned records; damage, morale, projects other than the represented Xenoempathy trait, and
prototype/rules-driven chassis records arrive in later milestones.
