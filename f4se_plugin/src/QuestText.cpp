#include "QuestText.h"

#include "EngineTypes.h"
#include "QuestDistance.h"

namespace NQT
{
	namespace
	{
		// BSStringT<char>::Set (CommonLibF4 has no OG id for it).
		void SetString(RE::BSStringT<char>& a_string, const char* a_text)
		{
			using func_t = bool(RE::BSStringT<char>*, const char*, std::uint64_t);
			static REL::Relocation<func_t*> func{ REL::RelocationID(407804, 2189084) };
			func(&a_string, a_text, 0);
		}

		// The same text path the Pip-Boy takes: PipboyQuestData::GenerateStandardQuest (quest
		// name) and BGSInstancedQuestObjective::GetDisplayText (objective text) both run the raw
		// text through BGSQuestInstanceText::ParseString with the objective's instance ID.
		std::string ParseText(const char* a_text, const RE::TESQuest& a_quest, std::uint32_t a_instanceID)
		{
			RE::BSStringT<char> text;
			SetString(text, a_text ? a_text : "");
			RE::BGSQuestInstanceText::ParseString(&text, &a_quest, a_instanceID);
			return std::string{ static_cast<std::string_view>(text) };
		}

		// BGSLocalizedString::QString: text without the "<ID=XXXXXXXX>" prefix of loaded strings.
		const char* LocalizedText(const RE::BGSLocalizedString& a_string)
		{
			constexpr std::size_t kPrefixLength = 13;
			const std::string_view text{ a_string };
			return text.starts_with("<ID="sv) && text.length() >= kPrefixLength ? text.data() + kPrefixLength : a_string.c_str();
		}

		struct ShownObjective
		{
			const Engine::QuestObjective* objective{ nullptr };
			std::uint32_t                 instanceID{ 0 };
		};

		// Displayed objectives of a_quest, and among them the one that owns a_target (if any).
		std::vector<ShownObjective> DisplayedObjectives(const RE::TESQuest& a_quest, const Engine::QuestTarget* a_target, ShownObjective& a_owner)
		{
			std::vector<ShownObjective> result;
			const auto                  player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return result;
			}
			RE::BSAutoLock lock{ player->questTargetLock };
			for (const auto& instance : player->objectives) {
				const auto objective = Engine::AsObjective(instance.objective);
				if (!objective || objective->ownerQuest != &a_quest ||
					instance.enstanceState.underlying() != std::to_underlying(Engine::ObjectiveState::kDisplayed)) {
					continue;
				}
				const ShownObjective shown{ objective, instance.instanceID };
				result.push_back(shown);
				if (a_target && !a_owner.objective && objective->targets &&
					std::ranges::find(std::span{ objective->targets, objective->numTargets }, a_target) !=
						std::span{ objective->targets, objective->numTargets }.end()) {
					a_owner = shown;
				}
			}
			return result;
		}
	}

	std::string GetQuestDisplayName(const RE::TESQuest& a_quest)
	{
		const Engine::QuestTarget* nearestTarget = nullptr;
		if (Engine::IsMiscQuest(a_quest)) {
			for (const auto& candidate : CollectQuestCandidates()) {
				if (candidate.quest == &a_quest) {
					nearestTarget = candidate.questTarget;
					break;
				}
			}
		}

		ShownObjective owner;
		const auto     shown = DisplayedObjectives(a_quest, nearestTarget, owner);
		if (shown.empty()) {
			logger::debug("display name of {}: no displayed objective"sv, DescribeQuest(a_quest));
			return {};
		}

		std::string name;
		if (Engine::IsMiscQuest(a_quest)) {
			const auto& objective = owner.objective ? owner : shown.front();
			name = ParseText(LocalizedText(objective.objective->displayText), a_quest, objective.instanceID);
			logger::debug("display name of {}: misc, objective {}{} -> \"{}\""sv, DescribeQuest(a_quest),
				objective.objective->index, owner.objective ? "" : " (nearest target not matched)", name);
		} else {
			const auto fullName = RE::TESFullName::GetFullName(a_quest);
			name = ParseText(std::string{ fullName }.c_str(), a_quest, shown.front().instanceID);
			logger::debug("display name of {}: \"{}\""sv, DescribeQuest(a_quest), name);
		}
		return name;
	}

	std::vector<std::string> GetObjectiveTexts(const RE::TESQuest& a_quest)
	{
		ShownObjective           owner;
		std::vector<std::string> result;
		for (const auto& shown : DisplayedObjectives(a_quest, nullptr, owner)) {
			auto text = ParseText(LocalizedText(shown.objective->displayText), a_quest, shown.instanceID);
			if (!text.empty() && std::ranges::find(result, text) == result.end()) {
				result.push_back(std::move(text));
			}
		}
		logger::debug("objective texts of {}: {}"sv, DescribeQuest(a_quest), result.size());
		return result;
	}

	std::string GetGameLanguage()
	{
		// sLanguage:General is a static SettingT<INISettingCollection> (OG id 1527977) registered
		// in INISettingCollection. In game (2026-09-30) CommonLibF4's INISettingCollection::GetSetting
		// did not find it (reason unknown), the engine's own SettingCollectionList::GetPtr (OG id
		// 504957, _stricmp) did. GetPtr has no NG id in CommonLibF4, so NG keeps the CommonLibF4
		// lookup until stage 3c.
		constexpr auto kKey = "sLanguage:General";
		const auto     collection = RE::INISettingCollection::GetSingleton();
		RE::Setting*   setting = nullptr;
		if (collection) {
			setting = REL::Module::IsNG() ? collection->GetSetting(kKey) : collection->GetPtr(kKey);
		}

		std::string language;
		if (setting && setting->GetType() == RE::Setting::SETTING_TYPE::kString) {
			language = setting->GetString();
		}
		std::ranges::transform(language, language.begin(), [](char a_ch) {
			return static_cast<char>(std::tolower(static_cast<unsigned char>(a_ch)));
		});
		return language.empty() ? "en"s : language;
	}
}
