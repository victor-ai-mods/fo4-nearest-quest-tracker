#pragma once

namespace NQT
{
	// The name the Pip-Boy shows for a quest in its list, with <Alias=...> tags resolved.
	// Miscellaneous quests have no own entry there (the Pip-Boy lists their objectives under
	// "Miscellaneous"), so for them it is the text of the displayed objective that owns the
	// quest's nearest target. Empty if the quest has no displayed objective.
	[[nodiscard]] std::string GetQuestDisplayName(const RE::TESQuest& a_quest);

	// Texts of the quest's displayed (not completed) objectives as the Pip-Boy shows them, aliases
	// resolved, in the order the engine keeps them; repeated texts once.
	[[nodiscard]] std::vector<std::string> GetObjectiveTexts(const RE::TESQuest& a_quest);

	// Name for a quest the Pip-Boy does not show and that has no name of its own (Miscellaneous quests, e.g.
	// DNMessageInABottleQuest): the text of objective a_objective (if given and it has one), else of its first
	// objective with a text, alias tags (<Alias=...>) cut out; "" if none.
	[[nodiscard]] std::string GetObjectiveName(const RE::TESQuest& a_quest, std::optional<std::uint16_t> a_objective);

	// sLanguage from the game INI ("en", "ru", ...), lower case; "en" if it is not set.
	[[nodiscard]] std::string GetGameLanguage();
}
