# Nearest Quest Tracker — план версии для AE (Anniversary Edition, 1.11.x)

Статус (2026-10-12): **не начат.** Версия 2.0.0 выходит только для 1.10.163 (до Next-Gen). В описании на Nexus:
версия для AE планируется, срок не обещаем. Делать — отдельными сессиями, по этапам ниже.

## Почему сейчас на AE не работает (проверено по коду 2026-10-12)

- Вся работа — в `NearestQuestTracker.dll`, собранной на alandtse/CommonLibF4: он знает только 1.10.163 (OG) и
  1.10.984 (NG), версий 1.11.x нет (`F4SE/Version.h`: `RUNTIME_LATEST = RUNTIME_1_10_984`).
- `main.cpp`: `F4SEPlugin_Query` (старый F4SE) — только 1.10.163; `F4SEPlugin_Version.CompatibleVersions` (новый F4SE) —
  только 1.10.984; `F4SEPlugin_Load` на любой версии, кроме OG, выходит, не регистрируя нативы и окно.
- Итог на AE: хоткеи ничего не делают (нативов `NQT_Native` нет), окно пишет «Нет NearestQuestTracker.dll». Падения
  не ждём, но как F4SE для AE обходится с такой DLL — не проверено (F4SE AE нет).

## Квесты Creation Club (проверено по `D:\Games\F4_AE`, AE 1.11.191, 2026-10-12)

519 файлов `cc*`, в них 236 квестов, у 66 есть цели (QOBJ) — около 30 настоящих линий (Enclave Remnants
`ccOTMFO4001`, Hellfire `ccBGSFO4044`, X-02 `ccBGSFO4115`, Tunnel Snakes `ccRZRFO4001`, Captain Cosmos `ccSWKFO4001`,
Neon Flats `ccTOSFO4002`, Vault-Tec homes `ccVLTFO4001`, собаки `ccBGSFO4050..4061` и др.). Скрипт подсчёта был в
scratchpad сессии (`tools/esm.py`, QUST с QOBJ).

| Часть мода | Квесты CC на AE после переноса |
|---|---|
| «Отметить ближайшее», квесты Pip-Boy | видны: берутся из целей игрока, плагин не важен |
| «Новые квесты» (старты) | нет: в `QuestStarts.json` ни одного CC — добавить (этап 4) |
| Окно, дерево категорий | «Другие квесты» → `other.<cc-файл>`; нормальные категории — из раздела Creation Club вики (этап 4) |

## Этапы

### AE-0. Тестовая среда (отдельная сессия)
- Отдельный портативный MO2 для `D:\Games\F4_AE` (например `D:\Games\MO2 Fallout 4 AE`), профиль с LocalSettings +
  LocalSaves (ini и сохранения — в профиле, не в `My Games`). `Fallout4.exe_` → `Fallout4.exe` (переименован при
  установке, чтобы не запустить случайно); запускать AE только через этот MO2.
- Скачать для 1.11.191: F4SE (f4se.silverlock.org), Address Library (AE), MCM (AE-сборка).
- Не изолируется MO2: логи F4SE/Papyrus (`My Games\Fallout4\F4SE`, `\Logs` — перезапишут логи OG), возможно реестр
  (`Bethesda Softworks\Fallout4\Installed Path`). Перед первым запуском — копии `My Games\Fallout4`,
  `AppData\Local\Fallout4`, ключа реестра; после — сравнить.
- Игру запускает пользователь; сам я в игре не проверяю.

### AE-1. Сборка DLL под AE
- CommonLibF4 с поддержкой 1.11.x (форк alandtse её не имеет): выбрать форк/версию, где есть AE и Address Library AE;
  либо одна DLL на OG + AE, либо две сборки.
- `main.cpp`: версии в Query / Version, убрать выход на не-OG.

### AE-2. Проверка структур движка на AE (дизассемблер AE `Fallout4.exe_` 1.11.191)
Всё, что DLL читает сама (раскладки из `EngineTypes.h`, смещения в `PLAN-1.0.md` / `PLAN.md` «Решения и находки»):
`PlayerCharacter::objectives` / `questTargets` / `questTargetLock`, `TeleportPath` и `ComputeLength`,
`BGSQuestObjective`, `TESQuest` (data +0xF0, stages, aliasedHandles), `TESQuestStage` (+0x18/+0x1A),
`MapMarkerData` (flags, тип +0x12), `ExtraSeenData` / IntSeenData (туман войны), `TESWorldSpace` (persistentCell,
offset ONAM), `PlayerCharacterQuestEvent::Notify`, `BGSQuestInstanceText::ParseString`, `GetLock`, `IsCrimeToActivate`,
`SettingCollectionList::GetPtr` (sLanguage), Scaleform-интерфейс F4SE. Для каждой функции — ID Address Library AE.
Нет PDB для AE: сверять по сигнатурам и коду с OG (PDB 1.10.163 есть, `tools\fo4_pdb.py`).

### AE-3. Тест в игре (пользователь)
Минимальный тест каждой функции (как в PLAN.md): хоткей «Отметить ближайшее» (квест, старт, локация, интерьер),
окно (обе вкладки, действия), предмет в избранном, «Показать текущий квест».

### AE-4. Квесты Creation Club
- Категории: раздел Creation Club на вики (ru/en) → `tools/gen_quest_categories.py`, плагины `cc*.esl`.
- Старты: квестодатели CC-квестов → `tools/quest_starts.txt` (большинство стартует от записки / радио / места).

### AE-5. Релиз
Отдельный файл на Nexus («AE version») или общий архив; README — какие версии поддерживаются.
