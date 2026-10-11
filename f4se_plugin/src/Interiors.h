#pragma once

namespace NQT
{
	struct Candidate;

	// How much of an interior the Pip-Boy local map has uncovered ("fog of war"). The engine keeps it in the
	// cell's ExtraSeenData (extra type 5, SeenData* at +0x18): for an interior a chain of IntSeenData sections
	// (TESObjectCELL::GetIntSeenSection, OG rva 3C19D0), each a 4096x4096 square of the cell with 16x16 bits
	// (256x256 units each). PlayerCharacter's update calls TESObjectCELL::UpdateSeenDataForPlayerLoc (OG rva
	// 3C10F0) for the player's cell every frame; the cell saves it under change flag 0x80000000 (kCellSeenData),
	// and a loaded save has it for every interior at once (checked in game 2026-10-04).
	struct InteriorSeen
	{
		std::uint32_t sections{ 0 };  // 0: the player has never been inside
		std::uint32_t bits{ 0 };      // uncovered 256x256 squares; any bit = "visited"
	};

	[[nodiscard]] InteriorSeen GetInteriorSeen(const RE::TESObjectCELL& a_cell);

	// The interior a door teleports to (ExtraTeleport -> linked door -> its cell); null if a_door is not a door
	// into an interior.
	[[nodiscard]] const RE::TESObjectCELL* GetDoorInterior(const RE::TESObjectREFR& a_door);

	// The player has been inside the interior a_door leads to (true if a_door leads into no interior).
	[[nodiscard]] bool IsDoorInteriorVisited(const RE::TESObjectREFR& a_door);

	// The interior's name as the door prompt shows it ("Enter: <name>"); "" if none.
	[[nodiscard]] std::string GetDoorInteriorName(const RE::TESObjectREFR& a_door);

	// Interiors the player has never been inside, one candidate per interior: its nearest door from a worldspace
	// (doors into interiors are persistent refs in a world's persistent cell). Skipped doors: disabled,
	// "inaccessible", locked with "Requires Key" / "Inaccessible" (user's choice 2026-10-04: lockpickable doors stay),
	// into the test cell CTEST or an abandoned "z..." cell. Automatic (walk-in) doors stay: caves, sinkholes, metro
	// stations use them. a_skipOwnedLocked: also locked doors that are a crime to pick (red "Pick" prompt).
	// Distance: MeasureBetween to the door.
	[[nodiscard]] std::vector<Candidate> CollectInteriorCandidates(bool a_skipOwnedLocked);

	// Writes the fog of war of all interiors and the interiors that doors of the worldspaces lead into
	// (nearest first, with the reason a door is skipped, "locked, crime to pick" included) to the log.
	void DumpInteriors();
}
