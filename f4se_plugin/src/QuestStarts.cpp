#include "QuestStarts.h"

#include "EngineTypes.h"

#include <nlohmann/json.hpp>

namespace NQT
{
	namespace
	{
		constexpr auto kDataDir = "Data/F4SE/Plugins/NearestQuestTracker"sv;

		std::vector<QuestStart> starts;

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
			kLocked,      // an `after` quest is not completed
			kGiverGone,   // giver dead or disabled
			kNoMarker,    // nothing in memory to put the marker on
		};

		std::string_view StatusName(Status a_status) noexcept
		{
			switch (a_status) {
			case Status::kAvailable:
				return "available"sv;
			case Status::kStarted:
				return "started"sv;
			case Status::kLocked:
				return "locked"sv;
			case Status::kGiverGone:
				return "giver gone"sv;
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

		// Straight line in the same root world; else the best way in through a door (WorldEntry): path length
		// to the door + straight line from where it comes out; else kOtherSpace.
		Distance Between(const std::optional<RootPoint>& a_from, const RootPoint& a_to, std::span<const WorldEntry> a_entries)
		{
			if (a_from && a_to.world && a_from->world == a_to.world) {
				return Distance{ a_from->pos.GetDistance(a_to.pos), DistanceKind::kStraight };
			}
			std::optional<float> best;
			for (const auto& entry : a_entries) {
				if (a_to.world && RootWorld(entry.world) == a_to.world) {
					const auto length = entry.length + entry.point.GetDistance(a_to.pos);
					if (!best || length < *best) {
						best = length;
					}
				}
			}
			return best ? Distance{ *best, DistanceKind::kViaDoors } : Distance{ 0.0f, DistanceKind::kOtherSpace };
		}

		std::optional<Distance> MeasureToRef(const RE::PlayerCharacter& a_player, const std::optional<RootPoint>& a_playerRoot,
			std::span<const WorldEntry> a_entries, const QuestStart& a_start, const RE::TESObjectREFR* a_live)
		{
			if (a_live) {
				if (const auto straight = MeasureStraight(a_player, *a_live); straight && straight->kind != DistanceKind::kOtherSpace) {
					return straight;
				}
				if (const auto root = RootOf(*a_live)) {
					return Between(a_playerRoot, *root, a_entries);
				}
			}
			if (a_start.world) {
				return Between(a_playerRoot, RootPoint{ RootWorld(a_start.world), a_start.pos }, a_entries);
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

	std::vector<Candidate> CollectStartCandidates()
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
			logger::info("  {} {}{}{}: {}, giver {:08X}{}{}, marker {:08X}, root {}{}"sv, DescribeQuest(*start.quest), start.name,
				start.objective ? std::format(" (objective {})"sv, *start.objective) : ""s,
				start.stage ? std::format(" (stage {})"sv, *start.stage) : ""s,
				StatusName(state.status), start.refID, state.live ? " (in memory)"sv : ""sv,
				start.radioRadius > 0.0f ? std::format(", radio \"{}\" radius {:.0f}"sv, GetStartRadioName(start), start.radioRadius) : ""s,
				state.markerID, state.live ? DescribeRoot(RootOf(*state.live)) : "-"s,
				distance ? std::format(" -> {} {:.0f}"sv, KindName(distance->kind), distance->value) : ""s);
		}
		logger::info("=== end of quest starts ==="sv);
	}
}
