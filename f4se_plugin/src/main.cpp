#include "Version.h"

#include "Papyrus.h"
#include "QuestCategories.h"
#include "QuestDistance.h"
#include "QuestStarts.h"
#include "QuestText.h"

namespace
{
	void OnF4SEMessage(F4SE::MessagingInterface::Message* a_msg)
	{
		if (a_msg->type == F4SE::MessagingInterface::kGameDataReady) {
			// NQT_StartMarkerQuest, NQT_LocationMarkerQuest (tools/gen_esp.py): their objectives are never candidates.
			const auto dataHandler = RE::TESDataHandler::GetSingleton();
			const auto startQuest = dataHandler ? dataHandler->LookupForm<RE::TESQuest>(0x805, "NearestQuestTracker.esp"sv) : nullptr;
			const auto locationQuest = dataHandler ? dataHandler->LookupForm<RE::TESQuest>(0x809, "NearestQuestTracker.esp"sv) : nullptr;
			NQT::SetMarkerQuests(startQuest, locationQuest);
			logger::info("marker quests: start {:08X}, location {:08X}"sv, startQuest ? startQuest->GetFormID() : 0u,
				locationQuest ? locationQuest->GetFormID() : 0u);
			NQT::LoadQuestStarts();
			NQT::LoadQuestCategories();

			const auto player = RE::PlayerCharacter::GetSingleton();
			logger::info("game data ready; player {:p}, objectives {}, language {}"sv,
				static_cast<void*>(player), player ? player->objectives.size() : 0u, NQT::GetGameLanguage());
		}
	}

	bool RegisterPapyrus(RE::BSScript::IVirtualMachine* a_vm)
	{
		return NQT::Papyrus::Register(a_vm);
	}
}

extern "C" DLLEXPORT constinit auto F4SEPlugin_Version = []() noexcept {
	F4SE::PluginVersionData data{};
	data.PluginName(Version::NAME);
	data.PluginVersion({ Version::MAJOR, Version::MINOR, Version::PATCH, 0 });
	data.AuthorName("victor-ai-mods"sv);
	data.UsesAddressLibrary(true);
	data.UsesSigScanning(false);
	data.IsLayoutDependent(true);
	data.HasNoStructUse(false);
	data.CompatibleVersions({ F4SE::RUNTIME_1_10_984 });
	return data;
}();

// Old-gen F4SE (0.6.x, game 1.10.163) calls Query; next-gen F4SE reads F4SEPlugin_Version.
extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = Version::NAME.data();
	a_info->version = Version::MAJOR;

	if (a_f4se->IsEditor()) {
		return false;
	}
	return a_f4se->RuntimeVersion() == F4SE::RUNTIME_1_10_163;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se);
	logger::info("game runtime {}, F4SE {}"sv, a_f4se->RuntimeVersion().string(), a_f4se->F4SEVersion().string());

	// Quest-target layouts (EngineTypes.h) are verified on 1.10.163 only; NG is stage 3c.
	if (REL::Module::IsNG()) {
		logger::warn("next-gen runtime is not supported yet; native functions not registered"sv);
		return true;
	}

	if (const auto messaging = F4SE::GetMessagingInterface()) {
		messaging->RegisterListener(OnF4SEMessage);
	}
	if (const auto papyrus = F4SE::GetPapyrusInterface()) {
		papyrus->Register(RegisterPapyrus);
	}
	return true;
}
