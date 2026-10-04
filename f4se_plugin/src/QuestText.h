#pragma once

namespace NQT
{
	// The name the Pip-Boy shows for a quest in its list, with <Alias=...> tags resolved.
	// Miscellaneous quests have no own entry there (the Pip-Boy lists their objectives under
	// "Miscellaneous"), so for them it is the text of the displayed objective that owns the
	// quest's nearest target. Empty if the quest has no displayed objective.
	[[nodiscard]] std::string GetQuestDisplayName(const RE::TESQuest& a_quest);

	// sLanguage from the game INI ("en", "ru", ...), lower case; "en" if it is not set.
	[[nodiscard]] std::string GetGameLanguage();
}
