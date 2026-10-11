#pragma once

namespace NQT
{
	// A category of the quest tree (QuestCategories.json, stage 2.0-2): wiki sections like "faction.bos.side".
	// The key's parts are the path from the top level; the parent of "a.b.c" is "a.b".
	struct QuestCategory
	{
		std::string key;
		std::string ru;
		std::string en;
	};

	// Quests the wiki tables do not list (other mods, Creation Club, a few vanilla ones) go here, one
	// subcategory per plugin: "other.<plugin file name>". Always last in the tree.
	inline constexpr auto kOtherCategory = "other"sv;

	// Reads Data\F4SE\Plugins\NearestQuestTracker\QuestCategories.json and QuestCategories-user.json.
	// Quests whose plugins are not loaded are skipped. Call once game data is loaded.
	void LoadQuestCategories();

	// Categories in tree order (a parent before its children), without "other".
	[[nodiscard]] const std::vector<QuestCategory>& GetQuestCategories();

	// The quest's category key; "other.<plugin>" if the files give it none.
	[[nodiscard]] std::string GetQuestCategory(const RE::TESQuest& a_quest);

	// Name of a category in a_language ("ru" -> Russian, anything else -> English); for "other.<plugin>"
	// the plugin file name. Empty if the key is unknown.
	[[nodiscard]] std::string GetQuestCategoryName(std::string_view a_key, std::string_view a_language);

	// Debug: the category of every quest the Pip-Boy shows, to the log.
	void DumpQuestCategories();
}
