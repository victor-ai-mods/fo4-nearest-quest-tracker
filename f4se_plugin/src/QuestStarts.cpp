#include "QuestStarts.h"

#include "EngineTypes.h"

#include <nlohmann/json.hpp>

namespace NQT
{
	// "Plugin.esp|ID" -> loaded FormID; the ID as xEdit shows it (with or without the load
	// order byte, FExxxxxx for ESL). 0 if the plugin is not loaded or the text is malformed.
	// Works for refs that are not in memory: a temporary ref keeps its file FormID.
	std::uint32_t ResolveID(std::string_view a_key)
	{
		const auto bar = a_key.find('|');
		if (bar == std::string_view::npos) {
			return 0;
		}
		const auto    plugin = a_key.substr(0, bar);
		const auto    hex = a_key.substr(bar + 1);
		std::uint32_t raw = 0;
		if (hex.empty() || std::from_chars(hex.data(), hex.data() + hex.size(), raw, 16).ec != std::errc{}) {
			return 0;
		}
		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			return 0;
		}
		const auto [file, full] = dataHandler->LookupLoadedFile(plugin);
		if (!file) {
			return 0;
		}
		return full ? (std::uint32_t{ file->compileIndex } << 24) | (raw & 0xFFFFFF) :
		              0xFE000000 | (std::uint32_t{ file->smallFileCompileIndex } << 12) | (raw & 0xFFF);
	}

	namespace
	{
		constexpr auto kDataDir = "Data/F4SE/Plugins/NearestQuestTracker"sv;

		std::vector<QuestStart> starts;

		RE::TESQuest* ResolveQuest(const nlohmann::json& a_value)
		{
			if (!a_value.is_string()) {
				return nullptr;
			}
			const auto id = ResolveID(a_value.get_ref<const std::string&>());
			return id ? RE::TESForm::GetFormByID<RE::TESQuest>(id) : nullptr;
		}

		std::uint32_t ResolveRefID(const nlohmann::json& a_entry, const char* a_field)
		{
			const auto it = a_entry.find(a_field);
			return it != a_entry.end() && it->is_string() ? ResolveID(it->get_ref<const std::string&>()) : 0;
		}

		// One entry of "starts"; nullopt (with the reason in the log) if it cannot be used.
		std::optional<QuestStart> ParseEntry(const nlohmann::json& a_entry, std::string_view a_file)
		{
			if (!a_entry.is_object()) {
				return std::nullopt;
			}
			QuestStart start;
			start.name = a_entry.value("name", a_entry.value("quest", "?"s));
			start.quest = ResolveQuest(a_entry.value("quest", nlohmann::json{}));
			start.refID = ResolveRefID(a_entry, "ref");
			if (!start.quest || !start.refID) {
				logger::debug("{}: skipped {} (quest or ref plugin not loaded)"sv, a_file, start.name);
				return std::nullopt;
			}
			if (const auto after = a_entry.find("after"); after != a_entry.end() && after->is_array()) {
				// An element is a quest, or an array of quests of which any one will do. A quest whose
				// plugin is not loaded drops out of its group; a group left empty skips the entry.
				for (const auto& element : *after) {
					std::vector<RE::TESQuest*> group;
					for (const auto& key : element.is_array() ? element : nlohmann::json::array({ element })) {
						if (const auto quest = ResolveQuest(key)) {
							group.push_back(quest);
						}
					}
					if (group.empty()) {
						logger::debug("{}: skipped {} (quest in \"after\" not loaded)"sv, a_file, start.name);
						return std::nullopt;
					}
					start.after.push_back(std::move(group));
				}
			}
			if (const auto objective = a_entry.find("objective"); objective != a_entry.end() && objective->is_number_integer()) {
				start.objective = objective->get<std::uint16_t>();
			}
			if (const auto stage = a_entry.find("stage"); stage != a_entry.end() && stage->is_number_integer()) {
				start.stage = stage->get<std::uint16_t>();
			}
			const auto worldID = ResolveRefID(a_entry, "world");
			const auto pos = a_entry.find("pos");
			if (worldID && pos != a_entry.end() && pos->is_array() && pos->size() == 3) {
				start.world = RE::TESForm::GetFormByID<RE::TESWorldSpace>(worldID);
				start.pos = { (*pos)[0].get<float>(), (*pos)[1].get<float>(), (*pos)[2].get<float>() };
			}
			start.anchorID = ResolveRefID(a_entry, "anchor");
			start.markerID = ResolveRefID(a_entry, "marker");
			if (const auto radio = a_entry.find("radio"); radio != a_entry.end() && radio->is_number()) {
				start.radioRadius = std::max(radio->get<float>(), 1.0f);
			}
			start.running = a_entry.value("running", false);
			start.radiant = a_entry.value("radiant", false);
			if (const auto infos = a_entry.find("info"); infos != a_entry.end() && infos->is_array()) {
				for (const auto& key : *infos) {
					if (const auto id = key.is_string() ? ResolveID(key.get_ref<const std::string&>()) : 0) {
						start.infos.push_back(id);
					}
				}
			}
			start.giverBaseID = ResolveRefID(a_entry, "giver");
			if (const auto pointers = a_entry.find("pointers"); pointers != a_entry.end() && pointers->is_array()) {
				for (const auto& key : *pointers) {
					if (const auto quest = ResolveQuest(key)) {
						start.pointers.push_back(quest);
					}
				}
			}
			if (start.radiant && start.infos.empty()) {
				logger::debug("{}: skipped {} (radiant without offer lines loaded)"sv, a_file, start.name);
				return std::nullopt;
			}
			if (const auto closed = a_entry.find("closed"); closed != a_entry.end() && closed->is_array()) {
				// A condition whose plugin is not loaded is dropped (the entry stays).
				for (const auto& item : *closed) {
					if (!item.is_object()) {
						continue;
					}
					StartClosure closure;
					closure.ru = item.value("ru", ""s);
					closure.en = item.value("en", ""s);
					if (const auto id = ResolveRefID(item, "faction")) {
						closure.faction = RE::TESForm::GetFormByID<RE::TESFaction>(id);
					}
					if (const auto id = ResolveRefID(item, "global")) {
						closure.global = RE::TESForm::GetFormByID<RE::TESGlobal>(id);
					}
					if (closure.faction || closure.global) {
						start.closed.push_back(std::move(closure));
					}
				}
			}
			return start;
		}

		// Reads one file into a_out. A quest that has entries in this file loses the entries earlier files
		// gave it (QuestStarts-user.json replaces the built-in ones). Returns entries read.
		std::size_t LoadFile(const std::filesystem::path& a_path, std::vector<QuestStart>& a_out)
		{
			std::ifstream file{ a_path };
			if (!file) {
				return 0;
			}
			const auto doc = nlohmann::json::parse(file, nullptr, false, true);
			const auto list = doc.is_object() ? doc.find("starts") : doc.end();
			if (doc.is_discarded() || list == doc.end() || !list->is_array()) {
				logger::warn("{}: not valid JSON or no \"starts\" array; file ignored"sv, a_path.string());
				return 0;
			}
			const auto              fileName = a_path.filename().string();
			std::vector<QuestStart> read;
			for (const auto& entry : *list) {
				if (auto start = ParseEntry(entry, fileName)) {
					read.push_back(std::move(*start));
				}
			}
			std::erase_if(a_out, [&](const QuestStart& a_old) {
				return std::ranges::find(read, a_old.quest, &QuestStart::quest) != read.end();
			});
			std::ranges::move(read, std::back_inserter(a_out));
			logger::info("{}: {} quest starts"sv, fileName, read.size());
			return read.size();
		}

		[[nodiscard]] bool IsCompleted(const RE::TESQuest& a_quest) noexcept
		{
			return (a_quest.data.flags >> 1) & 1;
		}

		// Data flags bit 0: started or starting (GetQuestRunning, OG rva 4C98E0, also wants bit 7 clear and no
		// promote task, i.e. not still starting).
		[[nodiscard]] bool IsEnabled(const RE::TESQuest& a_quest) noexcept
		{
			return a_quest.data.flags & 1;
		}

		// BGSLocAlias::IsLocAlias (OG id 78630): the alias itself if its QType is the location alias type.
		[[nodiscard]] bool IsLocAlias(const RE::BGSBaseAlias* a_alias)
		{
			using func_t = const RE::BGSBaseAlias*(const RE::BGSBaseAlias*);
			static REL::Relocation<func_t*> func{ REL::ID(78630) };
			return func(a_alias) != nullptr;
		}

		// TESQuest::GetAliasedLoc (OG id 1552479, rva 5DD0A0): the location an alias of a running quest holds;
		// takes aliasAccessLock itself.
		[[nodiscard]] RE::BGSLocation* GetAliasedLoc(const RE::TESQuest& a_quest, std::uint32_t a_aliasID)
		{
			using func_t = RE::BGSLocation*(const RE::TESQuest*, std::uint32_t);
			static REL::Relocation<func_t*> func{ REL::ID(1552479) };
			return func(&a_quest, a_aliasID);
		}

		// BGSLocAlias (no layout in CommonLibF4): fill type 1 = specific location, the location at +0x28
		// (TESQuest::FindLocForAlias, OG rva 5DC1E0, case 1).
		constexpr std::uint16_t kSpecificLocationFill = 1;

		[[nodiscard]] const RE::BGSLocation* SpecificLocation(const RE::BGSBaseAlias& a_alias)
		{
			return *reinterpret_cast<RE::BGSLocation* const*>(reinterpret_cast<const std::byte*>(&a_alias) + 0x28);
		}

		// The running quest (other than a_except) with a reserving location alias holding a_location.
		[[nodiscard]] RE::TESQuest* FindReservingQuest(const RE::BGSLocation& a_location, const RE::TESQuest& a_except)
		{
			const auto dataHandler = RE::TESDataHandler::GetSingleton();
			if (!dataHandler) {
				return nullptr;
			}
			for (const auto quest : dataHandler->GetFormArray<RE::TESQuest>()) {
				if (!quest || quest == &a_except || !IsEnabled(*quest)) {
					continue;
				}
				for (const auto alias : quest->aliases) {
					if (alias && alias->flags.all(RE::BGSBaseAlias::FLAGS::kReserves) && IsLocAlias(alias) &&
						GetAliasedLoc(*quest, alias->aliasID) == &a_location) {
						return quest;
					}
				}
			}
			return nullptr;
		}

		// TESFaction::GetPCAnEnemy (OG id 586385): what GetPlayerEnemyofFaction / Faction.IsPlayerEnemy read
		// (Faction.SetPlayerEnemy; also a crime enemy that wears off).
		[[nodiscard]] bool GetPCAnEnemy(const RE::TESFaction& a_faction)
		{
			using func_t = bool(const RE::TESFaction*);
			static REL::Relocation<func_t*> func{ REL::ID(586385) };
			return func(&a_faction);
		}

		// TESReactionForm::GetFactionCombatRelation (OG id 167267): the fight reaction toward a_other; 1 = enemy,
		// what Faction.SetEnemy sets both ways (mem_Faction_SetEnemy -> TESFaction::SetFactionFightReaction(.., 1)).
		// TESFaction's TESReactionForm is at +0x30 (TESFaction::GetFactionReaction, OG rva 5A5970).
		[[nodiscard]] std::int32_t GetCombatRelation(const RE::TESFaction& a_faction, const RE::TESForm* a_other)
		{
			using func_t = std::int32_t(const void*, const RE::TESForm*);
			static REL::Relocation<func_t*> func{ REL::ID(167267) };
			return func(reinterpret_cast<const std::byte*>(&a_faction) + 0x30, a_other);
		}

		constexpr std::uint32_t kPlayerFactionID = 0x1C21C;  // Fallout4.esm PlayerFaction
		constexpr std::int32_t  kEnemyRelation = 1;

		// The faction kick-outs: BoS/Railroad/Minutemen (Nuka-World raids) SetPlayerEnemy, Institute/BoS/Railroad
		// (MQ00Script.MQFactionKickoutPlayer) and Far Harbor/Acadia SetEnemy with PlayerFaction.
		[[nodiscard]] bool IsPlayerEnemy(const RE::TESFaction& a_faction)
		{
			if (GetPCAnEnemy(a_faction)) {
				return true;
			}
			const auto playerFaction = RE::TESForm::GetFormByID<RE::TESFaction>(kPlayerFactionID);
			return playerFaction && (GetCombatRelation(a_faction, playerFaction) == kEnemyRelation ||
										GetCombatRelation(*playerFaction, &a_faction) == kEnemyRelation);
		}

		[[nodiscard]] const StartClosure* ClosedBy(const QuestStart& a_start)
		{
			for (const auto& closure : a_start.closed) {
				if ((closure.faction && IsPlayerEnemy(*closure.faction)) || (closure.global && closure.global->GetValue() != 0.0f)) {
					return &closure;
				}
			}
			return nullptr;
		}

		// --- Radiant offers: the giver's offer lines evaluated by the engine -----------------------------------

		// TESConditionItem::IsTrue(ConditionCheckParams&) (OG id 43452): one condition with the quest scope set,
		// so GetIsAliasRef reads the line's quest.
		[[nodiscard]] bool ItemIsTrue(RE::TESConditionItem& a_item, RE::ConditionCheckParams& a_params)
		{
			using func_t = bool(RE::TESConditionItem*, RE::ConditionCheckParams&);
			static REL::Relocation<func_t*> func{ REL::ID(43452) };
			return func(&a_item, a_params);
		}

		// Condition functions that do not read the subject (the giver): judged even while it is not in memory.
		[[nodiscard]] bool IsSubjectFree(std::uint32_t a_function) noexcept
		{
			switch (a_function) {
			case 56:   // GetQuestRunning
			case 58:   // GetStage
			case 59:   // GetStageDone
			case 74:   // GetGlobalValue
			case 543:  // GetQuestCompleted
			case 629:  // GetVMQuestVariable
				return true;
			default:
				return false;
			}
		}

		// TESCondition::IsTrue<ConditionItemIsTrue> (OG rva 72B6B0): an item with the OR flag joins the next one,
		// such groups are ANDed. Without a subject the items that read it count as true (the giver is assumed
		// to be in place: it is where the marker leads).
		[[nodiscard]] bool ConditionsHold(const RE::TESCondition& a_conditions, RE::ConditionCheckParams& a_params)
		{
			bool all = true;
			bool inOr = false;
			bool group = false;
			for (auto item = a_conditions.head; item; item = item->next) {
				const auto function = static_cast<std::uint32_t>(std::to_underlying(item->data.functionData.function.get())) & 0xFFF;
				const bool needsSubject = item->data.object.get() == RE::CONDITIONITEMOBJECT::kSelf && !IsSubjectFree(function);
				const auto value = [&]() { return (!a_params.actionRef && needsSubject) || ItemIsTrue(*item, a_params); };
				if (inOr) {
					group = group || value();
				} else {
					group = value();
				}
				if (item->data.compareOr) {
					inOr = true;
					continue;
				}
				inOr = false;
				all = all && group;
				if (!all) {
					return false;
				}
			}
			return inOr ? all && group : all;
		}

		// A radiant start is offered while one of its lines could be said now: the line's quest is running (a
		// quest's dialogue exists only then) and its conditions hold for the giver and the player.
		[[nodiscard]] bool Offered(const QuestStart& a_start, RE::TESObjectREFR* a_live)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			for (const auto id : a_start.infos) {
				const auto info = RE::TESForm::GetFormByID<RE::TESTopicInfo>(id);
				const auto owner = info && info->parentTopic ? info->parentTopic->ownerQuest : nullptr;
				if (!info || (owner && !IsEnabled(*owner))) {
					continue;
				}
				RE::ConditionCheckParams params;
				params.actionRef = a_live;
				params.targetRef = player;
				params.scopeQuest = owner;
				if (ConditionsHold(info->objConditions, params)) {
					return true;
				}
			}
			return false;
		}

		// Actor::CalculateFactionFightReaction (OG id 339469, rva DA3190): how a_actor would react to a_other -
		// factions with the ref's own changes (FactionUtil::IntegrateFactionArrays), the crime faction's
		// GetPCAnEnemy, Actor::IsAngryWithPlayer (assault), relationships. Actor::GetFactionFightReaction calls it
		// with false when the actor has no AI process; with one it goes through AIProcess::CreateActorKnowledge,
		// which changes AI state - not used.
		[[nodiscard]] std::int32_t CalculateFightReaction(RE::Actor& a_actor, RE::Actor& a_other)
		{
			using func_t = std::int32_t(RE::Actor*, RE::Actor*, bool);
			static REL::Relocation<func_t*> func{ REL::ID(339469) };
			return func(&a_actor, &a_other, false);
		}

		// The giver would fight the player: in memory (a persistent ref always, a temporary one while its cell is
		// loaded) the engine's fight reaction; otherwise a faction of its base NPC that is hostile to the player
		// (changes made on the ref itself are not known then).
		[[nodiscard]] bool IsGiverHostile(const QuestStart& a_start, RE::TESObjectREFR* a_live)
		{
			const auto player = RE::PlayerCharacter::GetSingleton();
			if (a_live) {
				const auto actor = a_live->As<RE::Actor>();
				return actor && player && actor != player && CalculateFightReaction(*actor, *player) == kEnemyRelation;
			}
			const auto npc = a_start.giverBaseID ? RE::TESForm::GetFormByID<RE::TESNPC>(a_start.giverBaseID) : nullptr;
			if (!npc) {
				return false;
			}
			return std::ranges::any_of(npc->factions, [](const RE::FACTION_RANK& a_rank) {
				return a_rank.faction && a_rank.rank >= 0 && IsPlayerEnemy(*a_rank.faction);
			});
		}

		// Quests whose giver was seen not offering them -> the player's location then (see NotOffered).
		std::mutex                                                     notOfferedLock;
		std::unordered_map<const RE::TESQuest*, const RE::BGSLocation*> notOffered;
		std::unordered_set<const RE::TESQuest*>                          heldBlocked;  // FindStartBlocker found a holder (logged once)

		// Same place: the same location, or one inside the other, but not a top-level one (the Commonwealth
		// holds every location of the world).
		[[nodiscard]] bool SamePlace(const RE::BGSLocation* a_seen, const RE::BGSLocation* a_here)
		{
			if (!a_seen || !a_here) {
				return false;
			}
			return a_seen == a_here || a_seen->IsChild(a_here) || (a_here->parentLoc && a_here->IsChild(a_seen));
		}

		// A `running` start whose quest is not running: its dialogue lives in the quest's own topics, so the
		// giver has nothing to say about it. Judged only while the giver is loaded near the player (before
		// that the Story Manager may still start the quest, e.g. on entering the giver's location), then
		// remembered until the player leaves that place.
		[[nodiscard]] bool NotOffered(const QuestStart& a_start, const RE::TESObjectREFR* a_live)
		{
			const auto              player = RE::PlayerCharacter::GetSingleton();
			const auto              here = player ? player->currentLocation : nullptr;
			std::scoped_lock        lock{ notOfferedLock };
			if (!a_start.running || IsEnabled(*a_start.quest)) {
				notOffered.erase(a_start.quest);
				heldBlocked.erase(a_start.quest);
				return false;
			}
			// A place it needs is held by another running quest: it cannot start anywhere (the Story Manager's
			// next try fails too) until that quest lets go - not offered however far the giver is.
			if (const auto blocker = FindStartBlocker(*a_start.quest)) {
				if (heldBlocked.insert(a_start.quest).second) {
					logger::info("start {}: quest not running, location {:08X} {} held by {} -> not offered anywhere"sv,
						a_start.name, blocker->location->GetFormID(), RE::TESFullName::GetFullName(*blocker->location),
						DescribeQuest(*blocker->holder));
				}
				return true;
			}
			if (heldBlocked.erase(a_start.quest)) {
				logger::info("start {}: the location it needs is free again"sv, a_start.name);
			}
			if (a_live && a_live->Get3D()) {
				if (notOffered.insert_or_assign(a_start.quest, here).second) {
					logger::info("start {}: giver loaded, quest not running -> not offered now"sv, a_start.name);
				}
				return true;
			}
			const auto it = notOffered.find(a_start.quest);
			if (it == notOffered.end()) {
				return false;
			}
			if (SamePlace(it->second, here)) {
				return true;
			}
			logger::info("start {}: player left the place, the quest may start again"sv, a_start.name);
			notOffered.erase(it);
			return false;
		}

		// TESQuest::IsStageDone (OG rva 5D7370) without its hash table: TESQuestStage +0x18 is the stage
		// index, bit 0 of +0x1A is "done". TESQuestStage has no layout in CommonLibF4.
		[[nodiscard]] bool IsStageDone(const RE::TESQuest& a_quest, std::uint16_t a_stage) noexcept
		{
			for (const auto stage : a_quest.stages) {
				const auto bytes = reinterpret_cast<const std::byte*>(stage);
				if (bytes && *reinterpret_cast<const std::uint16_t*>(bytes + 0x18) == a_stage) {
					return (std::to_integer<std::uint8_t>(bytes[0x1A]) & 1) != 0;
				}
			}
			return false;
		}

		[[nodiscard]] bool IsStarted(const QuestStart& a_start, const ShownObjectives& a_shown)
		{
			if (a_start.radiant) {
				return a_shown.contains(a_start.quest);  // taken: in the Pip-Boy; completed ones come again
			}
			if (IsCompleted(*a_start.quest)) {
				return true;
			}
			if (a_start.stage) {
				return IsStageDone(*a_start.quest, *a_start.stage);
			}
			const auto it = a_shown.find(a_start.quest);
			if (it == a_shown.end()) {
				return false;
			}
			return !a_start.objective || std::ranges::find(it->second, *a_start.objective) != it->second.end();
		}

		// The giver if it is in memory; a temporary ref is only while its cell is loaded.
		RE::TESObjectREFR* LiveRef(std::uint32_t a_id)
		{
			const auto form = a_id ? RE::TESForm::GetFormByID(a_id) : nullptr;
			const auto ref = form ? form->As<RE::TESObjectREFR>() : nullptr;
			return ref && !ref->IsDeleted() ? ref : nullptr;
		}

		// Same test as Workshop::IsDeadActor: IsDead(true) (vfunc C0), for actors only.
		[[nodiscard]] bool IsGone(RE::TESObjectREFR& a_ref)
		{
			return a_ref.IsDisabled() || (a_ref.Is(RE::ENUM_FORM_ID::kACHR) && a_ref.IsDead(true));
		}

		enum class Status
		{
			kAvailable,
			kStarted,     // completed, or an objective has been shown
			kClosed,      // a `closed` condition holds (faction hostile)
			kNoOffer,     // radiant: the giver has nothing to offer now
			kGiverHostile,  // the giver would fight the player
			kLocked,      // an `after` quest is not completed
			kGiverGone,   // giver dead or disabled
			kNotOffered,  // giver near, quest not running (NotOffered)
			kNoMarker,    // nothing in memory to put the marker on
		};

		std::string_view StatusName(Status a_status) noexcept
		{
			switch (a_status) {
			case Status::kAvailable:
				return "available"sv;
			case Status::kStarted:
				return "started"sv;
			case Status::kClosed:
				return "closed (faction hostile)"sv;
			case Status::kNoOffer:
				return "radiant: no offer now"sv;
			case Status::kGiverHostile:
				return "giver hostile"sv;
			case Status::kLocked:
				return "locked"sv;
			case Status::kGiverGone:
				return "giver gone"sv;
			case Status::kNotOffered:
				return "not offered now"sv;
			default:
				return "no marker ref"sv;
			}
		}

		struct StartState
		{
			Status             status{ Status::kAvailable };
			RE::TESObjectREFR* live{ nullptr };
			std::uint32_t      markerID{ 0 };
		};

		StartState Evaluate(const QuestStart& a_start, const ShownObjectives& a_shown)
		{
			StartState state;
			if (IsStarted(a_start, a_shown)) {
				state.status = Status::kStarted;
				return state;
			}
			if (ClosedBy(a_start)) {
				state.status = Status::kClosed;
				return state;
			}
			const auto groupDone = [](const std::vector<RE::TESQuest*>& a_group) {
				return std::ranges::any_of(a_group, [](const RE::TESQuest* a_quest) { return IsCompleted(*a_quest); });
			};
			if (!std::ranges::all_of(a_start.after, groupDone)) {
				state.status = Status::kLocked;
				return state;
			}
			state.live = LiveRef(a_start.refID);
			if (state.live && IsGone(*state.live)) {
				state.status = Status::kGiverGone;
				return state;
			}
			if (IsGiverHostile(a_start, state.live)) {
				state.status = Status::kGiverHostile;
				return state;
			}
			if (NotOffered(a_start, state.live)) {
				state.status = Status::kNotOffered;
				return state;
			}
			if (a_start.radiant && !Offered(a_start, state.live)) {
				state.status = Status::kNoOffer;
				return state;
			}
			if (state.live) {
				state.markerID = a_start.refID;
			} else if (LiveRef(a_start.markerID)) {
				state.markerID = a_start.markerID;
			} else if (LiveRef(a_start.anchorID)) {
				state.markerID = a_start.anchorID;
			} else {
				state.status = Status::kNoMarker;
			}
			return state;
		}

		std::optional<Distance> MeasureToRef(const RE::PlayerCharacter& a_player, const std::optional<RootPoint>& a_playerRoot,
			std::span<const WorldEntry> a_entries, const QuestStart& a_start, const RE::TESObjectREFR* a_live)
		{
			if (a_live) {
				if (const auto straight = MeasureStraight(a_player, *a_live); straight && straight->kind != DistanceKind::kOtherSpace) {
					return straight;
				}
				if (const auto root = RootOf(*a_live)) {
					return MeasureBetween(a_playerRoot, *root, a_entries);
				}
			}
			// The giver is not in memory: the door inside its cell, if that cell opens straight into a world;
			// else the static point (the anchor outside).
			if (const auto marker = a_live ? nullptr : LiveRef(a_start.markerID)) {
				if (const auto root = WorldDoorRoot(*marker)) {
					return MeasureBetween(a_playerRoot, *root, a_entries);
				}
			}
			if (a_start.world) {
				return MeasureBetween(a_playerRoot, ToRoot(a_start.world, a_start.pos), a_entries);
			}
			return std::nullopt;
		}

		// A radio start is reached at the edge of the reception radius, not at the transmitter.
		std::optional<Distance> MeasureStart(const RE::PlayerCharacter& a_player, const std::optional<RootPoint>& a_playerRoot,
			std::span<const WorldEntry> a_entries, const QuestStart& a_start, const RE::TESObjectREFR* a_live)
		{
			auto distance = MeasureToRef(a_player, a_playerRoot, a_entries, a_start, a_live);
			if (distance && a_start.radioRadius > 0.0f && distance->kind != DistanceKind::kOtherSpace) {
				distance->value = std::max(distance->value - a_start.radioRadius, 0.0f);
			}
			return distance;
		}

		std::string DescribeRoot(const std::optional<RootPoint>& a_point)
		{
			return a_point ? std::format("world {:08X} ({:.0f}, {:.0f}, {:.0f})"sv, a_point->world ? a_point->world->GetFormID() : 0u,
								 a_point->pos.x, a_point->pos.y, a_point->pos.z) :
			                 "none"s;
		}
	}

	void LoadQuestStarts()
	{
		starts.clear();
		const std::filesystem::path dir{ kDataDir };
		if (LoadFile(dir / "QuestStarts.json", starts) == 0) {
			logger::warn("QuestStarts.json: no quest starts loaded"sv);
		}
		LoadFile(dir / "QuestStarts-user.json", starts);
		logger::info("{} quest starts in total"sv, starts.size());
	}

	const QuestStart* FindQuestStart(const RE::TESQuest& a_quest)
	{
		const auto it = std::ranges::find(starts, &a_quest, &QuestStart::quest);
		return it != starts.end() ? std::to_address(it) : nullptr;
	}

	std::vector<RE::TESQuest*> GetQuestsWithStarts()
	{
		std::vector<RE::TESQuest*> result;
		for (const auto& start : starts) {
			if (std::ranges::find(result, start.quest) == result.end()) {
				result.push_back(start.quest);
			}
		}
		return result;
	}

	bool IsQuestCompleted(const RE::TESQuest& a_quest) noexcept
	{
		return IsCompleted(a_quest);
	}

	const QuestStart* FindAvailableStart(const RE::TESQuest& a_quest)
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return nullptr;
		}
		const auto        shown = GetQuestsWithObjectives();
		const auto        playerRoot = PlayerRoot(*player);
		const auto        entries = CollectWorldEntries();
		const QuestStart* best = nullptr;
		double            bestKey = 0.0;
		for (const auto& start : starts) {
			if (start.quest != &a_quest) {
				continue;
			}
			const auto state = Evaluate(start, shown);
			if (state.status != Status::kAvailable) {
				continue;
			}
			const auto distance = MeasureStart(*player, playerRoot, entries, start, state.live);
			const auto key = distance ? distance->SortKey() : std::numeric_limits<double>::max();
			if (!best || key < bestKey) {
				best = &start;
				bestKey = key;
			}
		}
		return best;
	}

	bool IsStartNotOffered(const RE::TESQuest& a_quest)
	{
		const auto shown = GetQuestsWithObjectives();
		bool       refused = false;
		for (const auto& start : starts) {
			if (start.quest != &a_quest) {
				continue;
			}
			const auto status = Evaluate(start, shown).status;
			if (status == Status::kAvailable) {
				return false;
			}
			refused |= status == Status::kNotOffered;
		}
		return refused;
	}

	bool IsStartGiverHostile(const RE::TESQuest& a_quest)
	{
		const auto shown = GetQuestsWithObjectives();
		bool       hostile = false;
		for (const auto& start : starts) {
			if (start.quest != &a_quest) {
				continue;
			}
			const auto status = Evaluate(start, shown).status;
			if (status == Status::kAvailable) {
				return false;
			}
			hostile |= status == Status::kGiverHostile;
		}
		return hostile;
	}

	const StartClosure* FindStartClosure(const RE::TESQuest& a_quest)
	{
		const auto          shown = GetQuestsWithObjectives();
		const StartClosure* first = nullptr;
		for (const auto& start : starts) {
			if (start.quest != &a_quest) {
				continue;
			}
			if (IsStarted(start, shown)) {
				return nullptr;
			}
			const auto closure = ClosedBy(start);
			if (!closure) {
				return nullptr;
			}
			first = first ? first : closure;
		}
		return first;
	}

	std::optional<StartBlocker> FindStartBlocker(const RE::TESQuest& a_quest)
	{
		using Flags = RE::BGSBaseAlias::FLAGS;
		for (const auto alias : a_quest.aliases) {
			if (!alias || alias->fillType != kSpecificLocationFill || alias->flags.any(Flags::kOptional, Flags::kAllowReserved) ||
				!IsLocAlias(alias)) {
				continue;
			}
			const auto location = SpecificLocation(*alias);
			if (const auto holder = location ? FindReservingQuest(*location, a_quest) : nullptr) {
				return StartBlocker{ location, holder };
			}
		}
		return std::nullopt;
	}

	bool IsPointerBlocked(const RE::TESQuest& a_quest)
	{
		const auto it = std::ranges::find_if(starts, [&](const QuestStart& a_start) {
			return std::ranges::find(a_start.pointers, &a_quest) != a_start.pointers.end();
		});
		if (it == starts.end()) {
			return false;
		}
		const auto& target = *it->quest;
		return IsStartNotOffered(target) || FindStartClosure(target) || IsStartGiverHostile(target);
	}

	std::vector<Candidate> CollectStartCandidates(bool a_normal, bool a_radiant)
	{
		std::vector<Candidate> result;
		const auto             player = RE::PlayerCharacter::GetSingleton();
		if (!player || starts.empty()) {
			return result;
		}
		const auto withObjectives = GetQuestsWithObjectives();
		const auto playerRoot = PlayerRoot(*player);
		const auto entries = CollectWorldEntries();
		for (const auto& start : starts) {
			if (!(start.radiant ? a_radiant : a_normal)) {
				continue;
			}
			const auto state = Evaluate(start, withObjectives);
			if (state.status != Status::kAvailable) {
				continue;
			}
			const auto distance = MeasureStart(*player, playerRoot, entries, start, state.live);
			if (!distance) {
				logger::debug("  start {}: no position"sv, start.name);
				continue;
			}
			Candidate candidate;
			candidate.quest = start.quest;
			candidate.distance = *distance;
			candidate.start = &start;
			candidate.markerID = state.markerID;
			const auto liveRoot = state.live ? RootOf(*state.live) : std::nullopt;
			candidate.world = liveRoot ? liveRoot->world : RootWorld(start.world);
			result.push_back(candidate);
		}
		return result;
	}

	std::uint32_t GetStartMarkerID(const QuestStart& a_start)
	{
		const auto state = Evaluate(a_start, GetQuestsWithObjectives());
		return state.status == Status::kAvailable ? state.markerID : 0;
	}

	std::string GetStartName(const RE::TESQuest& a_quest)
	{
		const std::string_view full = RE::TESFullName::GetFullName(a_quest);
		// A radiant whose name is made of aliases (<Alias=...>, filled only while it runs): the giver's name.
		if (const auto start = FindQuestStart(a_quest); start && start->radiant && full.find('<') != std::string_view::npos) {
			const auto live = LiveRef(start->refID);
			const auto liveName = live ? live->GetDisplayFullName() : nullptr;
			if (liveName && *liveName) {
				return liveName;
			}
			if (const auto npc = start->giverBaseID ? RE::TESForm::GetFormByID(start->giverBaseID) : nullptr) {
				const auto name = RE::TESFullName::GetFullName(*npc);
				if (!name.empty()) {
					return std::string{ name };
				}
			}
		}
		std::string            name;
		bool                   inTag = false;
		for (const auto ch : full) {
			if (ch == '<') {
				inTag = true;
			} else if (ch == '>') {
				inTag = false;
			} else if (!inTag) {
				name.push_back(ch);
			}
		}
		const auto first = name.find_first_not_of(" :");
		const auto last = name.find_last_not_of(" :");
		return first == std::string::npos ? std::string{} : name.substr(first, last - first + 1);
	}

	std::string GetStartRadioName(const QuestStart& a_start)
	{
		const auto ref = a_start.radioRadius > 0.0f ? LiveRef(a_start.refID) : nullptr;
		const auto name = ref ? ref->GetDisplayFullName() : nullptr;
		return name ? std::string{ name } : std::string{};
	}

	void DumpQuestStarts()
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		const auto playerRoot = PlayerRoot(*player);
		logger::info("=== quest starts ({}); player root {}, exteriorPosition ({:.0f}, {:.0f}, {:.0f}) world {:08X} ==="sv,
			starts.size(), DescribeRoot(playerRoot), player->exteriorPosition.x, player->exteriorPosition.y,
			player->exteriorPosition.z, player->cachedWorldspace ? player->cachedWorldspace->GetFormID() : 0u);
		const auto withObjectives = GetQuestsWithObjectives();
		const auto entries = CollectWorldEntries();
		std::map<std::uint32_t, std::pair<std::size_t, float>> ways;  // root world -> (count, shortest)
		for (const auto& entry : entries) {
			const auto root = RootWorld(entry.world);
			auto&      way = ways.try_emplace(root ? root->GetFormID() : 0u, 0, entry.length).first->second;
			way = { way.first + 1, std::min(way.second, entry.length) };
		}
		for (const auto& [world, way] : ways) {
			logger::info("  ways into world {:08X}: {}, shortest {:.0f}"sv, world, way.first, way.second);
		}
		for (const auto& start : starts) {
			const auto state = Evaluate(start, withObjectives);
			const auto distance = state.status == Status::kAvailable ? MeasureStart(*player, playerRoot, entries, start, state.live) : std::nullopt;
			logger::info("  {} {}{}{}{}: {}, giver {:08X}{}{}, marker {:08X}, root {}{}"sv, DescribeQuest(*start.quest), start.name,
				start.objective ? std::format(" (objective {})"sv, *start.objective) : ""s,
				start.stage ? std::format(" (stage {})"sv, *start.stage) : ""s,
				start.running ? std::format(" (running: {})"sv, IsEnabled(*start.quest)) : ""s,
				StatusName(state.status), start.refID, state.live ? " (in memory)"sv : ""sv,
				start.radioRadius > 0.0f ? std::format(", radio \"{}\" radius {:.0f}"sv, GetStartRadioName(start), start.radioRadius) : ""s,
				state.markerID, state.live ? DescribeRoot(RootOf(*state.live)) : "-"s,
				distance ? std::format(" -> {} {:.0f}"sv, KindName(distance->kind), distance->value) : ""s);
		}
		logger::info("=== end of quest starts ==="sv);
	}
}
