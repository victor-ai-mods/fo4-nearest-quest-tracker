#pragma once

#include "QuestDistance.h"

namespace NQT
{
	// A place where a quest not started yet can be picked up (QuestStarts.json, stage 3a). A quest may
	// have several (other givers, several radio transmitters, one per objective).
	struct QuestStart
	{
		RE::TESQuest*                           quest{ nullptr };
		std::uint32_t                           refID{ 0 };  // quest giver; a temporary ref is in memory only while its cell is loaded
		std::vector<std::vector<RE::TESQuest*>> after;       // each group: one of its quests must be completed first
		std::optional<std::uint16_t>            objective;   // started once this objective has been shown (default: any)
		std::optional<std::uint16_t>            stage;       // started once this stage is done (quests without objectives)
		RE::TESWorldSpace*                      world{ nullptr };  // static point for an unloaded giver (null: none)
		RE::NiPoint3               pos;
		std::uint32_t              markerID{ 0 };  // persistent ref in the giver's own cell/world (door out of it)
		std::uint32_t              anchorID{ 0 };  // persistent ref at pos (outer door or map marker)
		float                      radioRadius{ 0.0f };  // > 0: the ref is a radio transmitter heard within this radius
		std::string                name;           // for the log
	};

	// Reads Data\F4SE\Plugins\NearestQuestTracker\QuestStarts.json and QuestStarts-user.json.
	// Entries whose plugins are not loaded are skipped. Call once game data is loaded.
	void LoadQuestStarts();

	// The first start point of a_quest; nullptr if it has none.
	[[nodiscard]] const QuestStart* FindQuestStart(const RE::TESQuest& a_quest);

	// The nearest start point of a_quest that is available now; nullptr if none is.
	[[nodiscard]] const QuestStart* FindAvailableStart(const RE::TESQuest& a_quest);

	// Start points that can be used now: quest not completed, not started yet (see QuestStart::objective
	// and stage), every `after` group satisfied, the giver not dead or disabled (when it is in memory).
	[[nodiscard]] std::vector<Candidate> CollectStartCandidates();

	// Where the marker for a_start goes right now: the giver itself when it is in memory, else
	// the door of its cell, else the outer anchor. 0 if the start is no longer available.
	[[nodiscard]] std::uint32_t GetStartMarkerID(const QuestStart& a_start);

	// Quest name for notifications: the FULL name with <...> tags removed.
	[[nodiscard]] std::string GetStartName(const RE::TESQuest& a_quest);

	// Station name of a radio start: the transmitter's display name (the radio quest's alias names it,
	// as in the Pip-Boy radio list). Empty if the transmitter is not in memory or has no name.
	[[nodiscard]] std::string GetStartRadioName(const QuestStart& a_start);

	void DumpQuestStarts();
}
