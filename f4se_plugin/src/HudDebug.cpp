#include "HudDebug.h"

namespace NQT
{
	namespace
	{
		using GFxValue = RE::Scaleform::GFx::Value;
		using ValueType = GFxValue::ValueType;

		std::string Describe(const GFxValue& a_value)
		{
			switch (a_value.GetType()) {
			case ValueType::kUndefined:
				return "undefined";
			case ValueType::kNull:
				return "null";
			case ValueType::kBoolean:
				return a_value.GetBoolean() ? "true" : "false";
			case ValueType::kInt:
				return std::to_string(a_value.GetInt());
			case ValueType::kUInt:
				return std::to_string(a_value.GetUInt());
			case ValueType::kNumber:
				return std::format("{:.1f}", a_value.GetNumber());
			case ValueType::kString:
				return std::format("'{}'", a_value.GetString());
			default:
				return a_value.IsObject() ? "object" : "?";
			}
		}

		std::string Member(const GFxValue& a_object, const char* a_name)
		{
			GFxValue value;
			if (!a_object.IsObject() || !a_object.GetMember(a_name, &value)) {
				return "-";
			}
			return Describe(value);
		}

		void Dump(const std::string& a_tag)
		{
			const auto ui = RE::UI::GetSingleton();
			const auto menu = ui ? ui->GetMenu("HUDMenu"sv) : nullptr;
			if (!menu || !menu->uiMovie || !menu->uiMovie->asMovieRoot) {
				logger::info("HUD [{}]: no HUDMenu movie"sv, a_tag);
				return;
			}
			GFxValue messages;
			if (!menu->uiMovie->asMovieRoot->GetVariable(&messages, "root.HUDNotificationsGroup_mc.Messages_mc") ||
				!messages.IsObject()) {
				logger::info("HUD [{}]: no Messages_mc"sv, a_tag);
				return;
			}
			GFxValue queued;
			GFxValue shown;
			messages.GetMember("MessageArray", &queued);
			messages.GetMember("ShownMessageArray", &shown);
			GFxValue group;
			menu->uiMovie->asMovieRoot->GetVariable(&group, "root.HUDNotificationsGroup_mc");
			logger::info("HUD [{}]: dialogue {}, queued {}, shown {}, CanAddMessage {}, Messages visible {} alpha {} y {}"
						 " height {}; group visible {} alpha {}"sv,
				a_tag, ui->GetMenuOpen("DialogueMenu"sv),Member(queued, "length"), Member(shown, "length"), Member(messages, "CanAddMessage"),
				Member(messages, "visible"), Member(messages, "alpha"), Member(messages, "y"), Member(messages, "height"),
				Member(group, "visible"), Member(group, "alpha"));
			GFxValue count;
			messages.GetMember("numChildren", &count);
			const std::int32_t children = count.IsInt() ? count.GetInt() : count.IsUInt() ? static_cast<std::int32_t>(count.GetUInt()) :
										  count.IsNumber()                              ? static_cast<std::int32_t>(count.GetNumber()) :
																						  0;
			for (std::int32_t i = 0; i < children && i < 16; ++i) {
				GFxValue child;
				const std::array<GFxValue, 1> args{ GFxValue(i) };
				if (!messages.Invoke("getChildAt", &child, args) || !child.IsObject()) {
					continue;
				}
				GFxValue data;
				child.GetMember("data", &data);
				logger::info("HUD [{}]   child {}: name {} y {} height {} visible {} alpha {} frame {} fadeIn {} fullyIn {}"
							 " fadeOut {} dirty {} text {}"sv,
					a_tag, i, Member(child, "name"), Member(child, "y"), Member(child, "height"), Member(child, "visible"),
					Member(child, "alpha"), Member(child, "currentFrame"), Member(child, "fadeInStarted"),
					Member(child, "fullyFadedIn"), Member(child, "fadeOutStarted"), Member(child, "bIsDirty"),
					Member(data, "messageText"));
			}
		}
	}

	void DumpHudMessages(std::string a_tag)
	{
		const auto tasks = F4SE::GetTaskInterface();
		if (!tasks) {
			logger::info("HUD [{}]: no task interface"sv, a_tag);
			return;
		}
		tasks->AddUITask([tag = std::move(a_tag)]() { Dump(tag); });
	}
}
