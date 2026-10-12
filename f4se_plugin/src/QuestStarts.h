#pragma once

#include "QuestDistance.h"

namespace NQT
{
	// QuestStart::closed: the start is closed while the faction is hostile to the player or the global is not 0.
	struct StartClosure
	{
		RE::TESFaction*      faction{ nullptr };
		RE::TESGlobal*       global{ nullptr };
		std::string          ru;  // names the faction in the window / notifications
		std::string          en;
	};

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
		bool                       running{ false };  // the giver offers the quest only while it is running (see NotOffered)
		std::vector<StartClosure>  closed;          // any of them holds -> the start is closed (faction kicked the player out)
		bool                       radiant{ false };  // repeatable: offered while a line in `infos` would be said (see Offered)
		std::vector<std::uint32_t> infos;           // radiant: the giver's lines offering the quest (TESTopicInfo FormIDs)
		std::uint32_t              giverBaseID{ 0 };  // the giver's base NPC: hostility and radiant name while the ref is not loaded
		std::vector<RE::TESQuest*> pointers;        // misc quests pointing at the giver ("Talk to Abbot at the wall")
		std::string                name;           // for the log
	};

	// "Plugin.esp|ID" (data files) -> loaded FormID; 0 if the plugin is not loaded or the text is malformed.
	[[nodiscard]] std::uint32_t ResolveID(std::string_view a_key);

	// Reads Data\F4SE\Plugins\NearestQuestTracker\QuestStarts.json and QuestStarts-user.json.
	// Entries whose plugins are not loaded are skipped. Call once game data is loaded.
	void LoadQuestStarts();

	// The first start point of a_quest; nullptr if it has none.
	[[nodiscard]] const QuestStart* FindQuestStart(const RE::TESQuest& a_quest);

	// Quests that have at least one start point, each once.
	[[nodiscard]] std::vector<RE::TESQuest*> GetQuestsWithStarts();

	// True if a_quest is completed (TESQuest data flags bit 1).
	[[nodiscard]] bool IsQuestCompleted(const RE::TESQuest& a_quest) noexcept;

	// The nearest start point of a_quest that is available now; nullptr if none is.
	[[nodiscard]] const QuestStart* FindAvailableStart(const RE::TESQuest& a_quest);

	// True if a_quest has no available start point because a `running` start's quest is not running (the Story
	// Manager did not start it, e.g. a required alias did not fill) and either a place it needs is held by another
	// running quest (FindStartBlocker; then wherever the player is), or the player is at the giver - kept while the
	// player stays in that location, leaving it gives the quest a new chance.
	[[nodiscard]] bool IsStartNotOffered(const RE::TESQuest& a_quest);

	// True if a_quest has no available start point and one of its givers is hostile to the player (IsGiverHostile).
	[[nodiscard]] bool IsStartGiverHostile(const RE::TESQuest& a_quest);

	// Why a quest cannot be started: every start point it has is closed (QuestStart::closed) - the closure that
	// holds for its first start; nullptr if a start is not closed (or the quest is started or completed).
	[[nodiscard]] const StartClosure* FindStartClosure(const RE::TESQuest& a_quest);

	// Why a quest may not start: a location one of its required aliases must hold (specific location, no
	// "Allow Reserved") is held by a reserving location alias of another running quest - the usual case
	// (Painting the Town vs Quartermastery at Hardware Town). nullopt if no such quest is found.
	struct StartBlocker
	{
		const RE::BGSLocation* location{ nullptr };
		RE::TESQuest*          holder{ nullptr };
	};
	[[nodiscard]] std::optional<StartBlocker> FindStartBlocker(const RE::TESQuest& a_quest);

	// Start points that can be used now: quest not completed, not started yet (see QuestStart::objective
	// and stage), every `after` group satisfied, the giver not dead or disabled (when it is in memory),
	// the giver not refusing it (IsStartNotOffered).
	[[nodiscard]] std::vector<Candidate> CollectStartCandidates(bool a_normal, bool a_radiant);

	// True if a_quest is a pointer quest (QuestStart::pointers) of a quest that cannot start now: its starts are
	// blocked (a needed place held by another quest, hostile faction or giver) - the pointer leads nowhere.
	[[nodiscard]] bool IsPointerBlocked(const RE::TESQuest& a_quest);

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
