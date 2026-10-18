#include "Interiors.h"

#include <bit>
#include <numeric>
#include <set>

#include "QuestDistance.h"

namespace NQT
{
	namespace
	{
		// SeenData (ctor OG rva D56B70, 0x28 bytes): vtable + 16x16 bits, bit index y * 16 + x
		// (SeenData::IsSeenBitTrue, OG rva D56BC0). IntSeenData (ctor OG rva D57110, 0x38 bytes) adds the section
		// coordinates ((pos - 0x800) >> 12, TESObjectCELL::UpdateSeenData OG rva 3C13F0) and the next section.
		struct IntSeenData
		{
			void*         vtable;    // 00
			std::uint32_t bits[8];   // 08
			std::int8_t   x;         // 28
			std::int8_t   y;         // 29
			std::uint8_t  pad2A[6];  // 2A
			IntSeenData*  next;      // 30
		};
		static_assert(offsetof(IntSeenData, x) == 0x28);
		static_assert(offsetof(IntSeenData, next) == 0x30);
		static_assert(sizeof(IntSeenData) == 0x38);

		// REFR_LOCK (REFR_LOCK::GetLevel / SetLocked, OG rva B7DB0 / B7E10): numeric level u8 at +0x00 (0-100, or
		// 251 barred, 252 chained, 253 requires terminal, 254 inaccessible, 255 requires key - xEdit XLOC), flags
		// u8 at +0x10: bit 0 locked, bit 2 leveled.
		constexpr std::uint8_t kLockInaccessible = 254;
		constexpr std::uint8_t kLockRequiresKey = 255;
		constexpr std::uint8_t kLockLocked = 1u << 0;

		// TESForm::formFlags of a REFR: 0x100 "Inaccessible" (xEdit: the door leads nowhere usable).
		constexpr std::uint32_t kRefInaccessible = 0x100;

		// CTEST "Quick test cell" (Fallout4.esm 0242A2): four doors in Boston lead into it.
		constexpr RE::TESFormID kTestCell = 0x0242A2;

		// Bethesda's editor IDs of abandoned cells start with a lower-case "z": zInstituteRotunda (seven enabled doors
		// around the C.I.T. ruins, enable parent CITPreBoomMarker), zWaystation, zVitalePumphouse01.
		bool IsAbandonedCell(const RE::TESObjectCELL& a_cell)
		{
			const auto editorID = a_cell.GetFormEditorID();
			return editorID && editorID[0] == 'z';
		}

		// Creation Club interiors that Fallout4.esm itself carries as stubs, with enabled unlocked world doors into them
		// (2026-10-18, NG: door 0024A1B1 in Goodneighbor -> ccTOSFO4002Interior01 "Quick Test Cell", the Neon Flats
		// entrance; also Shroud Manor ccZSEFO4002Interior01 and three Vault-Tec homes ccxxxFO4VLTInterior01). Their CC
		// plugins (cctosfo4002_neonflats.esm, cczsefo4002-smanor.esm, ccvltfo4001-homes.esm) override the cell, so a cc*
		// cell is real only when one of its files is a cc plugin. TESForm::GetFile clamps the index, hence the array.
		bool IsCreationClubStub(const RE::TESObjectCELL& a_cell)
		{
			const auto editorID = a_cell.GetFormEditorID();
			if (!editorID || _strnicmp(editorID, "cc", 2) != 0) {
				return false;
			}
			if (const auto files = a_cell.sourceFiles.array) {
				for (const auto file : *files) {
					const auto name = file ? file->GetFilename() : ""sv;
					if (name.size() >= 2 && _strnicmp(name.data(), "cc", 2) == 0) {
						return false;
					}
				}
			}
			return true;
		}

		// ExtraDataList::GetSeenData (OG rva 94600): extra type 5, pointer at +0x18.
		const IntSeenData* GetSeenData(const RE::TESObjectCELL& a_cell)
		{
			const auto list = a_cell.extraList.get();
			const auto extra = list ? list->GetByType(RE::EXTRA_DATA_TYPE::kSeenData) : nullptr;
			return extra ? *reinterpret_cast<IntSeenData* const*>(reinterpret_cast<const std::byte*>(extra) + 0x18) : nullptr;
		}

		// The cell a ref is in; for a persistent ref whose cell is not loaded, ExtraPersistentCell (see QuestDistance.cpp).
		// All 522 doors from the worldspaces into interiors find their cell this way (2026-10-04 dump).
		const RE::TESObjectCELL* CellOf(const RE::TESObjectREFR& a_ref)
		{
			const auto cell = a_ref.GetParentCell();
			return cell ? cell : a_ref.GetSaveParentCell();
		}

		std::string CellLabel(const RE::TESObjectCELL& a_cell)
		{
			const auto editorID = a_cell.GetFormEditorID();
			const auto name = a_cell.GetFullName();
			return std::format("{:08X} {} \"{}\""sv, a_cell.GetFormID(), editorID ? editorID : "", name ? name : "");
		}

		// Why a door into an interior is not offered; empty if it is. a_skipOwnedLocked (MCM "Skip locked doors of others"):
		// also a locked door that is a crime to pick, as the red "Pick" prompt shows it: TESObjectREFR::IsCrimeToActivate
		// (OG rva 410140) - not the player's door (IsAnOwner: owner NPC, or a faction the player is not in) and no key
		// for it in the player's inventory; an unowned door into a private owned cell (TESObjectCELL::GetOwner, cell not
		// public) counts too.
		// The lock: TESObjectREFR::GetLock (OG rva 47FEE0) - the door's own, else the one of the door it is linked to. Most
		// locks and owners of houses sit on the inside door (2026-10-06: Cooke's house, outside door 00001E95 without
		// XLOC, inside door 0000222C lock 50, key, owner DmndColonialTaphouseOwnerFaction), so the outside door's own
		// ExtraLock missed them.
		std::string_view SkipReason(RE::TESObjectREFR& a_door, const RE::TESObjectCELL& a_interior, bool a_skipOwnedLocked)
		{
			if (a_door.IsDisabled()) {
				return "disabled"sv;
			}
			if (a_door.formFlags & kRefInaccessible) {
				return "inaccessible"sv;
			}
			const auto lock = a_door.GetLock();
			if (lock && (lock->flags & kLockLocked) && (lock->baseLevel == kLockRequiresKey || lock->baseLevel == kLockInaccessible)) {
				return lock->baseLevel == kLockRequiresKey ? "requires key"sv : "lock inaccessible"sv;
			}
			if (a_skipOwnedLocked && lock && (lock->flags & kLockLocked) && a_door.IsCrimeToActivate()) {
				return "locked, crime to pick"sv;
			}
			if (a_interior.GetFormID() == kTestCell) {
				return "test cell"sv;
			}
			if (IsCreationClubStub(a_interior)) {
				return "creation club stub"sv;
			}
			if (IsAbandonedCell(a_interior)) {
				return "abandoned cell"sv;
			}
			return {};
		}

		struct WorldDoor
		{
			RE::TESObjectREFR*       door;
			const RE::TESObjectCELL* interior;
			RootPoint                point;  // the door on the world map (ToRoot)
		};

		// Every door of every worldspace that leads into an interior.
		std::vector<WorldDoor> CollectWorldDoors()
		{
			std::vector<WorldDoor> result;
			const auto             dataHandler = RE::TESDataHandler::GetSingleton();
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
					const auto interior = ref ? GetDoorInterior(*ref) : nullptr;
					if (interior) {
						result.push_back({ ref.get(), interior, ToRoot(world, ref->GetPosition()) });
					}
				}
			}
			return result;
		}
	}

	InteriorSeen GetInteriorSeen(const RE::TESObjectCELL& a_cell)
	{
		InteriorSeen result;
		if (!a_cell.IsInterior()) {
			return result;
		}
		constexpr std::uint32_t kMaxSections = 4096;  // guard against a broken chain
		for (auto section = GetSeenData(a_cell); section && result.sections < kMaxSections; section = section->next) {
			++result.sections;
			for (const auto word : section->bits) {
				result.bits += static_cast<std::uint32_t>(std::popcount(word));
			}
		}
		return result;
	}

	const RE::TESObjectCELL* GetDoorInterior(const RE::TESObjectREFR& a_door)
	{
		const auto extra = a_door.extraList ? a_door.extraList->GetByType<RE::ExtraTeleport>() : nullptr;
		const auto data = extra ? extra->teleportData : nullptr;
		const auto linked = data ? data->linkedDoor.get() : nullptr;
		const auto cell = linked ? CellOf(*linked) : nullptr;
		return cell && cell->IsInterior() ? cell : nullptr;
	}

	bool IsDoorInteriorVisited(const RE::TESObjectREFR& a_door)
	{
		const auto interior = GetDoorInterior(a_door);
		return !interior || GetInteriorSeen(*interior).bits > 0;
	}

	std::string GetDoorInteriorName(const RE::TESObjectREFR& a_door)
	{
		const auto interior = GetDoorInterior(a_door);
		const auto name = interior ? interior->GetFullName() : nullptr;
		return name ? std::string{ name } : std::string{};
	}

	std::vector<Candidate> CollectInteriorCandidates(bool a_skipOwnedLocked)
	{
		std::vector<Candidate> result;
		const auto             player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return result;
		}
		const auto playerRoot = PlayerRoot(*player);
		const auto entries = CollectWorldEntries();
		std::unordered_map<const RE::TESObjectCELL*, std::size_t> byInterior;  // index in result
		std::unordered_map<const RE::TESObjectCELL*, bool>        visited;
		std::size_t                                               doors = 0, skipped = 0;
		for (const auto& door : CollectWorldDoors()) {
			++doors;
			auto [it, added] = visited.try_emplace(door.interior, false);
			if (added) {
				it->second = GetInteriorSeen(*door.interior).bits > 0;
			}
			if (it->second) {
				continue;
			}
			if (!SkipReason(*door.door, *door.interior, a_skipOwnedLocked).empty()) {
				++skipped;
				continue;
			}
			Candidate candidate;
			candidate.location = door.door;
			candidate.interior = door.interior;
			candidate.markerID = door.door->GetFormID();
			candidate.world = door.point.world;
			candidate.distance = MeasureBetween(playerRoot, door.point, entries);
			const auto [known, first] = byInterior.try_emplace(door.interior, result.size());
			if (first) {
				result.push_back(candidate);
			} else if (candidate.distance.SortKey() < result[known->second].distance.SortKey()) {
				result[known->second] = candidate;
			}
		}
		logger::debug("  interiors: {} doors into {} interiors, {} visited, {} doors skipped -> {} candidates"sv, doors,
			visited.size(), std::ranges::count_if(visited, [](const auto& a_pair) { return a_pair.second; }), skipped,
			result.size());
		return result;
	}

	void DumpInteriors()
	{
		const auto dataHandler = RE::TESDataHandler::GetSingleton();
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!dataHandler || !player) {
			return;
		}

		std::size_t interiors = 0, seen = 0, emptySeen = 0;
		std::vector<std::string> seenLines;
		for (const auto cell : dataHandler->interiorCells) {
			if (!cell || !cell->IsInterior()) {
				continue;
			}
			++interiors;
			const auto info = GetInteriorSeen(*cell);
			if (info.sections == 0) {
				continue;
			}
			++seen;
			emptySeen += info.bits == 0;
			std::string sections;
			for (auto section = GetSeenData(*cell); section; section = section->next) {
				sections += std::format(" ({},{}){}"sv, section->x, section->y,
					std::accumulate(std::begin(section->bits), std::end(section->bits), 0,
						[](int a_sum, std::uint32_t a_word) { return a_sum + std::popcount(a_word); }));
			}
			seenLines.push_back(std::format("{} sections {} bits {}:{}"sv, CellLabel(*cell), info.sections, info.bits, sections));
		}
		const auto playerCell = player->GetParentCell();
		logger::info("=== interiors: {} cells, {} with seen data ({} with no bits); player in {} ==="sv, interiors, seen,
			emptySeen, playerCell ? CellLabel(*playerCell) : "none"s);

		const auto doors = CollectWorldDoors();
		std::set<const RE::TESObjectCELL*> reachable;
		std::size_t                        reachableSeen = 0;
		std::map<std::string_view, std::size_t> skipped;
		for (const auto& door : doors) {
			if (reachable.insert(door.interior).second) {
				reachableSeen += GetInteriorSeen(*door.interior).bits > 0;
			}
			const auto reason = SkipReason(*door.door, *door.interior, true);
			if (!reason.empty()) {
				++skipped[reason];
			}
		}
		logger::info("  doors from worldspaces into interiors: {}; {} interiors, {} of them seen"sv, doors.size(),
			reachable.size(), reachableSeen);
		for (const auto& [reason, count] : skipped) {
			logger::info("  skipped doors, {}: {}"sv, reason, count);
		}

		const auto playerRoot = PlayerRoot(*player);
		const auto entries = CollectWorldEntries();
		std::vector<std::pair<double, std::string>> lines;
		for (const auto& door : doors) {
			const auto distance = MeasureBetween(playerRoot, door.point, entries);
			const auto info = GetInteriorSeen(*door.interior);
			const auto reason = SkipReason(*door.door, *door.interior, true);
			lines.emplace_back(distance.SortKey(), std::format("door {:08X} -> {} sections {} bits {}{}{} -> {} {:.0f}"sv,
													   door.door->GetFormID(), CellLabel(*door.interior), info.sections, info.bits,
													   reason.empty() ? ""sv : ", skipped: "sv, reason, KindName(distance.kind),
													   distance.value));
		}
		std::ranges::sort(lines);
		constexpr std::size_t kNearest = 60;
		logger::info("  nearest {} doors:"sv, std::min(kNearest, lines.size()));
		for (std::size_t i = 0; i < lines.size() && i < kNearest; ++i) {
			logger::info("    {}"sv, lines[i].second);
		}
		logger::info("  skipped doors:"sv);
		for (const auto& [key, line] : lines) {
			if (line.contains(", skipped: "sv)) {
				logger::info("    {}"sv, line);
			}
		}

		logger::info("  interiors with seen data:"sv);
		std::ranges::sort(seenLines);
		for (const auto& line : seenLines) {
			logger::info("    {}"sv, line);
		}
		logger::info("=== end of interiors ==="sv);
	}
}
