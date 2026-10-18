#pragma once

// Quest-target structures that CommonLibF4 only forward-declares. Layouts were read from
// the Fallout4.exe 1.10.163 disassembly (constructors, BGSQuestObjective::Load/SetState,
// TeleportPath::ComputeLength, TESQuestTarget::REF_DATA::GetReference); only the members
// this plugin reads are named. AE (1.11.191) code reads them at the same offsets; NG (1.10.984) code of these
// functions is the AE code (tools/fo4_ng.py cmp, 2026-10-18: ComputeLength, IsStageDone, PipboyQuestData vtable).
namespace NQT::Engine
{
	// QUEST_OBJECTIVE_STATE values (BGSQuestObjective::SetState: 1 -> OnObjectiveDisplayed,
	// 3 -> DisplayObjectiveComplete, 5 -> DisplayObjectiveFailed).
	enum class ObjectiveState : std::uint8_t
	{
		kDormant = 0,
		kDisplayed = 1,
		kCompleted = 2,
		kCompletedDisplayed = 3,
		kFailed = 4,
		kFailedDisplayed = 5,
	};

	// Door hop of a path: the door the path goes through and the point it teleports to.
	struct TeleportDoor
	{
		RE::TESObjectREFR* door;      // 00
		RE::NiPoint3       exitPoint; // 08
	};
	static_assert(sizeof(TeleportDoor) == 0x18);

	class TeleportPath
	{
	public:
		RE::BSTArray<void*>        sections;  // 00 - empty = no path (ComputeLength returns -1)
		RE::BSTArray<TeleportDoor> doors;     // 18
		RE::NiPoint3               start;     // 30 - player position when the path was built
		RE::NiPoint3               end;       // 3C - target position
		bool                       unk48;     // 48
	};
	static_assert(offsetof(TeleportPath, doors) == 0x18);
	static_assert(offsetof(TeleportPath, start) == 0x30);
	static_assert(offsetof(TeleportPath, end) == 0x3C);
	static_assert(sizeof(TeleportPath) == 0x50);

	// TESQuestTarget::REF_DATA: one resolved target reference with its path from the player.
	class QuestTargetRefData
	{
	public:
		TeleportPath        path;    // 00
		RE::ObjectRefHandle target;  // 50
	};
	static_assert(offsetof(QuestTargetRefData, target) == 0x50);

	class QuestTarget
	{
	public:
		std::uint8_t                       flags;       // 00
		std::uint8_t                       pad01[7];    // 01
		RE::TESCondition                   conditions;  // 08
		std::uint64_t                      unk10;       // 10
		std::uint64_t                      unk18;       // 18
		RE::BSTArray<QuestTargetRefData*>  refs;        // 20
	};
	static_assert(offsetof(QuestTarget, conditions) == 0x08);
	static_assert(offsetof(QuestTarget, refs) == 0x20);

	// Value type of PlayerCharacter::questTargets: targets of the quest's displayed objectives.
	class QuestTargetArray
	{
	public:
		RE::BSTArray<QuestTarget*> targets;          // 00
		std::uint64_t              nextRepathTime;   // 18 - ms; repath every uQuestTargetRepathInterval (5 s)
	};
	static_assert(offsetof(QuestTargetArray, nextRepathTime) == 0x18);

	class QuestObjective
	{
	public:
		RE::BGSLocalizedString displayText;  // 00
		RE::TESQuest*          ownerQuest;   // 08
		QuestTarget**          targets;      // 10
		std::uint32_t          numTargets;   // 18
		std::uint16_t          index;        // 1C
		bool                   initialized;  // 1E
		ObjectiveState         state;        // 1F
		std::uint32_t          flags;        // 20
	};
	static_assert(offsetof(QuestObjective, ownerQuest) == 0x08);
	static_assert(offsetof(QuestObjective, index) == 0x1C);
	static_assert(offsetof(QuestObjective, state) == 0x1F);

	// Element of TESQuest::aliasedHandles (+0xB8), parallel to TESQuest::aliases: the refs an alias holds,
	// a BSTSmallArray<ObjectRefHandle, 1>. Read as TESQuest::DoGetAliasedRef (OG rva 5F3840) does, under
	// aliasAccessLock (TESQuest::GetAliasedRef takes it for read).
	struct AliasHandles
	{
		std::uint32_t capacity;  // 00 - bit 31: the one handle is stored inline at +08
		std::uint32_t pad04;     // 04
		union
		{
			std::uint32_t        inlineHandle;
			const std::uint32_t* heap;
		};                       // 08
		std::uint32_t size;      // 10
		std::uint32_t pad14;     // 14

		[[nodiscard]] std::span<const RE::ObjectRefHandle> Handles() const noexcept
		{
			static_assert(sizeof(RE::ObjectRefHandle) == sizeof(std::uint32_t));
			const auto data = (capacity & 0x80000000) ? &inlineHandle : heap;
			return { reinterpret_cast<const RE::ObjectRefHandle*>(data), size };
		}
	};
	static_assert(offsetof(AliasHandles, size) == 0x10);
	static_assert(sizeof(AliasHandles) == 0x18);

	struct AliasHandlesArray
	{
		const AliasHandles* data;      // 00
		std::uint32_t       capacity;  // 08
		std::uint32_t       pad0C;     // 0C
		std::uint32_t       size;      // 10
	};

	[[nodiscard]] inline const AliasHandlesArray& GetAliasHandles(const RE::TESQuest& a_quest) noexcept
	{
		static_assert(offsetof(RE::TESQuest, aliasedHandles) == 0xB8);
		return *reinterpret_cast<const AliasHandlesArray*>(&a_quest.aliasedHandles);
	}

	// What the Pip-Boy's track button requires (QuestMenuUtils::ToggleQuestActiveStatus): the quest is
	// running (data.flags bit 0) and not completed (bit 1). A completed quest may keep an objective in the
	// displayed state (2026-10-01: DLC04MS02 "Start the ride"); tracking it does nothing.
	[[nodiscard]] inline bool IsQuestTrackable(const RE::TESQuest& a_quest) noexcept
	{
		return (a_quest.data.flags & 0x3) == 0x1;
	}

	// TESQuest::GetActive (id 690102): the Pip-Boy "tracked" flag, data.flags bit 11.
	[[nodiscard]] inline bool IsQuestActive(const RE::TESQuest& a_quest) noexcept
	{
		return (a_quest.data.flags >> 11) & 1;
	}

	// Quest type "Miscellaneous" (QUEST_DATA::questType, TESQuest+0xF7): the Pip-Boy lists the
	// objectives of such quests under one "Miscellaneous" entry (PipboyQuestData::Populate).
	[[nodiscard]] inline bool IsMiscQuest(const RE::TESQuest& a_quest) noexcept
	{
		constexpr std::int8_t kMisc = 6;
		return a_quest.data.questType == kMisc;
	}

	[[nodiscard]] inline const QuestTargetArray* AsTargetArray(const RE::QuestTargetArray* a_array) noexcept
	{
		return reinterpret_cast<const QuestTargetArray*>(a_array);
	}

	[[nodiscard]] inline const QuestObjective* AsObjective(const RE::BGSQuestObjective* a_objective) noexcept
	{
		return reinterpret_cast<const QuestObjective*>(a_objective);
	}
}
