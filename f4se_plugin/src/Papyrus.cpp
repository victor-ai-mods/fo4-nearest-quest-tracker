#include "Papyrus.h"

#include "EngineTypes.h"
#include "QuestDistance.h"
#include "QuestStarts.h"
#include "QuestText.h"

namespace NQT::Papyrus
{
	namespace
	{
		constexpr auto kScript = "NQT_Native"sv;

		// GetQuestDistance results that are not distances.
		constexpr float kNoTarget = -1.0f;
		constexpr float kOtherSpace = -2.0f;

		std::vector<RE::TESQuest*> GetTrackedQuestList(std::monostate)
		{
			return GetDisplayedQuests();
		}

		// Sources for FindNearestTarget.
		constexpr std::int32_t kSourceQuests = 1;  // targets of Pip-Boy quests
		constexpr std::int32_t kSourceStarts = 2;  // start points of quests not started yet
		constexpr std::int32_t kSourceNoMarker = 4;  // Pip-Boy quests without a marker (with kSourceQuests)

		// Third element of FindNearestTarget's result.
		constexpr std::int32_t kFlagNoMarker = 1;

		// Returns FormIDs, not forms: a single TESQuest* returned by CommonLibF4 reaches the script
		// as None (checked in game 2026-09-30). CommonLibF4 types the variable with the quest's bound
		// script (e.g. dlc04:dlc04koquestscript), the engine's own CreateVarFromValue<TESQuest*> types
		// it "Quest". Papyrus turns the FormIDs into forms with Game.GetForm.
		// Result: [quest, marker ref, flags]; the marker ref is 0 for a Pip-Boy quest; flags: kFlagNoMarker;
		// [0, 0, 0] if none.
		std::vector<std::int32_t> FindNearestTarget(std::monostate, RE::BGSListForm* a_excluded, std::int32_t a_sources)
		{
			std::unordered_set<const RE::TESForm*> excluded;
			if (a_excluded) {
				a_excluded->ForEachForm([&](RE::TESForm* a_form) {
					excluded.insert(a_form);
					return RE::BSContainer::ForEachResult::kContinue;
				});
			}

			logger::debug("FindNearestTarget (sources {}, {} excluded):"sv, a_sources, excluded.size());
			std::vector<Candidate> candidates;
			if (a_sources & kSourceQuests) {
				candidates = CollectQuestCandidates();
				if (a_sources & kSourceNoMarker) {
					std::ranges::move(CollectNoMarkerCandidates(candidates), std::back_inserter(candidates));
				}
			}
			if (a_sources & kSourceStarts) {
				std::ranges::move(CollectStartCandidates(), std::back_inserter(candidates));
			}
			const auto nearest = FindNearest(candidates, [&](const RE::TESQuest& a_quest) {
				return excluded.contains(&a_quest);
			});
			if (!nearest) {
				logger::debug("  -> none"sv);
				return { 0, 0, 0 };
			}
			logger::debug("  -> {}{}{}"sv, DescribeQuest(*nearest->quest),
				nearest->start ? std::format(", start marker on {:08X}"sv, nearest->markerID) : ""s,
				nearest->noMarker ? ", no marker"sv : ""sv);
			return { static_cast<std::int32_t>(nearest->quest->GetFormID()), static_cast<std::int32_t>(nearest->markerID),
				nearest->noMarker ? kFlagNoMarker : 0 };
		}

		// A quest may have several start points: the nearest available one (e.g. the next bottle once one is read).
		std::int32_t GetStartMarkerID(std::monostate, RE::TESQuest* a_quest)
		{
			const auto start = a_quest ? FindAvailableStart(*a_quest) : nullptr;
			return start ? static_cast<std::int32_t>(NQT::GetStartMarkerID(*start)) : 0;
		}

		// The start point the marker of a_quest is about: the nearest available one, else the first.
		const QuestStart* ShownStart(RE::TESQuest* a_quest)
		{
			if (!a_quest) {
				return nullptr;
			}
			const auto start = FindAvailableStart(*a_quest);
			return start ? start : FindQuestStart(*a_quest);
		}

		bool HasQuestStart(std::monostate, RE::TESQuest* a_quest)
		{
			return a_quest && FindQuestStart(*a_quest);
		}

		std::string GetStartName(std::monostate, RE::TESQuest* a_quest)
		{
			return a_quest ? NQT::GetStartName(*a_quest) : std::string{};
		}

		bool IsRadioStart(std::monostate, RE::TESQuest* a_quest)
		{
			const auto start = ShownStart(a_quest);
			return start && start->radioRadius > 0.0f;
		}

		std::string GetStartRadioName(std::monostate, RE::TESQuest* a_quest)
		{
			const auto start = ShownStart(a_quest);
			return start ? NQT::GetStartRadioName(*start) : std::string{};
		}

		// Replaces the text of an objective of a_quest (the start marker quest). Not saved: the
		// script sets it again after a load. Main thread: the Pip-Boy and HUD read this string.
		bool SetObjectiveText(std::monostate, RE::TESQuest* a_quest, std::int32_t a_index, std::string_view a_text)
		{
			if (!a_quest) {
				return false;
			}
			for (const auto objective : a_quest->objectives) {
				const auto own = const_cast<Engine::QuestObjective*>(Engine::AsObjective(objective));
				if (own && own->index == a_index) {
					own->displayText = a_text;
					return true;
				}
			}
			logger::warn("SetObjectiveText: {} has no objective {}"sv, DescribeQuest(*a_quest), a_index);
			return false;
		}

		void Log(std::monostate, std::string_view a_text)
		{
			logger::info("script: {}"sv, a_text);
		}

		float GetQuestDistance(std::monostate, RE::TESQuest* a_quest)
		{
			if (!a_quest) {
				return kNoTarget;
			}
			for (const auto& candidate : CollectQuestCandidates()) {
				if (candidate.quest == a_quest) {
					return candidate.distance.kind == DistanceKind::kOtherSpace ? kOtherSpace : candidate.distance.value;
				}
			}
			return kNoTarget;
		}

		std::string GetQuestDisplayName(std::monostate, RE::TESQuest* a_quest)
		{
			return a_quest ? NQT::GetQuestDisplayName(*a_quest) : std::string{};
		}

		// Papyrus strings are UTF-8 here (Russian names from ParseString): count code points, not bytes.
		std::string Shorten(std::monostate, std::string_view a_text, std::int32_t a_maxChars)
		{
			if (a_maxChars < 2) {
				return std::string{ a_text };
			}
			std::vector<std::size_t> starts;  // byte offset of each code point
			for (std::size_t i = 0; i < a_text.size(); ++i) {
				if ((static_cast<unsigned char>(a_text[i]) & 0xC0) != 0x80) {
					starts.push_back(i);
				}
			}
			if (starts.size() <= static_cast<std::size_t>(a_maxChars)) {
				return std::string{ a_text };
			}
			auto cut = std::string{ a_text.substr(0, starts[a_maxChars - 1]) };
			while (!cut.empty() && cut.back() == ' ') {
				cut.pop_back();
			}
			return cut + "\xE2\x80\xA6";  // "…" in UTF-8
		}

		std::string GetLanguage(std::monostate)
		{
			return GetGameLanguage();
		}

		std::int32_t GetObjectiveSignature(std::monostate)
		{
			return NQT::GetObjectiveSignature();
		}

		void DumpQuestTargets(std::monostate)
		{
			NQT::DumpQuestTargets();
			NQT::DumpQuestStarts();
		}
	}

	bool Register(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm) {
			return false;
		}
		a_vm->BindNativeMethod(kScript, "GetTrackedQuestList"sv, GetTrackedQuestList, true);
		a_vm->BindNativeMethod(kScript, "FindNearestTarget"sv, FindNearestTarget, true);
		a_vm->BindNativeMethod(kScript, "GetStartMarkerID"sv, GetStartMarkerID, true);
		a_vm->BindNativeMethod(kScript, "HasQuestStart"sv, HasQuestStart, true);
		a_vm->BindNativeMethod(kScript, "GetStartName"sv, GetStartName, true);
		a_vm->BindNativeMethod(kScript, "IsRadioStart"sv, IsRadioStart, true);
		a_vm->BindNativeMethod(kScript, "GetStartRadioName"sv, GetStartRadioName, false);  // main thread: alias display name
		a_vm->BindNativeMethod(kScript, "SetObjectiveText"sv, SetObjectiveText, false);  // main thread: Pip-Boy/HUD read it
		a_vm->BindNativeMethod(kScript, "Log"sv, Log, true);
		a_vm->BindNativeMethod(kScript, "GetQuestDistance"sv, GetQuestDistance, true);
		a_vm->BindNativeMethod(kScript, "GetQuestDisplayName"sv, GetQuestDisplayName, false);  // main thread: ParseString reads aliases
		a_vm->BindNativeMethod(kScript, "Shorten"sv, Shorten, true);
		a_vm->BindNativeMethod(kScript, "GetLanguage"sv, GetLanguage, true);
		a_vm->BindNativeMethod(kScript, "GetObjectiveSignature"sv, GetObjectiveSignature, true);
		a_vm->BindNativeMethod(kScript, "DumpQuestTargets"sv, DumpQuestTargets, true);
		logger::info("registered {} native functions"sv, kScript);
		return true;
	}
}
