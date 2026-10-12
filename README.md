# Nearest Quest Tracker

A Fallout 4 mod: one hotkey puts a marker on the nearest place to go - the nearest Pip-Boy quest objective, the
start of a quest you have not started yet, an undiscovered location or a building you have never been inside -
and removes the other markers. A window lists all quests by category and all undiscovered places, with mouse,
keyboard and gamepad controls.

Русское описание - [README.ru.md](README.ru.md).

## Known issues: the mod is buggy

And it cannot be fully fixed:

- **It is impossible to track every condition under which a quest can be picked up.** They are spread over the
  game's dialogue, scenes, triggers and scripts, different for every quest. So a new-quest marker sometimes leads
  to a quest giver who cannot give the quest right now (asleep behind a locked door, too early, something else must
  be done first). Exclude such a target with one key. Since 2.1 the mod recognizes the commonest cases by itself -
  see "Starts that cannot be taken now" below.
- **Quests in Fallout 4 are often buggy themselves**: sometimes quests refuse to start, and sometimes they refuse
  to end. The mod shows what the game gives, these bugs included.

## Features

**Mark nearest** puts a marker on the nearest target and removes the other markers. What counts as a target is
set in MCM:

- **Pip-Boy quests**, Miscellaneous objectives included; optionally also quests without a marker.
- **New quests** - places where quests you have not started yet begin: a quest giver, a note, a radio transmitter.
  98 start points for the base game, Far Harbor and Nuka-World. A "No spoilers" mode keeps the quest's name off the
  marker.
- **Radiant quest givers** - Preston Garvey, Proctor Quinlan, Lancer Captain Kells, PAM, Doctor Carrington, the
  Institute scientists and others (33 repeatable quests), but only while the giver really has a task for you: the
  conditions of the giver's offer line are checked by the game engine itself.
- **Undiscovered locations** - those on your map or compass, optionally any. Settlements are skipped.
- **Interiors you have never been inside** - buildings, basements, caves; the marker goes on the door outside.
  Doors that need a key, and optionally locked doors that are a crime to pick, are skipped.

**Starts that cannot be taken now** are not picked and are shown with the reason in the window:

- **another quest holds the place the quest needs.** Some quests are started by the game in the background (on
  entering a location) and must already run for the giver to offer them. If a place they need is reserved by
  another quest, they cannot start. Example: Abbot's *Painting the Town* never starts while *Quartermastery* holds
  Hardware Town - the window says which quest is in the way. Misc pointers to such quests ("Talk to Abbot at the
  wall") are not picked either;
- **the faction is hostile** - kicked out of the Brotherhood, the Railroad or the Institute, the Minutemen after
  Nuka-World raids, Far Harbor, Acadia, the Children of Atom. The real faction state is read, so a mod that keeps a
  faction friendly keeps its quests;
- **the quest giver is hostile** to you (attacked, wanted, hostile faction).

Quest distance follows **the route the compass marker takes, through doors**, not the straight line. Targets in
your current world come first, then the Commonwealth, Far Harbor and Nuka-World.

**The window** (the game pauses while it is open):

- **Quests** tab: a category tree like the Fallout Wiki's (main, faction, side, DLC, other mods' quests) with
  "done/total" counters; the quests with world, distance and state (in the Pip-Boy, can be started, not started,
  completed, blocked by another quest, hostile faction or giver, started by the game but not taken yet - with the
  place it holds). Filters: state, "with a marker only", world.
- **Search by part of the name**: press F (as in FallUI's barter search), type, Enter applies, Esc clears.
- **Locations** tab: undiscovered locations and unvisited interiors. Filters: seen / all, locations / interiors,
  world.
- Click a row to mark it; the row's button excludes or restores it. Mouse, keyboard and gamepad (hints at the
  bottom).
- Opens by a hotkey, by a button in MCM, or with the item **"Nearest Quest Tracker"** (Aid, weightless): put it into
  your favorites and the window opens with one gamepad button. The item returns to the inventory after use.

Hotkeys (MCM, page *Nearest Quest Tracker*, none bound by default): **Mark nearest**, **Exclude marked** (marks the
next nearest one; completed quests, discovered locations and visited interiors leave the exclusion list by
themselves), **Restore last excluded** (MCM also lists all excluded targets), **Show current quest** (the tracked
quest and what to do now), **Toggle automatic mode**, **Open window**.

Settings:

| Setting | Default | |
|---|---|---|
| Pick active quests | on | Pip-Boy quests with a marker |
| Quests without a marker | off | also Pip-Boy quests without a marker, after the ones with a marker |
| Pick new quests | off | start points of quests not started yet |
| Pick radiant quest givers | on | givers of repeatable quests while they have a task for you |
| No spoilers | off | a new-quest marker says just "New quest" |
| Pick undiscovered locations | off | map locations not discovered yet |
| All locations (spoilers) | off | any undiscovered location, not only those on the map or compass |
| Pick interiors | off | interiors you have never been inside |
| Skip locked doors of others | on | skip interiors behind a locked door that is a crime to pick |
| Only one marker | on | the hotkey removes all other markers |
| Automatic | off | the nearest target is marked by itself when an objective is completed, a quest starts, a location is discovered or an interior entered |
| Notifications | on | name the target that is now marked |
| Current quest in a window | off | "Show current quest" uses a message window instead of notifications |

Texts are in English or Russian, by the game's `sLanguage`.

## How it works

Papyrus cannot list the Pip-Boy quests or the positions of their objectives, so the work is done by an
F4SE plugin, `NearestQuestTracker.dll` (source in `f4se_plugin/`, CommonLibF4). Engine details come from
disassembling `Fallout4.exe` 1.10.163 with its PDB; more in `PLAN-1.0.md` and `PLAN.md` ("Decisions and findings").

- **Distance.** For every displayed quest target the engine keeps a path from the player
  (`PlayerCharacter::questTargets`, a `TeleportPath` that goes through doors) and rebuilds it every few
  seconds - this is what the compass marker follows. The mod measures that same path, so a quest behind a
  door is as far as the walk to it, not the straight line. Start points, locations and interiors in another world
  are measured through the engine's own paths into that world. Child worlds with their own coordinates (Diamond
  City) are shifted into their parent world's coordinates.
- **Order by world.** The player's world first, then the Commonwealth, Far Harbor, Nuka-World, other worlds. Within
  a world - by distance; quests without a marker (if enabled) come after the ones with a marker.
- **Tracking** is `Quest.SetActive`, the same as the Pip-Boy track button. Start and location markers are hidden
  Miscellaneous quests of the plugin (`NQT_StartMarkerQuest`, `NQT_LocationMarkerQuest`) whose objective points at
  an alias; the script moves the alias, the DLL rewrites the objective text.
- **Quest starts.** `QuestStarts.json` is generated by `tools/gen_quest_starts.py` from the hand-written list
  `tools/quest_starts.txt` and an index of the masters (`tools/esm_index.py`).
- **Starts that cannot be taken.** A quest that must already run to be offered is checked for a required location
  alias held by a reserving alias of another running quest (`TESQuest::FindLocForAlias` checks only the
  reservation for a specific location, not "cleared"). Faction hostility is the engine's own
  `TESFaction::GetPCAnEnemy` plus the fight reaction between the faction and `PlayerFaction`; a giver's hostility is
  `Actor::CalculateFactionFightReaction`, or the factions of its base NPC while the giver is not in memory.
- **Radiant givers.** The giver's offer lines (`tools/radiant_offers_research.py` finds them) are evaluated with
  `TESConditionItem::IsTrue` - subject the giver, target the player, scope quest the line's quest - while that quest
  runs.
- **Categories.** `QuestCategories.json` is generated by `tools/gen_quest_categories.py` from the Fallout Wiki quest
  tables; quests the wiki does not list go to "Other quests", one subcategory per plugin.
- **Locations** are the map markers of all worlds (their "visible" / "can travel" flags); **interiors** use the
  Pip-Boy local map fog of war (the cell's `ExtraSeenData`, saved in the save): never uncovered = never visited.
- **The window** is an own Scaleform menu (`menu/NQTMenu.as`, compiled into a SWF by JPEXS FFDec), registered with
  F4SE's `UI.RegisterCustomMenu`. It reads its lists from the DLL through F4SE's Scaleform interface
  (`f4se.plugins.NearestQuestTracker`) and sends actions back to the script with `SendExternalEvent`.

### Your own quest starts and categories

Copy `QuestStarts-user.example.json` / `QuestCategories-user.example.json` (in
`F4SE\Plugins\NearestQuestTracker`) to `QuestStarts-user.json` / `QuestCategories-user.json` and add quests from
other mods. The format is explained inside the example files.

## Requirements

- Fallout 4 **1.10.163** (pre-Next-Gen). Next-Gen and Anniversary Edition are not supported yet: the DLL
  relies on engine structure layouts checked on 1.10.163 only and does nothing on other versions. An AE version is
  planned (`PLAN-AE.md`), no date promised.
- [F4SE](https://f4se.silverlock.org/) 0.6.23.
- [Address Library for F4SE Plugins](https://www.nexusmods.com/fallout4/mods/47327).
- [Mod Configuration Menu](https://www.nexusmods.com/fallout4/mods/21497) - hotkeys and settings.

The plugin `NearestQuestTracker.esp` is ESL-flagged; its only master is `Fallout4.esm`. It changes no vanilla
records.

## Building

Clone with the submodule (`git clone --recursive`). Needed: Visual Studio 2026 (18) with its CMake and vcpkg,
Python 3 with Pillow, the Creation Kit's Papyrus compiler in the game folder, JPEXS FFDec + Java (for the window
SWF; `FFDEC_JAR`, default `..\_tools\ffdec_26.3.0\ffdec-cli.jar`), the F4SE 0.6.23 Papyrus sources (`F4SE_SCRIPTS`,
default `..\_tools\f4se_0_06_23\Scripts\Source`).

```
python tools/build.py --release     # DLL, NearestQuestTracker.esp, MCM files, window SWF, Papyrus scripts
python tools/deploy.py --dll release
python tools/package.py 2.0.0       # release\NearestQuestTracker-2.0.0.zip
```

`build.py` and `deploy.py` take the game path from `FO4_PATH` (default `D:\Games\Fallout 4`).

## License

The Unlicense - public domain. Made with Claude Code.
