#include "QuestCategories.h"

#include "QuestDistance.h"
#include "QuestStarts.h"

#include <nlohmann/json.hpp>

namespace NQT
{
	namespace
	{
		constexpr auto kDataDir = "Data/F4SE/Plugins/NearestQuestTracker"sv;

		std::vector<QuestCategory>                            categories;
		std::unordered_map<const RE::TESQuest*, std::string> questCategories;

		[[nodiscard]] bool HasCategory(std::string_view a_key)
		{
			return std::ranges::find(categories, a_key, &QuestCategory::key) != categories.end();
		}

		// New categories of the user file go after the last category under the same parent (its whole subtree).
		void AddCategory(const nlohmann::json& a_entry, std::string_view a_file)
		{
			if (!a_entry.is_object()) {
				return;
			}
			QuestCategory category{ a_entry.value("key", ""s), a_entry.value("ru", ""s), a_entry.value("en", ""s) };
			if (category.key.empty() || category.key == kOtherCategory || category.key.starts_with("other."sv)) {
				logger::warn("{}: category without a key or under \"other\" skipped"sv, a_file);
				return;
			}
			if (category.ru.empty()) {
				category.ru = category.en;
			}
			if (category.en.empty()) {
				category.en = category.ru;
			}
			if (const auto old = std::ranges::find(categories, category.key, &QuestCategory::key); old != categories.end()) {
				*old = std::move(category);  // renames a built-in one
				return;
			}
			const auto dot = category.key.rfind('.');
			if (dot == std::string::npos) {
				categories.push_back(std::move(category));
				return;
			}
			const auto parent = std::string_view{ category.key }.substr(0, dot);
			if (!HasCategory(parent)) {
				logger::warn("{}: category {} skipped (no parent {})"sv, a_file, category.key, parent);
				return;
			}
			const auto prefix = std::string{ parent } + '.';
			auto       pos = std::ranges::find(categories, parent, &QuestCategory::key) + 1;
			while (pos != categories.end() && pos->key.starts_with(prefix)) {
				++pos;
			}
			categories.insert(pos, std::move(category));
		}

		// Reads one file; its quests replace what earlier files gave them. Returns quests read.
		std::size_t LoadFile(const std::filesystem::path& a_path)
		{
			std::ifstream file{ a_path };
			if (!file) {
				return 0;
			}
			const auto doc = nlohmann::json::parse(file, nullptr, false, true);
			if (doc.is_discarded() || !doc.is_object()) {
				logger::warn("{}: not valid JSON; file ignored"sv, a_path.string());
				return 0;
			}
			const auto fileName = a_path.filename().string();
			if (const auto list = doc.find("categories"); list != doc.end() && list->is_array()) {
				for (const auto& entry : *list) {
					AddCategory(entry, fileName);
				}
			}
			std::size_t read = 0, notLoaded = 0;
			if (const auto list = doc.find("quests"); list != doc.end() && list->is_array()) {
				for (const auto& entry : *list) {
					const auto key = entry.is_object() ? entry.value("quest", ""s) : ""s;
					const auto category = entry.is_object() ? entry.value("category", ""s) : ""s;
					if (!HasCategory(category)) {
						logger::warn("{}: {} skipped (unknown category \"{}\")"sv, fileName, key, category);
						continue;
					}
					const auto id = ResolveID(key);
					const auto quest = id ? RE::TESForm::GetFormByID<RE::TESQuest>(id) : nullptr;
					if (!quest) {
						++notLoaded;
						continue;
					}
					questCategories[quest] = category;
					++read;
				}
			}
			logger::info("{}: {} quests in {} categories ({} skipped: plugin not loaded or not a quest)"sv,
				fileName, read, categories.size(), notLoaded);
			return read;
		}
	}

	void LoadQuestCategories()
	{
		categories.clear();
		questCategories.clear();
		const std::filesystem::path dir{ kDataDir };
		if (LoadFile(dir / "QuestCategories.json") == 0) {
			logger::warn("QuestCategories.json: no quest categories loaded"sv);
		}
		LoadFile(dir / "QuestCategories-user.json");
	}

	const std::vector<QuestCategory>& GetQuestCategories()
	{
		return categories;
	}

	std::vector<RE::TESQuest*> GetCategorizedQuests()
	{
		std::vector<RE::TESQuest*> result;
		result.reserve(questCategories.size());
		for (const auto& [quest, key] : questCategories) {
			result.push_back(const_cast<RE::TESQuest*>(quest));
		}
		return result;
	}

	std::string GetQuestCategory(const RE::TESQuest& a_quest)
	{
		if (const auto it = questCategories.find(&a_quest); it != questCategories.end()) {
			return it->second;
		}
		const auto file = a_quest.GetFile(0);
		return std::string{ kOtherCategory } + '.' + std::string{ file ? file->GetFilename() : "?"sv };
	}

	std::string GetQuestCategoryName(std::string_view a_key, std::string_view a_language)
	{
		const auto ru = a_language == "ru"sv;
		if (a_key == kOtherCategory) {
			return ru ? "Другие квесты"s : "Other quests"s;
		}
		if (a_key.starts_with("other."sv)) {
			return std::string{ a_key.substr(6) };
		}
		const auto it = std::ranges::find(categories, a_key, &QuestCategory::key);
		return it == categories.end() ? std::string{} : ru ? it->ru : it->en;
	}

	void DumpQuestCategories()
	{
		const auto quests = GetDisplayedQuests();
		logger::info("quest categories: {} categories, {} quests in files; {} quests in the Pip-Boy:"sv,
			categories.size(), questCategories.size(), quests.size());
		for (const auto quest : quests) {
			const auto key = GetQuestCategory(*quest);
			logger::info("  {:08X} {} -> {} ({})"sv, quest->GetFormID(), quest->GetFormEditorID(), key,
				GetQuestCategoryName(key, "en"sv));
		}
	}
}
