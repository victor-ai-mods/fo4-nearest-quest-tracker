#pragma once

namespace NQT::Window
{
	// Gives the mod's window (menu/NQTMenu.as) its data: F4SE puts an object for this plugin into every SWF it
	// loads, root.f4se.plugins.NearestQuestTracker, and the functions below go into it (F4SE Scaleform interface).
	// The window calls them when it opens and after each action, so distances are measured then, not every frame.
	//   GetLanguage(): "ru", "en", ... (the game's sLanguage).
	//   GetQuests(): rows of the "Quests" tab, in the order the track hotkey picks them (see QuestDistance.h
	//     FindNearest), then the quests it cannot pick: Pip-Boy quests, start points available now, then every
	//     quest that QuestCategories.json or QuestStarts.json know and that is not started or completed.
	//   GetLocations(): rows of the "Locations" tab: map locations not discovered yet (all of them, `seen` = the
	//     hotkey would pick it without "All locations") and interiors never entered, in the hotkey's order.
	//   GetCategories(): the quest tree [{key, name}], parent before children, "other" last.
	// Row members (FormIDs are signed ints: they go back to Papyrus as Int through f4se.SendExternalEvent):
	//   kind: the Kind* of NQT_MainScript to act on the row with (1 Pip-Boy quest, 2 Pip-Boy quest without a
	//     marker, 3 start point, 4 location, 5 interior; 0 none); quest, ref: FormIDs (ref: where the start or
	//     location marker goes); name; world: name of the root world; worldRank: 0 the player's world, 1 the
	//     Commonwealth, 2 Far Harbor, 3 Nuka-World, 4 other, 5 unknown; dist: game units, -1 no distance
	//     (no marker), -2 in another worldspace with no way known; tracked: marked now (a Pip-Boy quest is active,
	//     the mod's start or location marker is on it); excluded: on the mod's exclusion list.
	//   Quest rows also: state (1 in the Pip-Boy, 2 can be started now, 3 not started, 4 completed),
	//     category (key of QuestCategories.h).
	//   Location rows also: type (icon type name, "" if unknown), onMap (named on the Pip-Boy map), seen.
	void Register();
}
