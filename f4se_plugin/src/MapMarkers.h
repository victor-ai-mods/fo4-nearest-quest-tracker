#pragma once

namespace NQT
{
	// What the player knows of a map marker: MapMarkerData::flags (+0x10) bit 0 "visible" (on the Pip-Boy
	// map), bit 1 "can travel to" (discovered: the player has been inside its radius). The condition function
	// GetMapMarkerVisible returns the same three values.
	enum class MarkerState : std::uint8_t
	{
		kUnknown = 0,  // not on the map
		kSeen = 1,     // on the map, never visited (a quest, note or holotape added it: ObjectReference.AddToMap)
		kVisited = 2,  // discovered, fast travel allowed
	};

	struct MapMarker
	{
		RE::TESObjectREFR*       ref{ nullptr };
		const RE::TESWorldSpace* world{ nullptr };  // the worldspace whose persistent cell holds it
		const RE::TESWorldSpace* root{ nullptr };   // its root (Diamond City -> Commonwealth)
		const RE::BGSLocation*   location{ nullptr };  // the location this is the world marker of; may be null
		MarkerState              state{ MarkerState::kUnknown };
		bool                     hidden{ false };    // "Show All" hidden: never on the map (Pip-Boy skips it)
		bool                     disabled{ false };  // the discovery check and the Pip-Boy skip disabled markers
		std::uint16_t            type{ 0 };          // TNAM icon type
	};

	// Every map marker of every worldspace, including other mods' (all map markers are persistent refs
	// of the MapMarker static in a world's persistent cell; TESWorldSpace::BuildMapMarkerList collects
	// PlayerCharacter::currentMapMarkers the same way, for the player's root world only).
	[[nodiscard]] std::vector<MapMarker> CollectMapMarkers();

	struct Candidate;

	// Map markers the player has not discovered, as candidates for FindNearest (QuestDistance.h). Skipped:
	// disabled, "Show All" hidden, visited, settlements (LocTypeWorkshopSettlement on the location or a parent).
	// a_all = false ("seen", no spoilers): only markers on the map, plus those not on the map that the
	// compass shows anyway (same root world, straight 3D distance within fHUDCompassLocationMaxDist,
	// fSurvivalCompassLocationMaxDist on Survival). Distance: MeasureBetween to the edge of the marker's
	// discovery radius.
	[[nodiscard]] std::vector<Candidate> CollectLocationCandidates(bool a_all);

	// Name the Pip-Boy map shows for a map marker ("" if a_ref is not one).
	[[nodiscard]] std::string GetMapMarkerName(const RE::TESObjectREFR& a_ref);

	// Name of the marker's icon type in a_language ("ru", else English), e.g. "Vault"; "" if unknown (a mod's type).
	[[nodiscard]] std::string GetMapMarkerTypeName(const RE::TESObjectREFR& a_ref, std::string_view a_language);

	// Writes all map markers with their state, location and distance to the log.
	void DumpMapMarkers();
}
