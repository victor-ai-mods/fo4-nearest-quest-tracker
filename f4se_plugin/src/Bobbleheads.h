#pragma once

namespace NQT
{
	struct Candidate;

	// A bobblehead in the world (Data\F4SE\Plugins\NearestQuestTracker\Bobbleheads.json, tools/gen_bobbleheads.py):
	// a persistent REFR of a MISC item with BobbleheadSkillScript (bonus: AddPerk(perk)) or BobbleheadSPECIALscrip
	// (bonus: PerksQuest.SetStage(stage)). Found = the bonus is there, what both scripts check before giving it.
	struct Bobblehead
	{
		RE::TESObjectREFR*  ref{ nullptr };
		RE::TESBoundObject* item{ nullptr };
		RE::BGSPerk*        perk{ nullptr };
		RE::TESQuest*       quest{ nullptr };
		std::uint16_t       stage{ 0 };
	};

	// Reads Bobbleheads.json; entries whose plugin is not loaded are skipped. Call once game data is loaded.
	void LoadBobbleheads();

	[[nodiscard]] const std::vector<Bobblehead>& GetBobbleheads();

	// The bobblehead whose ref is a_ref; nullptr if a_ref is not one.
	[[nodiscard]] const Bobblehead* FindBobblehead(const RE::TESObjectREFR& a_ref);

	// The player has its bonus (picked it up).
	[[nodiscard]] bool IsBobbleheadFound(const Bobblehead& a_bobblehead);

	// The item's name ("Пупс \"Сила\"" / "Strength Bobblehead"); "" if a_ref is no bobblehead.
	[[nodiscard]] std::string GetBobbleheadName(const RE::TESObjectREFR& a_ref);

	// Bobbleheads not found yet whose ref is enabled, one candidate each (bobblehead = true, location = the ref):
	// distance by MeasureBetween to where the ref is on the world map (RootOf: an interior one by its door out).
	[[nodiscard]] std::vector<Candidate> CollectBobbleheadCandidates();
}
