#include "QuestDistance.h"

#include "EngineTypes.h"
#include "MapMarkers.h"
#include "QuestStarts.h"

namespace NQT
{
	namespace
	{
		// NearestQuestTracker.esp's own marker quests (start marker, location marker): never candidates.
		std::array<const RE::TESQuest*, 2> markerQuests{};

		bool IsMarkerQuest(const RE::TESQuest* a_quest) noexcept
		{
			return std::ranges::find(markerQuests, a_quest) != markerQuests.end();
		}

		float Length(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to) noexcept
		{
			const auto dx = a_to.x - a_from.x;
			const auto dy = a_to.y - a_from.y;
			const auto dz = a_to.z - a_from.z;
			const auto length = std::sqrt(dx * dx + dy * dy + dz * dz);
			return std::isnan(length) ? 0.0f : length;
		}

		// TeleportPath::ComputeLength (id 95565) rewritten, except that an empty path gives
		// nullopt instead of -1 (the engine's own FindNearestObjective lets -1 win).
		// Legs: start -> first door, exit point of each door -> next door, last exit -> end.
		std::optional<float> PathLength(const Engine::TeleportPath& a_path) noexcept
		{
			if (a_path.sections.empty()) {
				return std::nullopt;
			}
			float total = 0.0f;
			auto  from = a_path.start;
			for (const auto& hop : a_path.doors) {
				if (hop.door) {
					total += Length(from, hop.door->GetPosition());
				}
				from = hop.exitPoint;
			}
			return total + Length(from, a_path.end);
		}

		std::optional<Distance> MeasureRefData(const RE::TESObjectREFR& a_player, const Engine::QuestTargetRefData& a_ref)
		{
			if (const auto length = PathLength(a_ref.path)) {
				return Distance{ *length, DistanceKind::kPath };
			}
			const auto target = a_ref.target.get();
			return target ? MeasureStraight(a_player, *target) : std::nullopt;
		}

		template <class F>
		void ForEachRefData(const Engine::QuestTargetArray& a_array, F&& a_visit)
		{
			for (const auto target : a_array.targets) {
				if (!target) {
					continue;
				}
				for (const auto ref : target->refs) {
					if (ref) {
						a_visit(*target, *ref);
					}
				}
			}
		}

		// Caller holds player.questTargetLock: the engine updates objectives under it too
		// (PlayerCharacter::UpdateInstancedQuestObjective).
		std::vector<RE::TESQuest*> DisplayedQuestsLocked(const RE::PlayerCharacter& a_player)
		{
			std::vector<RE::TESQuest*> result;
			for (const auto& instance : a_player.objectives) {
				const auto objective = Engine::AsObjective(instance.objective);
				if (!objective || !objective->ownerQuest || IsMarkerQuest(objective->ownerQuest) || !Engine::IsQuestTrackable(*objective->ownerQuest) ||
					instance.enstanceState.underlying() != std::to_underlying(Engine::ObjectiveState::kDisplayed)) {
					continue;
				}
				if (std::ranges::find(result, objective->ownerQuest) == result.end()) {
					result.push_back(objective->ownerQuest);
				}
			}
			return result;
		}

		// The cell a ref is in; for a persistent ref whose cell is not loaded (no parent cell, checked in the
		// 2026-10-01 dump) the persistent cell of its world from ExtraPersistentCell, as the engine's
		// TESObjectREFR::GetWorldSpace (OG rva 40F290) does through GetSaveParentCell (vfunc 0xBE).
		const RE::TESObjectCELL* CellOf(const RE::TESObjectREFR& a_ref)
		{
			const auto cell = a_ref.GetParentCell();
			return cell ? cell : a_ref.GetSaveParentCell();
		}

		// TESObjectCELL::GetPersistentCell (OG rva 3AF4A0): formFlags bit 10.
		bool IsPersistentCell(const RE::TESObjectCELL& a_cell) noexcept
		{
			return (a_cell.formFlags >> 10) & 1;
		}

		// The door a load door leads to (ExtraTeleport); null if none.
		RE::NiPointer<RE::TESObjectREFR> LinkedDoor(const RE::TESObjectREFR* a_door)
		{
			const auto extra = a_door && a_door->extraList ? a_door->extraList->GetByType(RE::EXTRA_DATA_TYPE::kTeleport) : nullptr;
			const auto data = extra ? static_cast<const RE::ExtraTeleport*>(extra)->teleportData : nullptr;
			return data ? data->linkedDoor.get() : nullptr;
		}

		// A door from a worldspace into an interior: the world side and the interior side.
		struct WorldDoor
		{
			RE::TESObjectREFR* outside;
			RE::TESObjectREFR* inside;
		};

		// Interior cell -> its doors straight out into a worldspace, collected once from the persistent cells of
		// all worldspaces (load doors are persistent; all 522 doors into interiors are found there, 2026-10-04 dump).
		const std::unordered_map<const RE::TESObjectCELL*, std::vector<WorldDoor>>& WorldDoorsByInterior()
		{
			static const auto doors = [] {
				std::unordered_map<const RE::TESObjectCELL*, std::vector<WorldDoor>> result;
				const auto dataHandler = RE::TESDataHandler::GetSingleton();
				if (!dataHandler) {
					return result;
				}
				for (const auto world : dataHandler->GetFormArray<RE::TESWorldSpace>()) {
					const auto cell = world ? world->persistentCell : nullptr;
					if (!cell) {
						continue;
					}
					RE::BSAutoLock lock{ cell->spinLock };  // TESObjectCELL::CellRefLockEnter (OG rva 3C4720)
					for (const auto& ref : cell->references) {
						const auto inside = ref ? LinkedDoor(ref.get()) : nullptr;
						const auto interior = inside ? CellOf(*inside) : nullptr;
						if (interior && interior->IsInterior()) {
							result[interior].push_back({ ref.get(), inside.get() });
						}
					}
				}
				return result;
			}();
			return doors;
		}

		// For a ref in an interior with a door straight out into a worldspace: the outside door of the nearest
		// such door (2026-10-06 log: an interior measured by its location's world marker put all Diamond City
		// houses at the city gate, so from inside one house every other was 0 units away).
		RE::TESObjectREFR* WorldDoorOf(const RE::TESObjectCELL& a_interior, const RE::NiPoint3& a_pos)
		{
			const auto& doors = WorldDoorsByInterior();
			const auto  it = doors.find(&a_interior);
			if (it == doors.end()) {
				return nullptr;
			}
			RE::TESObjectREFR* best = nullptr;
			float              bestLength = 0.0f;
			for (const auto& door : it->second) {
				if (door.outside->IsDisabled()) {
					continue;
				}
				const auto length = Length(a_pos, door.inside->GetPosition());
				if (!best || length < bestLength) {
					best = door.outside;
					bestLength = length;
				}
			}
			return best;
		}

		std::optional<RootPoint> ExteriorRoot(const RE::TESObjectREFR& a_ref, const RE::TESObjectCELL& a_cell)
		{
			const auto world = WorldOfCell(&a_cell);
			return world ? std::optional{ ToRoot(world, a_ref.GetPosition()) } : std::nullopt;
		}

		std::optional<RootPoint> WorldDoorRootIn(const RE::TESObjectREFR& a_ref, const RE::TESObjectCELL& a_interior)
		{
			const auto door = WorldDoorOf(a_interior, a_ref.GetPosition());
			const auto doorCell = door ? CellOf(*door) : nullptr;
			return doorCell && !doorCell->IsInterior() ? ExteriorRoot(*door, *doorCell) : std::nullopt;
		}

		std::optional<RootPoint> RootOfImpl(const RE::TESObjectREFR& a_ref, int a_depth)
		{
			const auto cell = CellOf(a_ref);
			if (!cell) {
				return std::nullopt;
			}
			if (!cell->IsInterior()) {
				return ExteriorRoot(a_ref, *cell);
			}
			if (auto point = WorldDoorRootIn(a_ref, *cell)) {
				return point;
			}
			if (a_depth > 2) {
				return std::nullopt;
			}
			auto location = cell->GetLocation();
			for (int depth = 0; location && depth < 8; ++depth, location = location->parentLoc) {
				const auto marker = location->worldLocMarker.get();
				if (marker && marker.get() != &a_ref) {
					if (auto point = RootOfImpl(*marker, a_depth + 1)) {
						return point;
					}
				}
			}
			return std::nullopt;
		}

		// The world marker of a location or of a parent location.
		std::optional<RootPoint> RootOfLocation(const RE::BGSLocation* a_location)
		{
			for (int depth = 0; a_location && depth < 8; ++depth, a_location = a_location->parentLoc) {
				if (const auto marker = a_location->worldLocMarker.get()) {
					if (auto point = RootOfImpl(*marker, 1)) {
						return point;
					}
				}
			}
			return std::nullopt;
		}

		// Commonwealth, Far Harbor, Nuka-World; null for a DLC that is not installed. Called from natives
		// only, so game data is loaded by the first call.
		const std::array<const RE::TESWorldSpace*, 3>& MainWorlds()
		{
			static const auto worlds = [] {
				std::array<const RE::TESWorldSpace*, 3> result{};
				if (const auto dataHandler = RE::TESDataHandler::GetSingleton()) {
					result[0] = dataHandler->LookupForm<RE::TESWorldSpace>(0x00003C, "Fallout4.esm"sv);
					result[1] = dataHandler->LookupForm<RE::TESWorldSpace>(0x000B0F, "DLCCoast.esm"sv);
					result[2] = dataHandler->LookupForm<RE::TESWorldSpace>(0x00290F, "DLCNukaWorld.esm"sv);
				}
				return result;
			}();
			return worlds;
		}

		// Root world of an exterior ref; null for an interior or a ref without a cell.
		const RE::TESWorldSpace* ExteriorWorld(const RE::TESObjectREFR* a_ref)
		{
			const auto world = a_ref ? WorldOfCell(CellOf(*a_ref)) : nullptr;
			return world ? RootWorld(world) : nullptr;
		}

		// World of the nearest target of a Pip-Boy quest: where the target is on the map; else the last
		// exterior the path goes through (a door hop comes out where the next door stands, the last one
		// beside the door it is linked to; checked in 2026-10-01 log: the door itself stands where the hop
		// starts, so taking its cell put Commonwealth targets reached through the Nuka-World transit into
		// Nuka-World); for a path without doors, the player's world; else the quest's plugin.
		const RE::TESWorldSpace* WorldOfTarget(const RE::TESQuest& a_quest, const Engine::QuestTargetRefData& a_ref,
			const RE::TESWorldSpace* a_playerWorld)
		{
			if (const auto target = a_ref.target.get()) {
				if (const auto root = RootOf(*target)) {
					return root->world;
				}
			}
			const auto& doors = a_ref.path.doors;
			for (auto i = doors.size(); i-- > 0;) {
				const auto exit = i + 1 < doors.size() ? RE::NiPointer<RE::TESObjectREFR>{ doors[i + 1].door } : LinkedDoor(doors[i].door);
				if (const auto world = ExteriorWorld(exit.get())) {
					return world;
				}
				if (const auto world = ExteriorWorld(doors[i].door)) {
					return world;
				}
				// Beyond an interior (a transit cell) lies another world: not the target's.
				if (!doors[i].door || !CellOf(*doors[i].door) || CellOf(*doors[i].door)->IsInterior()) {
					break;
				}
			}
			if (!a_ref.path.sections.empty() && doors.empty() && a_playerWorld) {
				return a_playerWorld;
			}
			return PluginWorld(a_quest);
		}

		// World of a quest without a marker: the world most of the refs and locations in its aliases are in
		// (a tie goes to the quest's plugin world), else its start point, else its plugin.
		const RE::TESWorldSpace* GuessQuestWorld(const RE::TESQuest& a_quest, const RE::TESObjectREFR* a_player)
		{
			std::vector<RE::ObjectRefHandle>    refs;
			std::vector<const RE::BGSLocation*> locations;
			{
				auto&              quest = const_cast<RE::TESQuest&>(a_quest);
				RE::BSAutoReadLock lock{ quest.aliasAccessLock };
				const auto&        aliases = Engine::GetAliasHandles(a_quest);
				for (std::uint32_t i = 0; aliases.data && i < aliases.size; ++i) {
					std::ranges::copy(aliases.data[i].Handles(), std::back_inserter(refs));
				}
				for (const auto& entry : quest.aliasedLocMap) {
					if (entry.second) {
						locations.push_back(entry.second);
					}
				}
			}

			std::vector<std::pair<const RE::TESWorldSpace*, int>> votes;
			const auto vote = [&](const std::optional<RootPoint>& a_point) {
				if (!a_point || !a_point->world) {
					return;
				}
				const auto it = std::ranges::find_if(votes, [&](const auto& a_vote) { return a_vote.first == a_point->world; });
				if (it != votes.end()) {
					++it->second;
				} else {
					votes.emplace_back(a_point->world, 1);
				}
			};
			for (const auto& handle : refs) {
				const auto ref = handle.get();
				if (ref && ref.get() != a_player && !ref->IsDisabled()) {
					vote(RootOf(*ref));
				}
			}
			for (const auto location : locations) {
				vote(RootOfLocation(location));
			}

			const auto               plugin = PluginWorld(a_quest);
			const RE::TESWorldSpace* best = nullptr;
			int                      bestVotes = 0;
			for (const auto& [world, count] : votes) {
				if (count > bestVotes || (count == bestVotes && world == plugin)) {
					best = world;
					bestVotes = count;
				}
			}
			const auto start = FindQuestStart(a_quest);
			logger::debug("  no marker {}: {} alias refs, {} locations, best {:08X} ({} of {} worlds), start {:08X}, plugin {:08X}"sv,
				DescribeQuest(a_quest), refs.size(), locations.size(), best ? best->GetFormID() : 0u, bestVotes, votes.size(),
				start && start->world ? start->world->GetFormID() : 0u, plugin ? plugin->GetFormID() : 0u);
			if (best) {
				return best;
			}
			if (start && start->world) {
				return RootWorld(start->world);
			}
			return plugin;
		}

		std::string DescribeRef(const RE::TESObjectREFR* a_ref)
		{
			if (!a_ref) {
				return "none"s;
			}
			const auto cell = CellOf(*a_ref);
			const auto pos = a_ref->GetPosition();
			std::string world;
			if (cell && !cell->IsInterior()) {
				const auto owner = WorldOfCell(cell);
				world = std::format(" world {:08X}{}"sv, owner ? owner->GetFormID() : 0u,
					IsPersistentCell(*cell) ? std::format(" (persistent cell {}, field {:08X})"sv, static_cast<const void*>(cell),
												  cell->worldSpace ? cell->worldSpace->GetFormID() : 0u) :
											  ""s);
			}
			return std::format("{:08X} cell {:08X}{}{} at ({:.0f}, {:.0f}, {:.0f})"sv,
				a_ref->GetFormID(), cell ? cell->GetFormID() : 0u,
				cell && cell->IsInterior() ? " (interior)"sv : ""sv, world, pos.x, pos.y, pos.z);
		}
	}

	void SetMarkerQuests(const RE::TESQuest* a_startQuest, const RE::TESQuest* a_locationQuest) noexcept
	{
		markerQuests = { a_startQuest, a_locationQuest };
	}

	const RE::TESWorldSpace* RootWorld(const RE::TESWorldSpace* a_world)
	{
		return ToRoot(a_world, RE::NiPoint3{}).world;
	}

	RootPoint ToRoot(const RE::TESWorldSpace* a_world, RE::NiPoint3 a_pos)
	{
		using Flag = RE::TESWorldSpace::PARENT_USE_FLAG;
		for (int depth = 0; a_world && a_world->parentWorld && depth < 8; ++depth) {
			if (!a_world->parentUseFlags.any(Flag::kLand, Flag::kLOD, Flag::kMap)) {
				break;
			}
			// TESWorldSpace::AdjustMapMarkerCoord(pos, false) (OG rva 4936D0): scale around the middle of the
			// world's bounds unless the scale is 1 (or 0), then add the offset (WRLD ONAM).
			const auto& offset = a_world->worldMapOffsetData;
			if (offset.mapScale != 1.0f && offset.mapScale != 0.0f) {
				const auto centerX = (a_world->minimumCoords.x + a_world->maximumCoords.x) * 0.5f;
				const auto centerY = (a_world->minimumCoords.y + a_world->maximumCoords.y) * 0.5f;
				a_pos.x = (a_pos.x - centerX) * offset.mapScale + centerX;
				a_pos.y = (a_pos.y - centerY) * offset.mapScale + centerY;
				a_pos.z *= offset.mapScale;
			}
			a_pos.x += offset.mapOffsetX;
			a_pos.y += offset.mapOffsetY;
			a_pos.z += offset.mapOffsetZ;
			a_world = a_world->parentWorld;
		}
		return RootPoint{ a_world, a_pos };
	}

	std::optional<RootPoint> RootOf(const RE::TESObjectREFR& a_ref)
	{
		return RootOfImpl(a_ref, 0);
	}

	std::optional<RootPoint> WorldDoorRoot(const RE::TESObjectREFR& a_ref)
	{
		const auto cell = CellOf(a_ref);
		return cell && cell->IsInterior() ? WorldDoorRootIn(a_ref, *cell) : std::nullopt;
	}

	std::optional<RootPoint> PlayerRoot(const RE::PlayerCharacter& a_player)
	{
		if (auto point = RootOf(a_player)) {
			return point;
		}
		if (a_player.cachedWorldspace) {
			return ToRoot(a_player.cachedWorldspace, a_player.exteriorPosition);
		}
		return std::nullopt;
	}

	int WorldRank(const RE::TESWorldSpace* a_world, const RE::TESWorldSpace* a_playerWorld)
	{
		if (!a_world) {
			return 5;
		}
		if (a_world == a_playerWorld) {
			return 0;
		}
		const auto& main = MainWorlds();
		const auto  it = std::ranges::find(main, a_world);
		return it != main.end() ? static_cast<int>(it - main.begin()) + 1 : 4;
	}

	const RE::TESWorldSpace* PluginWorld(const RE::TESQuest& a_quest)
	{
		const auto file = a_quest.GetFile(0);
		if (!file) {
			return nullptr;
		}
		constexpr std::array<std::string_view, 3> kPlugins{ "Fallout4.esm"sv, "DLCCoast.esm"sv, "DLCNukaWorld.esm"sv };
		const auto                                name = file->GetFilename();
		for (std::size_t i = 0; i < kPlugins.size(); ++i) {
			if (name.size() == kPlugins[i].size() && _strnicmp(name.data(), kPlugins[i].data(), name.size()) == 0) {
				return MainWorlds()[i];
			}
		}
		return nullptr;
	}

	const RE::TESWorldSpace* WorldOfCell(const RE::TESObjectCELL* a_cell)
	{
		if (!a_cell || a_cell->IsInterior()) {
			return nullptr;
		}
		if (!IsPersistentCell(*a_cell)) {
			return a_cell->worldSpace;
		}
		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			return nullptr;
		}
		for (const auto world : dataHandler->GetFormArray<RE::TESWorldSpace>()) {
			if (world && world->persistentCell == a_cell) {
				return world;
			}
		}
		return nullptr;
	}

	std::optional<Distance> MeasureStraight(const RE::TESObjectREFR& a_from, const RE::TESObjectREFR& a_to)
	{
		const auto fromCell = a_from.GetParentCell();
		const auto toCell = a_to.GetParentCell();
		if (!fromCell || !toCell) {
			return std::nullopt;
		}
		const bool sameSpace = fromCell->IsInterior() ?
		                           fromCell == toCell :
		                           toCell->IsExterior() && WorldOfCell(fromCell) && WorldOfCell(fromCell) == WorldOfCell(toCell);
		if (!sameSpace) {
			return Distance{ 0.0f, DistanceKind::kOtherSpace };
		}
		return Distance{ Length(a_from.GetPosition(), a_to.GetPosition()), DistanceKind::kStraight };
	}

	Distance MeasureBetween(const std::optional<RootPoint>& a_from, const RootPoint& a_to, std::span<const WorldEntry> a_entries)
	{
		if (a_from && a_to.world && a_from->world == a_to.world) {
			return Distance{ Length(a_from->pos, a_to.pos), DistanceKind::kStraight };
		}
		std::optional<float> best;
		for (const auto& entry : a_entries) {
			if (a_to.world && RootWorld(entry.world) == a_to.world) {
				const auto length = entry.length + Length(entry.point, a_to.pos);
				if (!best || length < *best) {
					best = length;
				}
			}
		}
		return best ? Distance{ *best, DistanceKind::kViaDoors } : Distance{ 0.0f, DistanceKind::kOtherSpace };
	}

	std::vector<RE::TESQuest*> GetDisplayedQuests()
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return {};
		}
		RE::BSAutoLock lock{ player->questTargetLock };
		return DisplayedQuestsLocked(*player);
	}

	ShownObjectives GetQuestsWithObjectives()
	{
		ShownObjectives result;
		const auto      player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return result;
		}
		RE::BSAutoLock lock{ player->questTargetLock };
		for (const auto& instance : player->objectives) {
			const auto objective = Engine::AsObjective(instance.objective);
			if (objective && objective->ownerQuest) {
				result[objective->ownerQuest].push_back(objective->index);
			}
		}
		return result;
	}

	std::int32_t GetObjectiveSignature()
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return 0;
		}
		RE::BSAutoLock lock{ player->questTargetLock };
		std::uint32_t  sum = 0;
		for (const auto& instance : player->objectives) {
			const auto objective = Engine::AsObjective(instance.objective);
			if (!objective || !objective->ownerQuest || IsMarkerQuest(objective->ownerQuest) || !Engine::IsQuestTrackable(*objective->ownerQuest) ||
				instance.enstanceState.underlying() != std::to_underlying(Engine::ObjectiveState::kDisplayed)) {
				continue;
			}
			// FNV-1a of (quest, objective index, instance), summed so that list order does not matter.
			std::uint32_t hash = 2166136261u;
			for (const auto value : { objective->ownerQuest->GetFormID(), std::uint32_t{ objective->index },
					 static_cast<std::uint32_t>(instance.instanceID) }) {
				for (int shift = 0; shift < 32; shift += 8) {
					hash = (hash ^ ((value >> shift) & 0xFF)) * 16777619u;
				}
			}
			sum += hash;
		}
		return static_cast<std::int32_t>(sum);
	}

	std::vector<WorldEntry> CollectWorldEntries()
	{
		std::vector<WorldEntry> result;
		const auto              player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return result;
		}
		RE::BSAutoLock lock{ player->questTargetLock };
		for (const auto& entry : player->questTargets) {
			if (!entry.first || !entry.second) {
				continue;
			}
			ForEachRefData(*Engine::AsTargetArray(entry.second), [&](const Engine::QuestTarget&, const Engine::QuestTargetRefData& a_ref) {
				const auto& path = a_ref.path;
				if (path.sections.empty() || path.doors.empty()) {
					return;
				}
				const auto target = a_ref.target.get();
				float      total = 0.0f;
				auto       from = path.start;
				for (std::uint32_t i = 0; i < path.doors.size(); ++i) {
					const auto& hop = path.doors[i];
					if (hop.door) {
						total += Length(from, hop.door->GetPosition());
					}
					from = hop.exitPoint;
					// A hop comes out where the next door stands (the last one: where the target is).
					const RE::TESObjectREFR* next = i + 1 < path.doors.size() ? path.doors[i + 1].door : target.get();
					if (const auto world = next ? WorldOfCell(CellOf(*next)) : nullptr) {
						result.push_back(WorldEntry{ world, total, ToRoot(world, hop.exitPoint).pos });
					}
				}
			});
		}
		return result;
	}

	std::vector<Candidate> CollectQuestCandidates()
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return {};
		}
		const auto     playerRoot = PlayerRoot(*player);
		const auto     playerWorld = playerRoot ? playerRoot->world : nullptr;
		RE::BSAutoLock lock{ player->questTargetLock };
		const auto     displayed = DisplayedQuestsLocked(*player);

		std::vector<Candidate> result;
		for (const auto& entry : player->questTargets) {
			const auto quest = entry.first;
			if (!quest || !entry.second || std::ranges::find(displayed, quest) == displayed.end()) {
				continue;
			}
			std::optional<Candidate>          best;
			const Engine::QuestTargetRefData* bestRef = nullptr;
			ForEachRefData(*Engine::AsTargetArray(entry.second), [&](const Engine::QuestTarget& a_target, const Engine::QuestTargetRefData& a_ref) {
				const auto distance = MeasureRefData(*player, a_ref);
				if (distance && (!best || distance->SortKey() < best->distance.SortKey())) {
					best = Candidate{ .quest = quest, .questTarget = &a_target, .target = a_ref.target, .distance = *distance };
					bestRef = &a_ref;
				}
			});
			if (best) {
				best->world = WorldOfTarget(*quest, *bestRef, playerWorld);
				result.push_back(*best);
			}
		}
		return result;
	}

	std::vector<Candidate> CollectNoMarkerCandidates(std::span<const Candidate> a_withMarker)
	{
		std::vector<Candidate> result;
		const auto             player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return result;
		}
		for (const auto quest : GetDisplayedQuests()) {
			if (std::ranges::find(a_withMarker, quest, &Candidate::quest) != a_withMarker.end()) {
				continue;
			}
			Candidate candidate;
			candidate.quest = quest;
			candidate.noMarker = true;
			candidate.world = GuessQuestWorld(*quest, player);
			result.push_back(candidate);
		}
		return result;
	}

	namespace
	{
		[[nodiscard]] auto TrackOrderKey(const Candidate& a_candidate, const RE::TESWorldSpace* a_playerWorld)
		{
			return std::tuple{ WorldRank(a_candidate.world, a_playerWorld), a_candidate.noMarker,
				a_candidate.noMarker ? 0.0 : a_candidate.distance.SortKey() };
		}
	}

	const RE::TESWorldSpace* GetPlayerRootWorld()
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		const auto playerRoot = player ? PlayerRoot(*player) : std::nullopt;
		return playerRoot ? playerRoot->world : nullptr;
	}

	void SortByTrackOrder(std::vector<Candidate>& a_candidates)
	{
		const auto playerWorld = GetPlayerRootWorld();
		std::ranges::stable_sort(a_candidates, std::less{}, [&](const Candidate& a_candidate) {
			return TrackOrderKey(a_candidate, playerWorld);
		});
	}

	const Candidate* FindNearest(std::span<const Candidate> a_candidates, const std::function<bool(const Candidate&)>& a_isExcluded)
	{
		const auto playerWorld = GetPlayerRootWorld();
		const auto key = [&](const Candidate& a_candidate) {
			return TrackOrderKey(a_candidate, playerWorld);
		};

		logger::debug("  player world {:08X}"sv, playerWorld ? playerWorld->GetFormID() : 0u);
		const Candidate* best = nullptr;
		for (const auto& candidate : a_candidates) {
			const bool excluded = a_isExcluded(candidate);
			logger::debug("  {}: world {:08X} (rank {}), {}{}"sv, DescribeCandidate(candidate),
				candidate.world ? candidate.world->GetFormID() : 0u, WorldRank(candidate.world, playerWorld),
				candidate.noMarker ? "-"s : std::format("{} {:.0f}"sv, KindName(candidate.distance.kind), candidate.distance.value),
				excluded ? " (excluded)"sv : ""sv);
			if (!excluded && (!best || key(candidate) < key(*best))) {
				best = &candidate;
			}
		}
		return best;
	}

	std::string DescribeCandidate(const Candidate& a_candidate)
	{
		if (a_candidate.interior) {
			const auto name = a_candidate.interior->GetFullName();
			return std::format("interior {:08X} \"{}\" door {:08X}"sv, a_candidate.interior->GetFormID(), name ? name : "",
				a_candidate.location ? a_candidate.location->GetFormID() : 0u);
		}
		if (a_candidate.location) {
			return std::format("location {:08X} \"{}\""sv, a_candidate.location->GetFormID(), GetMapMarkerName(*a_candidate.location));
		}
		if (!a_candidate.quest) {
			return "?"s;
		}
		return std::format("{} {}"sv, a_candidate.start ? "start"sv : (a_candidate.noMarker ? "no-marker"sv : "quest"sv),
			DescribeQuest(*a_candidate.quest));
	}

	std::string DescribeQuest(const RE::TESQuest& a_quest)
	{
		const auto editorID = a_quest.formEditorID.c_str();
		return std::format("{:08X} {} \"{}\""sv, a_quest.GetFormID(), editorID ? editorID : "",
			RE::TESFullName::GetFullName(a_quest));
	}

	std::string_view KindName(DistanceKind a_kind) noexcept
	{
		switch (a_kind) {
		case DistanceKind::kPath:
			return "path"sv;
		case DistanceKind::kStraight:
			return "straight"sv;
		case DistanceKind::kViaDoors:
			return "via-doors"sv;
		default:
			return "other-space"sv;
		}
	}

	void DumpQuestTargets()
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			logger::info("dump: no player"sv);
			return;
		}
		RE::BSAutoLock lock{ player->questTargetLock };

		logger::info("=== dump: player {} ==="sv, DescribeRef(player));
		logger::info("objectives ({}):"sv, player->objectives.size());
		for (const auto& instance : player->objectives) {
			const auto objective = Engine::AsObjective(instance.objective);
			if (!objective) {
				continue;
			}
			logger::info("  {} obj {} instance {} state {} (own {}), targets {}: {}"sv,
				objective->ownerQuest ? DescribeQuest(*objective->ownerQuest) : "no quest"s,
				objective->index, instance.instanceID, instance.enstanceState.underlying(),
				std::to_underlying(objective->state), objective->numTargets,
				static_cast<std::string_view>(objective->displayText));
		}

		const auto displayed = DisplayedQuestsLocked(*player);
		logger::info("quest targets ({} quests, {} displayed):"sv, player->questTargets.size(), displayed.size());
		for (const auto& entry : player->questTargets) {
			const auto quest = entry.first;
			if (!quest || !entry.second) {
				continue;
			}
			const auto& array = *Engine::AsTargetArray(entry.second);
			logger::info("  {} tracked {} displayed {} flags {:04X}, targets {}"sv, DescribeQuest(*quest),
				Engine::IsQuestActive(*quest), std::ranges::find(displayed, quest) != displayed.end(),
				quest->data.flags, array.targets.size());
			ForEachRefData(array, [&](const Engine::QuestTarget& a_target, const Engine::QuestTargetRefData& a_ref) {
				const auto& path = a_ref.path;
				const auto  length = PathLength(path);
				const auto  distance = MeasureRefData(*player, a_ref);
				logger::info("    target {} ref {}"sv, static_cast<const void*>(&a_target), DescribeRef(a_ref.target.get().get()));
				logger::info("      path sections {} doors {} length {} start ({:.0f}, {:.0f}, {:.0f}) drift {:.0f} -> {} {:.0f}"sv,
					path.sections.size(), path.doors.size(), length ? std::format("{:.0f}"sv, *length) : "none"s,
					path.start.x, path.start.y, path.start.z, Length(path.start, player->GetPosition()),
					distance ? KindName(distance->kind) : "unmeasured"sv, distance ? distance->value : -1.0f);
				for (const auto& hop : path.doors) {
					logger::info("      door {} exit ({:.0f}, {:.0f}, {:.0f})"sv, DescribeRef(hop.door),
						hop.exitPoint.x, hop.exitPoint.y, hop.exitPoint.z);
				}
			});
		}
		logger::info("=== end of dump ==="sv);
	}
}
