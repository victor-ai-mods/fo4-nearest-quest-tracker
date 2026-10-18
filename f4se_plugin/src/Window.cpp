#include "Window.h"

#include "Interiors.h"
#include "MapMarkers.h"
#include "QuestCategories.h"
#include "QuestDistance.h"
#include "QuestStarts.h"
#include "QuestText.h"

namespace NQT::Window
{
	namespace
	{
		using RE::Scaleform::GFx::FunctionHandler;
		using RE::Scaleform::GFx::Movie;
		using GFxValue = RE::Scaleform::GFx::Value;

		// NearestQuestTracker.esp (tools/gen_esp.py).
		constexpr auto          kPlugin = "NearestQuestTracker.esp"sv;
		constexpr std::uint32_t kExcludedQuestsID = 0x803;     // NQT_ExcludedList
		constexpr std::uint32_t kStartQuestID = 0x805;         // NQT_StartMarkerQuest
		constexpr std::uint32_t kLocationQuestID = 0x809;      // NQT_LocationMarkerQuest
		constexpr std::uint32_t kExcludedLocationsID = 0x80B;  // NQT_ExcludedLocationList
		constexpr std::uint32_t kSkipOwnedLockedID = 0x80F;    // NQT_SkipOwnedLocked
		constexpr std::uint32_t kMarkerAlias = 0;              // NQT_MainScript StartAlias, LocationRefAlias

		// NQT_MainScript Kind*.
		enum class Kind : std::int32_t
		{
			kNone = 0,
			kQuest = 1,
			kNoMarker = 2,
			kStart = 3,
			kLocation = 4,
			kInterior = 5,
		};

		enum class QuestState : std::int32_t
		{
			kPipboy = 1,
			kCanStart = 2,
			kNotStarted = 3,
			kCompleted = 4,
			kNotOffered = 5,  // IsStartNotOffered: the giver is near but does not offer it now
			kClosed = 6,      // FindStartClosure: its faction is hostile (kicked the player out)
			kGiverHostile = 7,  // IsStartGiverHostile: the giver would fight the player
			kRunningNotTaken = 8,  // the game runs it (e.g. a radiant waiting for its giver), not in the Pip-Boy
		};

		// Row distances that are not distances.
		constexpr double kNoDistance = -1.0;
		constexpr double kOtherSpace = -2.0;

		template <class T>
		T* Lookup(std::uint32_t a_id)
		{
			const auto dataHandler = RE::TESDataHandler::GetSingleton();
			return dataHandler ? dataHandler->LookupForm<T>(a_id, kPlugin) : nullptr;
		}

		std::unordered_set<const RE::TESForm*> FormsOf(std::uint32_t a_listID)
		{
			std::unordered_set<const RE::TESForm*> result;
			if (const auto list = Lookup<RE::BGSListForm>(a_listID)) {
				list->ForEachForm([&](RE::TESForm* a_form) {
					result.insert(a_form);
					return RE::BSContainer::ForEachResult::kContinue;
				});
			}
			return result;
		}

		// TESQuest::GetActive (OG rva 5D6AA0): data flags bit 11, what Quest.IsActive / SetActive use.
		bool IsActive(const RE::TESQuest& a_quest) noexcept
		{
			return (a_quest.data.flags >> 11) & 1;
		}

		// FormID of the ref the alias of a marker quest holds while the quest is active (the mod's marker is
		// shown: NQT_MainScript ClearStart / ClearLocation deactivate the quest and clear the alias); 0 if none.
		std::uint32_t ShownMarkerID(std::uint32_t a_questID)
		{
			const auto quest = Lookup<RE::TESQuest>(a_questID);
			if (!quest || !IsActive(*quest)) {
				return 0;
			}
			RE::ObjectRefHandle handle;
			{
				RE::BSAutoReadLock lock{ quest->aliasAccessLock };
#ifdef NQT_AE
				handle = quest->GetAliasedRef(kMarkerAlias);
#else
				quest->GetAliasedRef(&handle, kMarkerAlias);
#endif
			}
			const auto ref = handle.get();
			return ref ? ref->GetFormID() : 0;
		}

		// Pip-Boy quests: the Pip-Boy's name; others: their own name, else the text of the objective their start
		// point waits for (or of the first objective), else the editor ID.
		std::string QuestName(const RE::TESQuest& a_quest, bool a_pipboy)
		{
			auto name = a_pipboy ? GetQuestDisplayName(a_quest) : GetStartName(a_quest);
			if (name.empty() && !a_pipboy) {
				const auto start = FindAvailableStart(a_quest);
				const auto first = start ? start : FindQuestStart(a_quest);
				name = GetObjectiveName(a_quest, first ? first->objective : std::nullopt);
			}
			if (name.empty()) {
				const auto editorID = a_quest.formEditorID.c_str();
				name = editorID ? editorID : std::format("{:08X}"sv, a_quest.GetFormID());
			}
			return name;
		}

		double RowDistance(const Candidate& a_candidate) noexcept
		{
			if (a_candidate.noMarker) {
				return kNoDistance;
			}
			return a_candidate.distance.kind == DistanceKind::kOtherSpace ? kOtherSpace : a_candidate.distance.value;
		}

		// Makes the GFx values of one movie. Strings are made by the movie (managed), so they outlive the
		// std::string they come from.
		class Builder
		{
		public:
			explicit Builder(Movie& a_movie, const RE::TESWorldSpace* a_playerWorld) :
				_movie(a_movie),
				_playerWorld(a_playerWorld)
			{}

			GFxValue Object()
			{
				GFxValue value;
				_movie.CreateObject(&value);
				return value;
			}

			GFxValue Array()
			{
				GFxValue value;
				_movie.CreateArray(&value);
				return value;
			}

			GFxValue String(const std::string& a_text)
			{
				GFxValue value;
				_movie.asMovieRoot->CreateString(&value, a_text.c_str());
				return value;
			}

			GFxValue Row(Kind a_kind, const RE::TESQuest* a_quest, std::uint32_t a_refID, const std::string& a_name,
				const RE::TESWorldSpace* a_world, double a_distance, bool a_tracked, bool a_excluded)
			{
				auto row = Object();
				row.SetMember("kind", GFxValue(std::to_underlying(a_kind)));
				row.SetMember("quest", GFxValue(static_cast<std::int32_t>(a_quest ? a_quest->GetFormID() : 0u)));
				row.SetMember("ref", GFxValue(static_cast<std::int32_t>(a_refID)));
				row.SetMember("name", String(a_name));
				const auto worldName = a_world ? RE::TESFullName::GetFullName(*a_world) : ""sv;
				row.SetMember("world", String(std::string{ worldName }));
				row.SetMember("worldRank", GFxValue(static_cast<std::int32_t>(WorldRank(a_world, _playerWorld))));
				row.SetMember("dist", GFxValue(a_distance));
				row.SetMember("tracked", GFxValue(a_tracked));
				row.SetMember("excluded", GFxValue(a_excluded));
				return row;
			}

		private:
			Movie&                   _movie;
			const RE::TESWorldSpace* _playerWorld;
		};

		using Clock = std::chrono::steady_clock;

		double MillisecondsSince(Clock::time_point a_start)
		{
			return std::chrono::duration<double, std::milli>(Clock::now() - a_start).count();
		}

		GFxValue BuildQuests(Movie& a_movie)
		{
			const auto start = Clock::now();
			const auto excluded = FormsOf(kExcludedQuestsID);
			const auto shownStart = ShownMarkerID(kStartQuestID);
			Builder    builder{ a_movie, GetPlayerRootWorld() };
			auto       rows = builder.Array();

			auto candidates = CollectQuestCandidates();
			std::ranges::move(CollectNoMarkerCandidates(candidates), std::back_inserter(candidates));
			std::erase_if(candidates, [](const Candidate& a_candidate) {
				return a_candidate.quest && IsPointerBlocked(*a_candidate.quest);
			});
			std::ranges::move(CollectStartCandidates(true, true), std::back_inserter(candidates));
			SortByTrackOrder(candidates);

			// A quest with several start points: its nearest one (the first in track order).
			std::unordered_set<const RE::TESQuest*> listed;
			std::size_t                             pickable = 0;
			for (const auto& candidate : candidates) {
				if (!candidate.quest || !listed.insert(candidate.quest).second) {
					continue;
				}
				const auto& quest = *candidate.quest;
				const auto  kind = candidate.start ? Kind::kStart : candidate.noMarker ? Kind::kNoMarker : Kind::kQuest;
				const bool  tracked = candidate.start ? shownStart != 0 && shownStart == candidate.markerID : IsActive(quest);
				auto        row = builder.Row(kind, &quest, candidate.markerID, QuestName(quest, !candidate.start),
					candidate.world, RowDistance(candidate), tracked, excluded.contains(&quest));
				row.SetMember("state", GFxValue(std::to_underlying(candidate.start ? QuestState::kCanStart : QuestState::kPipboy)));
				row.SetMember("category", builder.String(GetQuestCategory(quest)));
				rows.PushBack(row);
				++pickable;
			}

			// The rest of the known quests: not started (or running with nothing to show) first, then completed;
			// by category, then by name.
			struct Rest
			{
				RE::TESQuest* quest;
				bool          completed;
				std::string   category;
				std::string   name;
			};
			std::vector<Rest> rest;
			auto              known = GetCategorizedQuests();
			std::ranges::move(GetQuestsWithStarts(), std::back_inserter(known));
			for (const auto quest : known) {
				if (quest && listed.insert(quest).second) {
					rest.push_back({ quest, IsQuestCompleted(*quest), GetQuestCategory(*quest), QuestName(*quest, false) });
				}
			}
			std::ranges::sort(rest, std::less{}, [](const Rest& a_rest) {
				return std::tie(a_rest.completed, a_rest.category, a_rest.name);
			});
			for (const auto& entry : rest) {
				auto row = builder.Row(Kind::kNone, entry.quest, 0, entry.name, PluginWorld(*entry.quest), kNoDistance,
					false, excluded.contains(entry.quest));
				const auto closure = entry.completed ? nullptr : FindStartClosure(*entry.quest);
				const auto state = entry.completed           ? QuestState::kCompleted :
				                   closure                   ? QuestState::kClosed :
				                   IsStartGiverHostile(*entry.quest) ? QuestState::kGiverHostile :
				                   IsQuestRunning(*entry.quest) ? QuestState::kRunningNotTaken :
				                   IsStartNotOffered(*entry.quest) ? QuestState::kNotOffered :
				                                                     QuestState::kNotStarted;
				row.SetMember("state", GFxValue(std::to_underlying(state)));
				if (state == QuestState::kRunningNotTaken) {
					if (const auto held = FindHeldLocation(*entry.quest)) {
						row.SetMember("heldAt", builder.String(std::string{ RE::TESFullName::GetFullName(*held) }));
					}
				}
				if (closure) {
					row.SetMember("closedRu", builder.String(closure->ru));
					row.SetMember("closedEn", builder.String(closure->en));
				}
				if (state == QuestState::kNotOffered) {
					if (const auto blocker = FindStartBlocker(*entry.quest)) {
						// Pip-Boy name only while it shows an objective (a radiant at its first stage may not): else its own name.
						auto by = GetQuestDisplayName(*blocker->holder);
						if (by.empty()) {
							by = QuestName(*blocker->holder, false);
						}
						row.SetMember("blockedBy", builder.String(by));
						row.SetMember("blockedAt", builder.String(std::string{ RE::TESFullName::GetFullName(*blocker->location) }));
					}
				}
				row.SetMember("category", builder.String(entry.category));
				rows.PushBack(row);
			}
			logger::info("window: GetQuests -> {} rows ({} pickable) in {:.1f} ms"sv, pickable + rest.size(), pickable,
				MillisecondsSince(start));
			return rows;
		}

		GFxValue BuildLocations(Movie& a_movie)
		{
			const auto start = Clock::now();
			const auto language = GetGameLanguage();
			const auto excluded = FormsOf(kExcludedLocationsID);
			std::unordered_set<const RE::TESObjectCELL*> excludedInteriors;
			for (const auto form : excluded) {
				const auto ref = form ? form->As<RE::TESObjectREFR>() : nullptr;
				if (const auto interior = ref ? GetDoorInterior(*ref) : nullptr) {
					excludedInteriors.insert(interior);
				}
			}
			const auto shown = ShownMarkerID(kLocationQuestID);
			const auto skipOwnedLocked = Lookup<RE::TESGlobal>(kSkipOwnedLockedID);
			Builder    builder{ a_movie, GetPlayerRootWorld() };
			auto       rows = builder.Array();

			std::unordered_set<const RE::TESObjectREFR*> seen;
			for (const auto& candidate : CollectLocationCandidates(false)) {
				seen.insert(candidate.location);
			}
			auto candidates = CollectLocationCandidates(true);
			std::ranges::move(CollectInteriorCandidates(skipOwnedLocked && skipOwnedLocked->GetValue() != 0.0f),
				std::back_inserter(candidates));
			SortByTrackOrder(candidates);

			for (const auto& candidate : candidates) {
				if (!candidate.location) {
					continue;
				}
				const auto& ref = *candidate.location;
				const bool  interior = candidate.interior != nullptr;
				const auto  refID = ref.GetFormID();
				auto        row = builder.Row(interior ? Kind::kInterior : Kind::kLocation, nullptr, refID,
					interior ? GetDoorInteriorName(ref) : GetMapMarkerName(ref), candidate.world, RowDistance(candidate),
					shown != 0 && shown == refID, interior ? excludedInteriors.contains(candidate.interior) : excluded.contains(&ref));
				row.SetMember("type", builder.String(interior ? ""s : GetMapMarkerTypeName(ref, language)));
				row.SetMember("onMap", GFxValue(interior || GetMapMarkerState(ref) != MarkerState::kUnknown));
				row.SetMember("seen", GFxValue(interior || seen.contains(&ref)));
				rows.PushBack(row);
			}
			logger::info("window: GetLocations -> {} rows ({} seen) in {:.1f} ms"sv, candidates.size(), seen.size(),
				MillisecondsSince(start));
			return rows;
		}

		GFxValue BuildCategories(Movie& a_movie)
		{
			const auto language = GetGameLanguage();
			Builder    builder{ a_movie, nullptr };
			auto       rows = builder.Array();
			const auto add = [&](const std::string& a_key) {
				auto row = builder.Object();
				row.SetMember("key", builder.String(a_key));
				row.SetMember("name", builder.String(GetQuestCategoryName(a_key, language)));
				rows.PushBack(row);
			};
			for (const auto& category : GetQuestCategories()) {
				add(category.key);
			}
			add(std::string{ kOtherCategory });
			return rows;
		}

		// One handler object per function for the whole game: `new` takes it from the Scaleform heap
		// (SF_HEAP_REDEFINE_NEW) with one reference that is never released, so the movies' references never
		// delete it.
		template <GFxValue (*Build)(Movie&)>
		class Handler final : public FunctionHandler
		{
		public:
			void Call(const Params& a_params) override
			{
				if (a_params.retVal && a_params.movie) {
					*a_params.retVal = Build(*a_params.movie);
				}
			}
		};

		GFxValue BuildLanguage(Movie& a_movie)
		{
			GFxValue value;
			a_movie.asMovieRoot->CreateString(&value, GetGameLanguage().c_str());
			return value;
		}

		template <GFxValue (*Build)(Movie&)>
		void AddFunction(Movie& a_movie, GFxValue& a_object, const char* a_name)
		{
			static const auto handler = new Handler<Build>();
			GFxValue          function;
			a_movie.CreateFunction(&function, handler);
			a_object.SetMember(a_name, function);
		}

		// F4SE calls this for every SWF it loads with the movie's root.f4se.plugins.NearestQuestTracker object.
		bool F4SEAPI OnMovie(Movie* a_movie, GFxValue* a_object)
		{
			if (!a_movie || !a_object || !a_object->IsObject()) {
				return false;
			}
			AddFunction<BuildLanguage>(*a_movie, *a_object, "GetLanguage");
			AddFunction<BuildQuests>(*a_movie, *a_object, "GetQuests");
			AddFunction<BuildLocations>(*a_movie, *a_object, "GetLocations");
			AddFunction<BuildCategories>(*a_movie, *a_object, "GetCategories");
			return true;
		}
	}

	void Register()
	{
		const auto scaleform = F4SE::GetScaleformInterface();
		// The window finds the functions under this name: f4se.plugins.NearestQuestTracker (menu/NQTMenu.as).
		const bool registered = scaleform && scaleform->Register("NearestQuestTracker", OnMovie);
		logger::info("Scaleform functions for the window {}"sv, registered ? "registered"sv : "NOT registered"sv);
	}
}
