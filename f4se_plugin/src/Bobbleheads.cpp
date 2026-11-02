#include "Bobbleheads.h"

#include "QuestDistance.h"
#include "QuestStarts.h"

#include <nlohmann/json.hpp>

namespace NQT
{
	namespace
	{
		constexpr auto kFile = "Data/F4SE/Plugins/NearestQuestTracker/Bobbleheads.json"sv;

		std::vector<Bobblehead> bobbleheads;

		template <class T>
		T* FormOf(const nlohmann::json& a_entry, const char* a_key)
		{
			const auto key = a_entry.value(a_key, ""s);
			const auto id = key.empty() ? 0u : ResolveID(key);
			return id ? RE::TESForm::GetFormByID<T>(id) : nullptr;
		}
	}

	void LoadBobbleheads()
	{
		bobbleheads.clear();
		std::ifstream file{ std::filesystem::path{ kFile } };
		if (!file) {
			logger::warn("{}: not found, no bobbleheads"sv, kFile);
			return;
		}
		const auto doc = nlohmann::json::parse(file, nullptr, false, true);
		const auto list = doc.is_object() ? doc.find("bobbleheads") : doc.end();
		if (doc.is_discarded() || list == doc.end() || !list->is_array()) {
			logger::warn("{}: not valid; no bobbleheads"sv, kFile);
			return;
		}
		std::size_t skipped = 0;
		for (const auto& entry : *list) {
			if (!entry.is_object()) {
				continue;
			}
			Bobblehead bobblehead;
			bobblehead.ref = FormOf<RE::TESObjectREFR>(entry, "ref");
			bobblehead.item = FormOf<RE::TESBoundObject>(entry, "item");
			bobblehead.perk = FormOf<RE::BGSPerk>(entry, "perk");
			bobblehead.quest = FormOf<RE::TESQuest>(entry, "quest");
			bobblehead.stage = static_cast<std::uint16_t>(entry.value("stage", 0));
			if (!bobblehead.ref || !bobblehead.item || (!bobblehead.perk && !bobblehead.quest)) {
				++skipped;
				continue;
			}
			bobbleheads.push_back(bobblehead);
		}
		logger::info("Bobbleheads.json: {} bobbleheads ({} skipped: plugin not loaded or wrong forms)"sv, bobbleheads.size(), skipped);
	}

	const std::vector<Bobblehead>& GetBobbleheads()
	{
		return bobbleheads;
	}

	const Bobblehead* FindBobblehead(const RE::TESObjectREFR& a_ref)
	{
		const auto it = std::ranges::find(bobbleheads, &a_ref, &Bobblehead::ref);
		return it != bobbleheads.end() ? &*it : nullptr;
	}

	bool IsBobbleheadFound(const Bobblehead& a_bobblehead)
	{
		if (a_bobblehead.perk) {
			// Actor::GetPerkRank: OG id 1368313 (rva DA6600), NG/AE 2230125 (NG C0C3F0, AE C91EA0) - the same
			// four instructions (AIProcess at +0x300 -> AIProcess::GetPerkRank), checked 2026-11-02.
			const auto player = RE::PlayerCharacter::GetSingleton();
			return player && player->GetPerkRank(a_bobblehead.perk) > 0;
		}
		return a_bobblehead.quest && IsQuestStageDone(*a_bobblehead.quest, a_bobblehead.stage);
	}

	std::string GetBobbleheadName(const RE::TESObjectREFR& a_ref)
	{
		const auto bobblehead = FindBobblehead(a_ref);
		return bobblehead ? std::string{ RE::TESFullName::GetFullName(*bobblehead->item) } : std::string{};
	}

	std::vector<Candidate> CollectBobbleheadCandidates()
	{
		std::vector<Candidate> result;
		const auto             player = RE::PlayerCharacter::GetSingleton();
		if (!player || bobbleheads.empty()) {
			return result;
		}
		const auto playerRoot = PlayerRoot(*player);
		const auto entries = CollectWorldEntries();
		for (const auto& bobblehead : bobbleheads) {
			if (IsBobbleheadFound(bobblehead) || bobblehead.ref->IsDisabled() || bobblehead.ref->IsDeleted()) {
				continue;
			}
			const auto root = RootOf(*bobblehead.ref);
			if (!root) {
				continue;  // in no cell (in a container)
			}
			Candidate candidate;
			candidate.location = bobblehead.ref;
			candidate.markerID = bobblehead.ref->GetFormID();
			candidate.world = root->world;
			candidate.distance = MeasureBetween(playerRoot, *root, entries);
			candidate.bobblehead = true;
			result.push_back(candidate);
		}
		return result;
	}
}
