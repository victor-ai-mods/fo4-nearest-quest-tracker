#pragma once

namespace NQT
{
	namespace Engine
	{
		class QuestTarget;
	}

	struct QuestStart;

	enum class DistanceKind : std::uint8_t
	{
		kPath,        // engine path through doors (the one the compass marker follows)
		kStraight,    // no path yet: straight line inside the same worldspace / interior cell
		kViaDoors,    // start point in another worldspace: engine path to a door into it + straight line on
		              // (see WorldEntry)
		kOtherSpace,  // target is in another worldspace or interior and has no path
	};

	struct Distance
	{
		float        value{ 0.0f };  // game units; 0 for kOtherSpace
		DistanceKind kind{ DistanceKind::kPath };

		// Anything reachable beats any target in another space.
		[[nodiscard]] double SortKey() const noexcept
		{
			constexpr double kOtherSpacePenalty = 1.0e12;
			return kind == DistanceKind::kOtherSpace ? kOtherSpacePenalty + value : value;
		}
	};

	// One place the player can be sent to: the nearest target of a Pip-Boy quest, the start point of a
	// quest not started yet (QuestStarts.h), or a Pip-Boy quest without any marker (only its world is known).
	struct Candidate
	{
		RE::TESQuest*              quest{ nullptr };
		const Engine::QuestTarget* questTarget{ nullptr };  // Pip-Boy quest: quest target the nearest ref belongs to
		RE::ObjectRefHandle        target;
		Distance                   distance;
		const QuestStart*          start{ nullptr };        // start point: its entry; target is not used
		std::uint32_t              markerID{ 0 };           // start point: FormID of the ref to put the marker on
		const RE::TESWorldSpace*   world{ nullptr };        // root worldspace of the place; null if not known
		bool                       noMarker{ false };       // Pip-Boy quest without a marker: distance is not used
	};

	// A point in one of the root worldspaces (Commonwealth, Far Harbor, Nuka-World, ...).
	struct RootPoint
	{
		const RE::TESWorldSpace* world{ nullptr };
		RE::NiPoint3             pos;
	};

	// Child worldspaces that take land, LOD or map from their parent (Diamond City, Goodneighbor)
	// share its coordinates: the parent is their root.
	[[nodiscard]] const RE::TESWorldSpace* RootWorld(const RE::TESWorldSpace* a_world);

	// Where a ref is on the world map. Interior: the world marker of the cell's location (or of a
	// parent location), as MapMarker::GetPlayerMapPosition does for the player (OG rva AF8BB0).
	[[nodiscard]] std::optional<RootPoint> RootOf(const RE::TESObjectREFR& a_ref);

	// The player's point: in an interior without a location marker, the engine's
	// PlayerCharacter::exteriorPosition (the map's fallback too).
	[[nodiscard]] std::optional<RootPoint> PlayerRoot(const RE::PlayerCharacter& a_player);

	// Root world of a quest by its plugin: Far Harbor for DLCCoast.esm, Nuka-World for DLCNukaWorld.esm,
	// the Commonwealth for Fallout4.esm; null for other plugins.
	[[nodiscard]] const RE::TESWorldSpace* PluginWorld(const RE::TESQuest& a_quest);

	// The quest NearestQuestTracker.esp shows start markers with: never a candidate itself.
	void SetMarkerQuest(const RE::TESQuest* a_quest) noexcept;

	// A way into a worldspace taken from the engine's paths to Pip-Boy quest targets: a path that goes
	// through doors comes out in `world` at `point` after `length` units from the player. Start points in
	// other worldspaces are measured through these (the engine builds no path to them).
	struct WorldEntry
	{
		const RE::TESWorldSpace* world{ nullptr };
		float                    length{ 0.0f };
		RE::NiPoint3             point;
	};

	[[nodiscard]] std::vector<WorldEntry> CollectWorldEntries();

	// Worldspace of an exterior cell (not its root); null for an interior. Persistent refs of a world that
	// is not loaded sit in its persistent cell (TESWorldSpace::persistentCell, FormID 0, created by
	// TESWorldSpace::CreatePersistentCell); its worldSpace field is not trusted (2026-10-01 log: from Nuka-World,
	// unloaded Commonwealth refs came out in Nuka-World), the owner is looked up by the cell pointer.
	[[nodiscard]] const RE::TESWorldSpace* WorldOfCell(const RE::TESObjectCELL* a_cell);

	// Straight-line fallback for a ref without an engine path; nullopt if either ref has no cell.
	[[nodiscard]] std::optional<Distance> MeasureStraight(const RE::TESObjectREFR& a_from, const RE::TESObjectREFR& a_to);

	// Quests in the Pip-Boy list: at least one objective displayed and not completed.
	[[nodiscard]] std::vector<RE::TESQuest*> GetDisplayedQuests();

	// Quests with any objective in the player's list, whatever its state (shown once at least), with
	// the indices of those objectives.
	using ShownObjectives = std::unordered_map<const RE::TESQuest*, std::vector<std::uint16_t>>;
	[[nodiscard]] ShownObjectives GetQuestsWithObjectives();

	// Order-independent hash of the displayed (active, not completed) objectives: changes when an
	// objective is completed or failed, or a new one is displayed (a quest started or advanced).
	[[nodiscard]] std::int32_t GetObjectiveSignature();

	// The nearest target of every displayed quest that has at least one target with a position.
	[[nodiscard]] std::vector<Candidate> CollectQuestCandidates();

	// Displayed quests that have no candidate in a_withMarker (their objectives have no target, or no
	// target with a position): the world is guessed from the refs and locations the quest's aliases hold,
	// else from its start point (QuestStarts.json), else from its plugin.
	[[nodiscard]] std::vector<Candidate> CollectNoMarkerCandidates(std::span<const Candidate> a_withMarker);

	// The candidate tracked first among those whose quest is not excluded; nullptr if none. Order (user's
	// choice 2026-10-01): by world - the player's root world, then the Commonwealth, Far Harbor, Nuka-World,
	// other worlds, unknown; within a world candidates with a marker by distance, then those without one.
	[[nodiscard]] const Candidate* FindNearest(std::span<const Candidate> a_candidates, const std::function<bool(const RE::TESQuest&)>& a_isExcluded);

	[[nodiscard]] std::string      DescribeQuest(const RE::TESQuest& a_quest);
	[[nodiscard]] std::string_view KindName(DistanceKind a_kind) noexcept;

	// Writes every quest target the player has, with objective states and distances, to the log.
	void DumpQuestTargets();
}
