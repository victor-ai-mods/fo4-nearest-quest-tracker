#include "Papyrus.h"

#include "EngineTypes.h"
#include "Interiors.h"
#include "MapMarkers.h"
#include "QuestCategories.h"
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
		constexpr std::int32_t kSourceQuests = 1;     // targets of Pip-Boy quests
		constexpr std::int32_t kSourceStarts = 2;     // start points of quests not started yet
		constexpr std::int32_t kSourceNoMarker = 4;   // Pip-Boy quests without a marker (with kSourceQuests)
		constexpr std::int32_t kSourceLocations = 8;  // map markers not discovered yet
		constexpr std::int32_t kSourceInteriors = 16; // interiors never entered
		constexpr std::int32_t kSourceSkipOwnedLocked = 32;  // with kSourceInteriors: skip locked doors that are a crime to pick

		// First element of FindNearestTarget's result.
		enum class TargetKind : std::int32_t
		{
			kNone = 0,
			kQuest = 1,     // Pip-Boy quest with a marker
			kNoMarker = 2,  // Pip-Boy quest without a marker
			kStart = 3,     // start point of a quest not started yet
			kLocation = 4,  // map marker not discovered yet
			kInterior = 5,  // interior never entered; the marker goes on its door
		};

		std::unordered_set<const RE::TESForm*> FormsOf(RE::BGSListForm* a_list)
		{
			std::unordered_set<const RE::TESForm*> result;
			if (a_list) {
				a_list->ForEachForm([&](RE::TESForm* a_form) {
					result.insert(a_form);
					return RE::BSContainer::ForEachResult::kContinue;
				});
			}
			return result;
		}

		// Returns FormIDs, not forms: a single TESQuest* returned by CommonLibF4 reaches the script
		// as None (checked in game 2026-09-30). CommonLibF4 types the variable with the quest's bound
		// script (e.g. dlc04:dlc04koquestscript), the engine's own CreateVarFromValue<TESQuest*> types
		// it "Quest". Papyrus turns the FormIDs into forms with Game.GetForm.
		// Result: [kind, quest, marker ref]; quest is 0 for a location or interior; the marker ref is where the start,
		// location or interior marker goes (0 for a Pip-Boy quest); [0, 0, 0] if none. a_excludedLocations holds map
		// markers and doors: a door excludes its interior, whichever door leads in.
		std::vector<std::int32_t> FindNearestTarget(std::monostate, RE::BGSListForm* a_excludedQuests,
			RE::BGSListForm* a_excludedLocations, std::int32_t a_sources, bool a_allLocations)
		{
			const auto excludedQuests = FormsOf(a_excludedQuests);
			const auto excludedLocations = FormsOf(a_excludedLocations);
			std::unordered_set<const RE::TESObjectCELL*> excludedInteriors;
			for (const auto form : excludedLocations) {
				const auto ref = form ? form->As<RE::TESObjectREFR>() : nullptr;
				if (const auto interior = ref ? GetDoorInterior(*ref) : nullptr) {
					excludedInteriors.insert(interior);
				}
			}
			logger::debug("FindNearestTarget (sources {}, {} quests and {} locations excluded):"sv, a_sources,
				excludedQuests.size(), excludedLocations.size());
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
			if (a_sources & kSourceLocations) {
				std::ranges::move(CollectLocationCandidates(a_allLocations), std::back_inserter(candidates));
			}
			if (a_sources & kSourceInteriors) {
				std::ranges::move(CollectInteriorCandidates((a_sources & kSourceSkipOwnedLocked) != 0), std::back_inserter(candidates));
			}
			const auto nearest = FindNearest(candidates, [&](const Candidate& a_candidate) {
				return a_candidate.interior ? excludedInteriors.contains(a_candidate.interior) :
				       a_candidate.location ? excludedLocations.contains(a_candidate.location) :
				                              excludedQuests.contains(a_candidate.quest);
			});
			if (!nearest) {
				logger::debug("  -> none"sv);
				return { 0, 0, 0 };
			}
			const auto kind = nearest->interior ? TargetKind::kInterior :
			                  nearest->location ? TargetKind::kLocation :
			                  nearest->start    ? TargetKind::kStart :
			                  nearest->noMarker ? TargetKind::kNoMarker :
			                                      TargetKind::kQuest;
			logger::debug("  -> {}{}"sv, DescribeCandidate(*nearest),
				nearest->markerID ? std::format(", marker on {:08X}"sv, nearest->markerID) : ""s);
			return { std::to_underlying(kind), nearest->quest ? static_cast<std::int32_t>(nearest->quest->GetFormID()) : 0,
				static_cast<std::int32_t>(nearest->markerID) };
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

		bool IsStartNotOffered(std::monostate, RE::TESQuest* a_quest)
		{
			return a_quest && NQT::IsStartNotOffered(*a_quest);
		}

		// FormID of the running quest holding a location a_quest needs (FindStartBlocker); 0 if none.
		std::int32_t GetStartBlocker(std::monostate, RE::TESQuest* a_quest)
		{
			const auto blocker = a_quest ? FindStartBlocker(*a_quest) : std::nullopt;
			return blocker ? static_cast<std::int32_t>(blocker->holder->GetFormID()) : 0;
		}

		// Name of that location; "" if none.
		std::string GetStartBlockedPlace(std::monostate, RE::TESQuest* a_quest)
		{
			const auto blocker = a_quest ? FindStartBlocker(*a_quest) : std::nullopt;
			return blocker ? std::string{ RE::TESFullName::GetFullName(*blocker->location) } : std::string{};
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

		// PlayerCharacterQuestEvent::Notify (OG id 395737). Type 1 = "objective updated": PipboyQuestData::
		// ProcessEvent then runs UpdateObjective, which reads GetDisplayText again.
		void NotifyObjectiveUpdated(RE::BGSInstancedQuestObjective* a_instance)
		{
			using func_t = void(std::int32_t, RE::BGSInstancedQuestObjective*);
			static REL::Relocation<func_t*> func{ REL::ID(395737) };
			constexpr std::int32_t kObjectiveUpdated = 1;
			func(kObjectiveUpdated, a_instance);
		}

		// Replaces the text of an objective of a_quest (a marker quest). Not saved: the script sets it again
		// after a load. Main thread: the Pip-Boy and HUD read this string.
		// The Pip-Boy keeps its own copy of the text, made when the objective was shown (or the save loaded -
		// before the script runs, so with the ESP text "Location"); like PlayerCharacter::
		// UpdateInstancedQuestObjective (rva EA4240, disassembled 2026-10-07) we send "objective updated" for the
		// shown instances, under questTargetLock as the engine does.
		bool SetObjectiveText(std::monostate, RE::TESQuest* a_quest, std::int32_t a_index, std::string_view a_text)
		{
			if (!a_quest) {
				return false;
			}
			for (const auto objective : a_quest->objectives) {
				const auto own = const_cast<Engine::QuestObjective*>(Engine::AsObjective(objective));
				if (own && own->index == a_index) {
					own->displayText = a_text;
					if (const auto player = RE::PlayerCharacter::GetSingleton()) {
						RE::BSAutoLock lock{ player->questTargetLock };
						for (auto& instance : player->objectives) {
							if (Engine::AsObjective(instance.objective) == own) {
								NotifyObjectiveUpdated(&instance);
							}
						}
					}
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

		std::vector<std::string> GetObjectiveTexts(std::monostate, RE::TESQuest* a_quest)
		{
			return a_quest ? NQT::GetObjectiveTexts(*a_quest) : std::vector<std::string>{};
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
			NQT::DumpQuestCategories();
		}

		void DumpMapMarkers(std::monostate)
		{
			NQT::DumpMapMarkers();
		}

		void DumpInteriors(std::monostate)
		{
			NQT::DumpInteriors();
		}

		std::string GetMapMarkerName(std::monostate, RE::TESObjectREFR* a_marker)
		{
			return a_marker ? NQT::GetMapMarkerName(*a_marker) : std::string{};
		}

		std::string GetMapMarkerTypeName(std::monostate, RE::TESObjectREFR* a_marker)
		{
			return a_marker ? NQT::GetMapMarkerTypeName(*a_marker, GetGameLanguage()) : std::string{};
		}

		std::string GetInteriorName(std::monostate, RE::TESObjectREFR* a_door)
		{
			return a_door ? GetDoorInteriorName(*a_door) : std::string{};
		}

		bool IsInteriorVisited(std::monostate, RE::TESObjectREFR* a_door)
		{
			return !a_door || IsDoorInteriorVisited(*a_door);
		}

		std::string GetQuestCategory(std::monostate, RE::TESQuest* a_quest)
		{
			return a_quest ? NQT::GetQuestCategory(*a_quest) : std::string{};
		}

		std::string GetQuestCategoryName(std::monostate, std::string_view a_key)
		{
			return NQT::GetQuestCategoryName(a_key, GetGameLanguage());
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
		a_vm->BindNativeMethod(kScript, "IsStartNotOffered"sv, IsStartNotOffered, true);
		a_vm->BindNativeMethod(kScript, "GetStartBlocker"sv, GetStartBlocker, true);
		a_vm->BindNativeMethod(kScript, "GetStartBlockedPlace"sv, GetStartBlockedPlace, true);
		a_vm->BindNativeMethod(kScript, "HasQuestStart"sv, HasQuestStart, true);
		a_vm->BindNativeMethod(kScript, "GetStartName"sv, GetStartName, true);
		a_vm->BindNativeMethod(kScript, "IsRadioStart"sv, IsRadioStart, true);
		a_vm->BindNativeMethod(kScript, "GetStartRadioName"sv, GetStartRadioName, false);  // main thread: alias display name
		a_vm->BindNativeMethod(kScript, "SetObjectiveText"sv, SetObjectiveText, false);  // main thread: Pip-Boy/HUD read it
		a_vm->BindNativeMethod(kScript, "Log"sv, Log, true);
		a_vm->BindNativeMethod(kScript, "GetQuestDistance"sv, GetQuestDistance, true);
		a_vm->BindNativeMethod(kScript, "GetQuestDisplayName"sv, GetQuestDisplayName, false);  // main thread: ParseString reads aliases
		a_vm->BindNativeMethod(kScript, "GetObjectiveTexts"sv, GetObjectiveTexts, false);  // main thread: ParseString reads aliases
		a_vm->BindNativeMethod(kScript, "Shorten"sv, Shorten, true);
		a_vm->BindNativeMethod(kScript, "GetLanguage"sv, GetLanguage, true);
		a_vm->BindNativeMethod(kScript, "GetObjectiveSignature"sv, GetObjectiveSignature, true);
		a_vm->BindNativeMethod(kScript, "DumpQuestTargets"sv, DumpQuestTargets, true);
		a_vm->BindNativeMethod(kScript, "DumpMapMarkers"sv, DumpMapMarkers, true);
		a_vm->BindNativeMethod(kScript, "DumpInteriors"sv, DumpInteriors, false);  // main thread: the player's cell updates its seen data
		a_vm->BindNativeMethod(kScript, "GetMapMarkerName"sv, GetMapMarkerName, true);
		a_vm->BindNativeMethod(kScript, "GetMapMarkerTypeName"sv, GetMapMarkerTypeName, true);
		a_vm->BindNativeMethod(kScript, "GetInteriorName"sv, GetInteriorName, true);
		a_vm->BindNativeMethod(kScript, "IsInteriorVisited"sv, IsInteriorVisited, true);
		a_vm->BindNativeMethod(kScript, "GetQuestCategory"sv, GetQuestCategory, true);
		a_vm->BindNativeMethod(kScript, "GetQuestCategoryName"sv, GetQuestCategoryName, true);
		logger::info("registered {} native functions"sv, kScript);
		return true;
	}
}
