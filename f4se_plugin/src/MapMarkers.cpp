#include "MapMarkers.h"

#include "QuestDistance.h"

namespace NQT
{
	namespace
	{
		// The MapMarker static (Fallout4.esm 000010): TESObjectCELL::AddMapMarkersToList (OG rva 3B9210) takes
		// a ref of this base that has ExtraMapMarker.
		constexpr RE::TESFormID kMapMarkerStatic = 0x10;

		// LocTypeWorkshopSettlement (Fallout4.esm 083C9A): locations of the player's settlements.
		constexpr RE::TESFormID kSettlementKeyword = 0x083C9A;

		// MapMarkerData::GetVisible / GetTravelLoc / GetHidden (OG rva DEA40, DEA70, DEAA0): flags +0x10.
		constexpr std::uint32_t kVisible = 1u << 0;
		constexpr std::uint32_t kCanTravelTo = 1u << 1;
		constexpr std::uint32_t kHidden = 1u << 2;

		// Read raw: libxse/commonlibf4 (AE) declares the members private.
		std::uint8_t MarkerFlags(const RE::MapMarkerData& a_data) noexcept
		{
			return *(reinterpret_cast<const std::uint8_t*>(&a_data) + 0x10);
		}

		// MapMarkerData::GetMarkerType (OG rva DEAD0): u16 at +0x12, after the flag byte and the loaded copy of it.
		std::uint16_t MarkerType(const RE::MapMarkerData& a_data) noexcept
		{
			return *reinterpret_cast<const std::uint16_t*>(reinterpret_cast<const std::byte*>(&a_data) + 0x12);
		}

		const RE::MapMarkerData* GetMapMarkerData(const RE::TESObjectREFR& a_ref)
		{
			const auto extra = a_ref.extraList ? a_ref.extraList->GetByType(RE::EXTRA_DATA_TYPE::kMapMarker) : nullptr;
			return extra ? static_cast<const RE::ExtraMapMarker*>(extra)->mapMarkerData : nullptr;
		}

		MarkerState StateOf(const RE::MapMarkerData& a_data) noexcept
		{
			const auto flags = MarkerFlags(a_data);
			return (flags & kCanTravelTo) && (flags & kVisible) ? MarkerState::kVisited :
			       (flags & kVisible)                           ? MarkerState::kSeen :
			                                                      MarkerState::kUnknown;
		}

		std::string_view StateName(MarkerState a_state) noexcept
		{
			switch (a_state) {
			case MarkerState::kSeen:
				return "seen"sv;
			case MarkerState::kVisited:
				return "visited"sv;
			default:
				return "unknown"sv;
			}
		}

		bool IsSettlement(const RE::BGSLocation* a_location)
		{
			for (int depth = 0; a_location && depth < 8; ++depth, a_location = a_location->parentLoc) {
				if (a_location->HasKeywordID(kSettlementKeyword)) {
					return true;
				}
			}
			return false;
		}

		// A game setting (GMST) of the masters; a_default if missing or of another type.
		float GameSettingFloat(std::string_view a_name, float a_default)
		{
			const auto collection = RE::GameSettingCollection::GetSingleton();
			const auto setting = collection ? collection->GetSetting(a_name) : nullptr;
			if (setting && setting->GetType() == RE::Setting::SETTING_TYPE::kFloat) {
				return setting->GetFloat();
			}
			if (setting && setting->GetType() == RE::Setting::SETTING_TYPE::kInt) {
				return static_cast<float>(setting->GetInt());
			}
			logger::warn("game setting {} not found, using {}"sv, a_name, a_default);
			return a_default;
		}

		// Radius in which the player discovers a marker: ExtraRadius (XRDS, ExtraDataList::GetRadius OG rva 92210:
		// extra type 0x59, float +0x18), else iMapMarkerRevealDistance (1000 in the masters).
		float DiscoveryRadius(const RE::TESObjectREFR& a_ref)
		{
			const auto extra = a_ref.extraList ? a_ref.extraList->GetByType(RE::EXTRA_DATA_TYPE::kRadius) : nullptr;
			const auto radius = extra ? *reinterpret_cast<const float*>(reinterpret_cast<const std::byte*>(extra) + 0x18) : 0.0f;
			return radius > 0.0f ? radius : GameSettingFloat("iMapMarkerRevealDistance"sv, 1000.0f);
		}

		// Range of location markers on the compass (CalculateCompassMarkersFunctor::UpdateLocationMarkers, OG rva
		// A3C260): fSurvivalCompassLocationMaxDist when PlayerCharacter::GetDifficultyLevel() == 6, else
		// fHUDCompassLocationMaxDist (masters: 5000 / 20000).
		float CompassRange(RE::PlayerCharacter& a_player)
		{
			return a_player.GetDifficultyLevel() == RE::DIFFICULTY_LEVEL::kTrueSurvival ?
			           GameSettingFloat("fSurvivalCompassLocationMaxDist"sv, 5000.0f) :
			           GameSettingFloat("fHUDCompassLocationMaxDist"sv, 20000.0f);
		}

		float Length(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to) noexcept
		{
			const auto dx = a_to.x - a_from.x;
			const auto dy = a_to.y - a_from.y;
			const auto dz = a_to.z - a_from.z;
			return std::sqrt(dx * dx + dy * dy + dz * dz);
		}

		// TNAM icon types: 0-64 base game (xEdit wbDefinitionsFO4.pas), 65-80 DLC; the DLC names follow the
		// icon names in Fallout4.exe (MechanistMarker ... OperatorsMarker) checked against the masters:
		// Nuka-World rides 74, safari 75, monorail stations 73, Galactic Zone 70. 78-80 are the three gang
		// hideouts of Nuka-Town (Disciples / Pack / Operators, order not verified): one name for all three.
		struct TypeName
		{
			std::string_view en, ru;
		};
		constexpr std::array<TypeName, 81> kTypeNames{ {
			{ "Cave"sv, "Пещера"sv },
			{ "City"sv, "Город"sv },
			{ "Diamond City"sv, "Даймонд-сити"sv },
			{ "Encampment"sv, "Лагерь"sv },
			{ "Factory"sv, "Завод"sv },
			{ "Monument"sv, "Памятник"sv },
			{ "Metro station"sv, "Станция метро"sv },
			{ "Military base"sv, "Военная база"sv },
			{ "Landmark"sv, "Природный ориентир"sv },
			{ "Office building"sv, "Офисное здание"sv },
			{ "Town ruins"sv, "Руины городка"sv },
			{ "Urban ruins"sv, "Городские руины"sv },
			{ "Sanctuary"sv, "Сэнкчуари"sv },
			{ "Settlement"sv, "Поселение"sv },
			{ "Sewer"sv, "Канализация"sv },
			{ "Vault"sv, "Убежище"sv },
			{ "Airfield"sv, "Аэродром"sv },
			{ "Bunker Hill"sv, "Банкер-Хилл"sv },
			{ "Camper"sv, "Трейлер"sv },
			{ "Car"sv, "Машина"sv },
			{ "Church"sv, "Церковь"sv },
			{ "Country club"sv, "Загородный клуб"sv },
			{ "Custom House"sv, "Таможня"sv },
			{ "Drive-in"sv, "Автокинотеатр"sv },
			{ "Elevated highway"sv, "Эстакада"sv },
			{ "Faneuil Hall"sv, "Фанел-холл"sv },
			{ "Farm"sv, "Ферма"sv },
			{ "Filling station"sv, "Заправка"sv },
			{ "Forest"sv, "Лес"sv },
			{ "Goodneighbor"sv, "Добрососедство"sv },
			{ "Graveyard"sv, "Кладбище"sv },
			{ "Hospital"sv, "Больница"sv },
			{ "Industrial dome"sv, "Промышленный купол"sv },
			{ "Industrial stacks"sv, "Заводские трубы"sv },
			{ "Institute"sv, "Институт"sv },
			{ "Irish Pride"sv, "«Айриш Прайд»"sv },
			{ "Junkyard"sv, "Свалка"sv },
			{ "Observatory"sv, "Обсерватория"sv },
			{ "Pier"sv, "Пристань"sv },
			{ "Pond"sv, "Пруд"sv },
			{ "Quarry"sv, "Карьер"sv },
			{ "Radioactive area"sv, "Радиоактивная зона"sv },
			{ "Radio tower"sv, "Радиовышка"sv },
			{ "Salem"sv, "Салем"sv },
			{ "School"sv, "Школа"sv },
			{ "Shipwreck"sv, "Кораблекрушение"sv },
			{ "Submarine"sv, "Подлодка"sv },
			{ "Swan Pond"sv, "Лебединый пруд"sv },
			{ "Synth head"sv, "Голова синта"sv },
			{ "Town"sv, "Городок"sv },
			{ "Brotherhood of Steel"sv, "Братство Стали"sv },
			{ "Townhouse"sv, "Жилой дом"sv },
			{ "Bunker"sv, "Бункер"sv },
			{ "Castle"sv, "Замок"sv },
			{ "Skyscraper"sv, "Небоскрёб"sv },
			{ "Libertalia"sv, "Либерталия"sv },
			{ "Low-rise building"sv, "Малоэтажное здание"sv },
			{ "Minutemen"sv, "Минитмены"sv },
			{ "Police station"sv, "Полицейский участок"sv },
			{ "Prydwen"sv, "«Придвен»"sv },
			{ "Railroad"sv, "Подземка"sv },
			{ "Railway"sv, "Железная дорога"sv },
			{ "Satellite array"sv, "Спутниковая антенна"sv },
			{ "Sentinel"sv, "Ракетная площадка"sv },
			{ "USS Constitution"sv, "«Конституция»"sv },
			{ "Mechanist's lair"sv, "Логово Механиста"sv },
			{ "Raider settlement"sv, "Поселение рейдеров"sv },
			{ "Vassal settlement"sv, "Поселение-данник"sv },
			{ "Potential vassal settlement"sv, "Будущий данник"sv },
			{ "Bottling plant"sv, "Разливочный завод"sv },
			{ "Galactic Zone"sv, "Галактика"sv },
			{ "Hub"sv, "Центр парка"sv },
			{ "Kiddie Kingdom"sv, "Детское королевство"sv },
			{ "Monorail"sv, "Монорельс"sv },
			{ "Ride"sv, "Аттракцион"sv },
			{ "Safari"sv, "Сафари"sv },
			{ "Wild West"sv, "Дикий Запад"sv },
			{ "Point of interest"sv, "Достопримечательность"sv },
			{ "Gang hideout"sv, "Логово банды"sv },
			{ "Gang hideout"sv, "Логово банды"sv },
			{ "Gang hideout"sv, "Логово банды"sv },
		} };
	}

	std::vector<MapMarker> CollectMapMarkers()
	{
		std::vector<MapMarker> result;
		const auto             dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			return result;
		}

		// BGSLocation::worldLocMarker: the marker a location is shown with on the map.
		std::unordered_map<const RE::TESObjectREFR*, const RE::BGSLocation*> locations;
		for (const auto location : dataHandler->GetFormArray<RE::BGSLocation>()) {
			if (const auto marker = location ? location->worldLocMarker.get() : nullptr) {
				locations.try_emplace(marker.get(), location);
			}
		}

		for (const auto world : dataHandler->GetFormArray<RE::TESWorldSpace>()) {
			const auto cell = world ? world->persistentCell : nullptr;
			if (!cell) {
				continue;
			}
			const auto           root = RootWorld(world);
			RE::BSAutoLock       lock{ cell->spinLock };  // TESObjectCELL::CellRefLockEnter (OG rva 3C4720)
			for (const auto& ref : cell->references) {
				if (!ref) {
					continue;
				}
				const auto base = ref->GetObjectReference();
				const auto data = base && base->GetFormID() == kMapMarkerStatic ? GetMapMarkerData(*ref) : nullptr;
				if (!data) {
					continue;
				}
				MapMarker marker;
				marker.ref = ref.get();
				marker.world = world;
				marker.root = root;
				const auto it = locations.find(ref.get());
				marker.location = it != locations.end() ? it->second : nullptr;
				marker.state = StateOf(*data);
				marker.hidden = (MarkerFlags(*data) & kHidden) != 0;
				marker.disabled = ref->IsDisabled();
				marker.type = MarkerType(*data);
				result.push_back(marker);
			}
		}
		return result;
	}

	std::vector<Candidate> CollectLocationCandidates(bool a_all)
	{
		std::vector<Candidate> result;
		const auto             player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return result;
		}
		const auto playerRoot = PlayerRoot(*player);
		const auto playerWorld = playerRoot ? playerRoot->world : nullptr;
		const auto compassRange = CompassRange(*player);
		const auto entries = CollectWorldEntries();
		logger::debug("  locations (all {}): compass range {:.0f}"sv, a_all, compassRange);
		for (const auto& marker : CollectMapMarkers()) {
			if (marker.disabled || marker.hidden || marker.state == MarkerState::kVisited || IsSettlement(marker.location)) {
				continue;
			}
			const auto point = ToRoot(marker.world, marker.ref->GetPosition());
			if (!a_all && marker.state == MarkerState::kUnknown &&
				(!playerRoot || marker.root != playerWorld || Length(playerRoot->pos, point.pos) > compassRange)) {
				continue;
			}
			Candidate candidate;
			candidate.location = marker.ref;
			candidate.markerID = marker.ref->GetFormID();
			candidate.world = marker.root;
			candidate.distance = MeasureBetween(playerRoot, point, entries);
			if (candidate.distance.kind != DistanceKind::kOtherSpace) {
				candidate.distance.value = std::max(0.0f, candidate.distance.value - DiscoveryRadius(*marker.ref));
			}
			result.push_back(candidate);
		}
		return result;
	}

	MarkerState GetMapMarkerState(const RE::TESObjectREFR& a_ref)
	{
		const auto data = GetMapMarkerData(a_ref);
		return data ? StateOf(*data) : MarkerState::kUnknown;
	}

	std::string GetMapMarkerName(const RE::TESObjectREFR& a_ref)
	{
		const auto data = GetMapMarkerData(a_ref);
		const auto name = data ? data->GetFullName() : nullptr;
		return name ? std::string{ name } : std::string{};
	}

	std::string GetMapMarkerTypeName(const RE::TESObjectREFR& a_ref, std::string_view a_language)
	{
		const auto data = GetMapMarkerData(a_ref);
		if (!data) {
			return {};
		}
		const auto type = MarkerType(*data);
		if (type >= kTypeNames.size()) {
			return {};
		}
		return std::string{ a_language == "ru"sv ? kTypeNames[type].ru : kTypeNames[type].en };
	}

	void DumpMapMarkers()
	{
		const auto player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}
		const auto markers = CollectMapMarkers();
		const auto playerRoot = PlayerRoot(*player);
		const auto entries = CollectWorldEntries();
		logger::info("=== map markers: {} in all worlds; player's currentMapMarkers {} ==="sv, markers.size(),
			player->currentMapMarkers.size());

		struct Counts
		{
			std::size_t total{ 0 }, disabled{ 0 }, hidden{ 0 }, settlements{ 0 };
			std::size_t states[3]{};
		};
		std::map<std::uint32_t, Counts> byRoot;
		for (const auto& marker : markers) {
			auto& counts = byRoot[marker.root ? marker.root->GetFormID() : 0u];
			++counts.total;
			if (marker.disabled) {
				++counts.disabled;
			} else if (marker.hidden) {
				++counts.hidden;
			} else {
				++counts.states[static_cast<int>(marker.state)];
				counts.settlements += IsSettlement(marker.location);
			}
		}
		for (const auto& [world, counts] : byRoot) {
			logger::info("  root {:08X}: {} markers, {} disabled, {} hidden; enabled: unknown {}, seen {}, visited {}; {} settlements"sv,
				world, counts.total, counts.disabled, counts.hidden, counts.states[0], counts.states[1], counts.states[2],
				counts.settlements);
		}

		std::vector<std::pair<double, std::string>> lines;
		for (const auto& marker : markers) {
			const auto data = GetMapMarkerData(*marker.ref);
			const auto name = data ? data->GetFullName() : nullptr;
			const auto distance = MeasureBetween(playerRoot, ToRoot(marker.world, marker.ref->GetPosition()), entries);
			const auto locationName = marker.location ? marker.location->GetFullName() : nullptr;
			lines.emplace_back(distance.SortKey(),
				std::format("{:08X} \"{}\" {}{}{} flags {:02X} type {} world {:08X} location {:08X} \"{}\"{} -> {} {:.0f}"sv,
					marker.ref->GetFormID(), name ? name : "", StateName(marker.state), marker.hidden ? " hidden"sv : ""sv,
					marker.disabled ? " disabled"sv : ""sv, data ? MarkerFlags(*data) : 0u, marker.type,
					marker.world ? marker.world->GetFormID() : 0u, marker.location ? marker.location->GetFormID() : 0u,
					locationName ? locationName : "", IsSettlement(marker.location) ? " settlement"sv : ""sv,
					KindName(distance.kind), distance.value));
		}
		std::ranges::sort(lines);
		for (const auto& [key, line] : lines) {
			logger::info("  {}"sv, line);
		}
		logger::info("=== end of map markers ==="sv);
	}
}
