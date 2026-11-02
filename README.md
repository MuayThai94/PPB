# Precision Physic Bodies 2.9.0 (Beta) + OStrands bridge

`package/` is the Precision Physic Bodies 2.9.0 Beta release (mad72), unchanged, with one addition: a new FOMOD
page, **OStrands**, that installs `package/40 OStrands Bridge`. The bridge connects PPB's touch-driven erection
to OStrands 0.8.0:

1. PPB raises the player's schlong while a hand is on it (`genBend` / `genBendPlayer`, unchanged).
2. While it is raised, the bridge measures how far a gripping hand travels **along the shaft**. Only movement
   relative to the shaft counts, so walking or turning with a still hand does nothing.
3. When the travel reaches `ClimaxStrokeU` (400 u by default, about 40 full strokes), the bridge triggers an
   OStrands climax: the real OStrands semen comes from the tip. Every actor within `CollisionRangeU` is passed in
   as a collision body, so the fluid lands on them instead of passing through.
4. The spurts' path is swept with OStrands' own gravity and pulse count. Every NPC body the path reaches gets a
   semen decal (the game's own skinned decals, the same kind blood uses).
5. The bridge sends the mod event `PPB_OStrands_Climax` (`numArg` = stroke travel, `sender` = the player) so
   other mods can react.

Settings are in `SKSE/Plugins/PPB_OStrands.ini`. The file is hot-reloaded within a second and every key is
documented in it. The log is `My Games/Skyrim VR/SKSE/PPB_OStrands.log`.

## Requirements

- Precision Physic Bodies 2.9.0, with the player schlong rig on (`genBendPlayer 1`, the default).
- **OStrands 0.8.0** (`0_OStrands.dll`), plus OStim Standalone or SexLab, because OStrands needs one of them.
- In `OStrands_User.ini` (or OStrands' SKSE Menu Framework page), set:
  ```ini
  [Semen]
  Enabled = true
  ```
  With `auto` and no OCum, OStrands refuses semen outside SexLab scenes. The bridge logs a warning about this
  at startup.
- Skinned decals turned on in the game (`bDecals=1`, `bSkinnedDecals=1`, which is vanilla's default).

## Files

| Path | What |
|---|---|
| `OStrandsBridge/` | Source for `PPB_OStrands.dll` (CommonLibVR NG, CMake + vcpkg) |
| `package/40 OStrands Bridge/PPB_OStrands.esp` | 3 forms, no masters: decal texture set, decal impact, impact set |
| `package/40 OStrands Bridge/textures/PPB_OStrands/` | Splat decal (2×2 atlas) + normal/specular map |
| `package/40 OStrands Bridge/SKSE/Plugins/PPB_OStrands.ini` | Settings |
| `tools/make_esp.py`, `tools/make_decal_textures.py` | Regenerate the ESP and textures (deterministic) |
| `.github/workflows/build-ostrands-bridge.yml` | Builds the DLL on a Windows runner and uploads it plus the full package as artifacts |

## Building PPB_OStrands.dll

The DLL is **not committed**. Either download the `PPB_OStrands-dll` artifact from the workflow run, or build it
on Windows with Visual Studio 2022 (C++ workload), CMake 3.24 or newer, and vcpkg:

```bat
cd OStrandsBridge
set VCPKG_ROOT=C:\path\to\vcpkg
cmake --preset release
cmake --build --preset release
copy build\PPB_OStrands.dll "..\package\40 OStrands Bridge\SKSE\Plugins\"
```

Build only in Release. A Debug STL changes `std::vector`'s size, and the source `static_assert`s on that.

## How the bridge reaches OStrands (it has no API)

OStrands only climaxes when OStim or SexLab tells it to. Both bridges, and its own test key, call one internal
function, `FluidManager::QueueClimax(self, threadId, actorHandle, vector<Pair>, vector<ActorHandle>)`. That
function locks OStrands' mutex and moves the two vectors into its pending queue. Its main-thread update then runs
the climax. With an empty pair list, OStrands takes its "no penetration" path: a free spurt from the tip.

`OStrandsLink.cpp` calls that function at its address inside `0_OStrands.dll` **0.8.0** (SHA-256
`93db60ed2b322c460855b58bf3f462a9b6c40580cc5cd8f0059086f68c55855e`). Before binding, it checks the first 24 bytes
of both functions it uses. **Any other OStrands build turns the OStrands half off** (logged). The game does not
crash, and stroking and decals keep working. The vectors it hands over are allocated from `ucrtbase`'s heap,
because OStrands frees them with its own `operator delete`.

**When OStrands updates**, the two RVAs and signatures in `OStrandsLink.cpp` need re-mapping. Find
`OStimBridge::TestClimax` (its log string is `"test key: climax sent to {} actor(s)"`). It calls the
`FluidManager` getter and then `QueueClimax` with `edx = 0`.

## PPB integration notes

`PPB.dll` is closed source, so the bridge does not patch it. It reads the same skeleton PPB bends
(`NPC Genitals01..06`), and listens for PPB's own `PPB_PlayerMasturbation` mod event. PPB sends that event when
a hand leaves the groin at full erection. Set `TriggerOnPPBRelease = 1` to climax on that event as well.

## Not yet verified in game

Everything here was built from the two release archives. **None of it has been run in Skyrim VR yet**, and the
DLL has not been compiled with MSVC in this repository (the CI workflow does that). Check these first:

- [ ] `PPB_OStrands.log` shows `OStrands: 0.8.0 recognised` and `decals: impact set ... ready`.
- [ ] Stroking with `DebugLog = 1`: STROKE lines climb only while the hand moves along the shaft.
- [ ] At climax, `OStrands.log` shows `... climaxes in thread 7340: no penetration`, and semen leaves the tip.
- [ ] Decals appear on NPCs in front of you. If they land short of or past OStrands' fluid, tune
      `[Decals] LaunchSpeedU`. If no decal appears at all, the impact pick may not be reaching the NPC body (see
      `PathDecals.cpp`). Report it with a DebugLog excerpt.
