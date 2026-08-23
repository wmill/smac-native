# SMAC Native Roadmap

This is the living plan from the current native map prototype to a complete, playable SMACX
reimplementation. It records shipped work conservatively: a box is checked only when the code,
tests, and documentation for that item are in the repository.

The order below is intentional. Each milestone should leave the game in a demonstrable state and
should deepen the deterministic simulation before adding UI around it.

## What "playable" means

There are three useful finish lines:

| Level | Definition | Target milestone |
|---|---|---|
| Playable alpha | A player can start, play, win, and lose a complete match against AI without using debug tools. | M6 |
| Feature-complete beta | The major SMACX systems and screens needed for normal play are present; remaining work is compatibility, balance evidence, and polish. | M8 |
| 1.0 | Stable macOS/Linux release with documented compatibility, packaging, and no known progression blockers. | M10 |

The initial behavioral baseline is the GOG SMACX 2.0 executable with SHA-256
`01901cbf7196b0c5d0df9540a029520f5df8fd9a6b343deef8b5663872805fcf`. Original game assets
remain user-supplied and are never committed or redistributed.

## Current snapshot

Last reconciled with the repository: 2026-08-22.

**Working now**

- [x] C++20 project with separate deterministic core, format library, SDL client, and inspection CLI
- [x] CMake/Ninja presets, strict warnings, ASan/UBSan preset, and macOS/Linux CI
- [x] Case-insensitive asset discovery, CP1252/CRLF normalization, and baseline executable hashing
- [x] Bounded `TERRANMAP` reader for known header, landmark, tile, and region data
- [x] Staggered map coordinates, horizontal wrapping, polar boundaries, and neighbors
- [x] Owned game state with `MoveUnit`/`EndTurn` commands, events, and a stable state hash
- [x] Deterministic bounded pathfinding and unit-aware movement costs, domains, ZOC, and transport
- [x] Authentic indexed-PCX Planet rendering with cached textures, culling, HiDPI picking, HUD, and
  a selectable native unit
- [x] `verify-data`, `dump-rules`, and `inspect-map` CLI commands
- [x] GPL licensing plus initial format and OpenSMACX provenance notes

**Next incomplete slices**

- The rules reader identifies sections and decodes the road movement rate; it does not yet build a
  complete typed `RulesDatabase` or parse faction definitions.
- Read-only original `.SC`/`.SAV` import and the user-run Windows oracle have not been started.
- CVR vehicle assembly, prototypes, combat, bases, economy, AI, and complete-match systems remain in
  M3 through M6.
- The SDL client is an authentic map-and-movement slice, not yet a complete game UI.

## Milestone map

| Milestone | Outcome | Status |
|---|---|---|
| M0 Foundation | Portable build, owned state, bounded input, basic tools | Complete |
| M1 Authentic map slice | Original-looking map and faithful single-unit movement | Complete |
| M2 Compatibility fixtures | Typed rules plus read-only scenario/save import | Not started |
| M3 Units and combat | Prototypes, workshops, combat, morale, damage, transport | Not started |
| M4 Bases and economy | Bases, yields, production, growth, research, terraforming | Not started |
| M5 Match setup and AI | World generation, faction setup, competent turn-taking AI | Not started |
| M6 Complete match | Diplomacy, social engineering, victory/defeat; playable alpha | Not started |
| M7 Full SMACX systems | Probes, council, satellites, advanced faction mechanics | Not started |
| M8 Complete game UI | Normal gameplay needs no debug/CLI workflow | Not started |
| M9 Compatibility and hardening | Evidence-driven parity, robust imports, performance | Not started |
| M10 1.0 release | Packaged, documented, stable macOS/Linux release | Not started |

## M0 — Foundation

Goal: every later system has a portable, deterministic, testable home.

- [x] Establish `smac_core`, `smac_formats`, `smac-native`, and `smac-tool` targets.
- [x] Keep SDL and original file layouts out of `smac_core`.
- [x] Use fixed-width owned types rather than original packed structures or process addresses.
- [x] Add structured errors and bounded allocation to current binary readers.
- [x] Add pinned optional SDL3, SDL3_image, and SDL3_ttf dependencies.
- [x] Add developer, sanitizer, and release presets.
- [x] Build and test the headless configuration on macOS and Linux CI.
- [x] Add formatting configuration and format the C++ sources.
- [x] Add a CI formatting check.
- [x] Build and smoke-test the SDL client in CI.
- [x] Add fuzz targets for every binary/text reader and run a short corpus in CI.
- [x] Define versioned command/event serialization and deterministic replay logs.
- [x] Expand the stable hash to cover all authoritative state, including map mutations and rules.

Exit gate: CI rejects unsafe input regressions and deterministic command replays produce identical
state hashes on macOS and Linux.

## M1 — Authentic map slice

Goal: make the existing interactive prototype a faithful, useful walking skeleton.

### Formats and rendering

- [x] Describe terrain and unit atlas regions, animation frames, palettes, and transparency in
  declarative metadata.
- [x] Preserve indexed PCX palettes and upload/cache textures without losing palette semantics.
- [x] Decode all tile fields needed to compose altitude, rainfall, rockiness, fungus, rivers,
  resources, improvements, landmarks, and ownership overlays.
- [x] Compose `xplanet.MP` from authentic `ter1.pcx` art with correct layer ordering and seams.
- [x] Render explored/unexplored states, cursor, path preview, and unit selection distinctly.
- [x] Make screen-to-world picking exact at all zoom levels, map seams, and window densities.
- [x] Add texture caching, viewport culling, and stable frame times on the full Planet map.

### Movement slice

- [x] Model chassis/domain and movement allowance instead of a hard-coded land unit.
- [x] Recover movement costs for terrain, fungus, rivers, roads, and mag tubes.
- [x] Enforce poles, wrapping, zones of control, occupancy, embarkation, and domain restrictions.
- [x] Preview routes without moving; confirm movement separately and report rejection reasons in UI.
- [x] Queue command events for animation rather than mutating through mouse handling.
- [x] Restore movement deterministically at the correct turn boundary.

### Acceptance

- [x] Add topology, movement-cost, malformed-map, coordinate-picking, and screenshot tests.
- [x] Render `xplanet.MP` correctly with user-owned assets on Apple Silicon and x86-64 Linux.
- [x] Select and move a native Gaian unit only along legal paths, then refresh it with End Turn.

Acceptance evidence and reproducible commands are recorded in
[`docs/acceptance/M1.md`](docs/acceptance/M1.md).

Exit gate: this is the original first interactive milestone—authentic Planet rendering and faithful,
test-covered movement for one unit, with no combat or economy yet.

## M2 — Typed rules and compatibility fixtures

Goal: remove hard-coded gameplay constants and make original scenarios useful as evidence.

- [ ] Parse all relevant `alphax.txt` sections into typed, validated records while retaining unknown
  fields and source locations.
- [ ] Parse SMACX faction files, including bonuses, priorities, social values, names, and art links.
- [ ] Represent technologies, facilities, projects, abilities, chassis, weapons, armor, reactors,
  terraforming actions, and resources by stable IDs.
- [ ] Diagnose unknown values and cross-reference failures without crashing or silently substituting.
- [ ] Reverse and document read-only `.SC` and `.SAV` containers.
- [ ] Import map, rules identity, factions, bases, units, prototypes, diplomacy, and turn state in
  independently testable stages.
- [ ] Preserve unknown save bytes/records for continuing format investigation.
- [ ] Build synthetic rules, PCX, map, scenario, and save fixtures.
- [ ] Create a user-run Windows oracle that emits JSON traces containing executable hash, seed,
  inputs, and observations; never patch or redistribute binaries.
- [ ] Compare typed rules and focused mechanics against versioned oracle traces.

Exit gate: a headless inspection tool can load the supported rules and a real user-owned scenario,
then print a stable summary without relying on original memory layouts.

## M3 — Units, prototypes, and combat

Goal: support the complete tactical lifecycle of SMACX units.

- [ ] Decode/render CVR vehicle components and assemble chassis, weapon, armor, reactor, and ability
  combinations.
- [ ] Implement standard prototypes and the Unit Workshop design/upgrade/obsolete workflow.
- [ ] Implement land, sea, and air movement, fuel, drops, transport, cargo, and waypoints.
- [ ] Implement morale, lifecycle, home base, support, damage, repair, promotion, and disbanding.
- [ ] Implement conventional and psi combat odds, modifiers, collateral damage, artillery, air
  interception, bombardment, and native lifecycle behavior.
- [ ] Implement zones of control, stacking, base occupancy, capture, and multiplayer-safe combat RNG.
- [ ] Represent every random decision as a deterministic state transition with auditable RNG state.
- [ ] Add combat preview, animation events, combat log, and unit detail/workshop screens.
- [ ] Add focused oracle traces and regression tests for combat edge cases.

Exit gate: two factions can design, produce via a debug command, move, transport, fight, repair, and
destroy all major unit domains in a deterministic combat sandbox.

## M4 — Bases, terrain, and economy

Goal: create the economic engine that makes turns meaningful.

- [ ] Model base founding, ownership, radius, worker assignment, specialists, drones, talents, and
  population growth/starvation.
- [ ] Calculate nutrient, mineral, and energy yields with landmarks, resources, improvements,
  faction effects, technology gates, satellites, and caps.
- [ ] Implement support, maintenance, inefficiency, commerce, psych, labs, economy, and energy bank.
- [ ] Implement production queues, hurry costs, stockpiling, facilities, secret projects, unit
  completion, and retooling penalties.
- [ ] Implement research choice, accumulated labs, technology discovery/trading, and prototype
  availability.
- [ ] Implement colony pods, base capture/destruction, population transfer, and faction elimination.
- [ ] Implement terraforming orders and tile changes: farms, mines, solar collectors, roads, tubes,
  bunkers, sensors, airbases, condensers, boreholes, aquifers, elevation, and fungus.
- [ ] Add base screen, production chooser, citizen management, research report, and terraforming UI.
- [ ] Test yield and production calculations with compact explainable breakdowns and oracle cases.

Exit gate: a human can found and manage bases, improve terrain, grow population, research
technologies, and produce units/facilities/projects across many turns.

## M5 — New game, world generation, and AI loop

Goal: turn the mechanics sandbox into a match that can run without hand-built debug state.

- [ ] Implement deterministic world generation: topology, climate, erosion, rainfall, fungus,
  resources, landmarks, regions, pods, and start-site selection.
- [ ] Add new-game options for map, difficulty, rules, victory conditions, and faction selection.
- [ ] Implement complete turn sequencing, upkeep phases, faction elimination, autosave, and resume.
- [ ] Add a native versioned save format with migration support and atomic writes.
- [ ] Implement reusable AI queries and commands—the AI must use the same legal command boundary as
  human players.
- [ ] Implement AI exploration, colonization, terraforming, production, research, unit design,
  tactical combat, defense, and logistics.
- [ ] Add governor automation as explicit, reversible player choices.
- [ ] Add observer/headless fast-forward modes and AI-vs-AI soak tests.
- [ ] Add difficulty scaling without hidden nondeterministic behavior.

Exit gate: a new randomly generated game with multiple factions can run for hundreds of turns; AI
expands, researches, builds, and fights without deadlocking or corrupting state.

## M6 — Complete match (playable alpha)

Goal: a normal match has a beginning, strategic middle, and supported ending.

- [ ] Implement diplomacy state, communications, treaties, pacts, vendettas, surrender, technology
  and energy exchange, loans, maps, bases, and joint-war requests.
- [ ] Implement social engineering choices, prerequisites, faction aversions/agendas, upheaval cost,
  and all resulting modifiers.
- [ ] Implement planetary ecology, eco-damage, fungal blooms, native spawning, atrocities, sanctions,
  global warming, and sea-level changes.
- [ ] Implement cooperative, conquest, transcendence, economic, and diplomatic victory/defeat checks
  with their enabling rules and projects.
- [ ] Add notifications, reports, event choices, objectives, score, retirement, victory, and defeat UI.
- [ ] Add pause-safe settings, key bindings, confirmations, autosaves, and recovery from interrupted
  saves.
- [ ] Complete one human-versus-AI campaign from new game to every supported victory type.
- [ ] Run deterministic long-game replays and progression-blocker test scenarios in CI.

Exit gate — **playable alpha**: a player can install, select assets, start a game, understand the UI,
play against AI, save/resume, and reach a declared victory or defeat without debug tools.

## M7 — Full SMACX strategic systems

Goal: cover the systems that distinguish full SMACX from the minimum complete match.

- [ ] Implement probe teams: infiltration, theft, sabotage, mind control, framing, algorithmic
  enhancement, and probe combat.
- [ ] Implement Planetary Council proposals, elections, governor powers, voting, bribery, and
  diplomatic victory flow.
- [ ] Implement satellites, orbital limits, interception/defense, and orbital effects.
- [ ] Implement supply crawlers, convoys, gifting, airlifts, nerve stapling, nerve gas, genetic
  warfare, and other special actions.
- [ ] Implement faction-specific mechanics and Alien Crossfire interactions for all fourteen
  factions, including alien diplomacy and energy-grid rules.
- [ ] Implement random events and optional rules with deterministic seeding.
- [ ] Complete facilities, secret projects, technologies, abilities, and rules-driven modifiers.
- [ ] Add scenario objectives, scripted events, scenario victory checks, and scenario selection.
- [ ] Add datalinks and contextual rules/help generated from the loaded database.

Exit gate — **feature-complete simulation**: every major SMACX rules family used in ordinary play is
represented in the deterministic core and reachable through commands.

## M8 — Complete game UI and presentation

Goal: expose the feature-complete simulation as a coherent native game rather than a collection of
developer surfaces.

- [ ] Establish a responsive UI framework with focus, keyboard/controller navigation, modal state,
  scaling, tooltips, and accessibility settings.
- [ ] Finish map overlays, minimap, unit/base cycling, route visualization, orders, reports, and
  contextual actions.
- [ ] Finish base, workshop, research, social engineering, diplomacy, council, espionage, datalinks,
  preferences, load/save, and scenario screens.
- [ ] Add message queues and animation pacing that never alter simulation results.
- [ ] Add SDL_mixer at a pinned version and support user-owned music, sound effects, voice, WVE, and
  volume/channel settings.
- [ ] Decode and play supported FLC cinematics with graceful fallbacks.
- [ ] Support windowed/fullscreen display, HiDPI, common aspect ratios, remappable input, and
  color/contrast options.
- [ ] Add first-run asset selection, validation/remediation, profile/preferences storage, and crash
  recovery messaging.
- [ ] Add screenshot tests for major screens with platform-appropriate text tolerances.

Exit gate — **feature-complete beta**: all ordinary gameplay workflows are available in the native
UI, presentation assets load from the user's installation, and debug tools are optional.

## M9 — Compatibility, correctness, and hardening

Goal: replace broad similarity with measured compatibility and release-grade reliability.

- [ ] Maintain a behavior matrix for the baseline executable, documented deliberate differences,
  and unresolved questions.
- [ ] Expand oracle coverage for economy, combat, AI-visible rules, diplomacy, ecology, and victory.
- [ ] Import representative unmodified `.SC` and `.SAV` files read-only with clear diagnostics.
- [ ] Decide separately whether writing original save formats is sufficiently understood and safe;
  native saves do not depend on this decision.
- [ ] Fuzz all parsers continuously and test allocation, integer-overflow, and truncation limits.
- [ ] Add deterministic replay compatibility tests across compilers, platforms, and save versions.
- [ ] Add performance budgets for rendering, pathfinding, world generation, AI turns, and saves.
- [ ] Add long-running soak, corrupted-save, abrupt-shutdown, missing-asset, and low-resource tests.
- [ ] Audit licenses, provenance, dependencies, third-party notices, and absence of proprietary data.
- [ ] Add opt-in anonymized crash diagnostics only if a privacy-preserving design is agreed upon.

Exit gate: no known progression blockers or critical data-loss issues; supported imports, deliberate
differences, and performance expectations are documented.

## M10 — 1.0 release and beyond

Goal: make the game straightforward to obtain, run, troubleshoot, and maintain.

- [ ] Produce signed/notarized macOS universal packages and reproducible x86-64 Linux packages.
- [ ] Document supported platforms, asset sources/versions, installation, controls, saves, known
  differences, troubleshooting, and contributor setup.
- [ ] Add release CI, dependency/license manifests, checksums, changelog, and upgrade tests.
- [ ] Conduct release-candidate campaigns on Apple Silicon macOS and x86-64 Linux.
- [ ] Publish 1.0 only after fresh-install and complete-campaign acceptance passes.
- [ ] Add classic SMAC mode after the SMACX superset is stable.
- [ ] Evaluate deterministic multiplayer only after save/replay compatibility is mature.
- [ ] Consider original-format save writing only when its round-trip behavior is fully evidenced.

## Cross-cutting rules

These apply to every milestone:

- **Determinism:** rules change only through commands; events describe results; RNG state is owned,
  serialized, and tested.
- **Evidence:** record every OpenSMACX adaptation and executable observation in the provenance ledger.
  Bug fixes and enhancements are explicit choices, never silent inheritance.
- **Input safety:** all external files use bounded readers, structured errors, allocation limits, and
  malformed/truncated-input tests.
- **Separation:** `smac_core` has no SDL or original packed types; presentation may consume events but
  cannot become authoritative game state.
- **Legal hygiene:** no original executable, art, audio, text, maps, fonts, or derived fixture is
  committed. CI uses synthetic data.
- **Incremental demos:** keep headless tests and the last playable slice working while building the
  next system.
- **Scope control:** online multiplayer, original-format save writing, and classic-mode divergence
  must not delay the SMACX single-player 1.0 path.

## How to maintain this file

When a change lands:

1. Check its box only when implementation, proportionate tests, and relevant format/provenance notes
   are complete.
2. Update the current snapshot date and milestone status if an exit gate changed.
3. Add newly discovered work under the milestone that depends on it; do not hide it in a vague
   “polish” bucket.
4. Put detailed reverse-engineering evidence in `docs/formats/` or `docs/provenance/`, then link it
   here if it changes sequencing or scope.
5. Prefer small issues that close one checkbox or a clearly named part of one checkbox.

The immediate critical path is **M2 typed rules/imports → M3 combat → M4 economy → M5 AI/game
setup → M6 complete match**.
