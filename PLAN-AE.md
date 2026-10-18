# Nearest Quest Tracker — план версии для AE (Anniversary Edition, 1.11.x)

Статус (2026-10-18): **AE-0..AE-5 сделаны: 2.2.0 с AE-файлом выпущен на GitHub и Nexus.** Дальше — NG. AE-DLL собрана
(Debug) и разложена на стенд: MO2 `D:\Games\MO2 Fallout 4 AE`, профиль «Nearest Quest Tracker», мод «Nearest Quest
Tracker (AE dev)». Подробности — раздел «AE-1/AE-2: сделано» ниже. NG — отдельно, когда будет стенд NG (пользователь
качает игру NG).

## AE-3: первый запуск на стенде (2026-10-18)

DLL загрузилась через MO2 (usvfs: путь к Address Library нашёлся), лог без [W]/[E]. Работают: F1 «Отметить ближайшее»
(квест FFGoodneighbor04), F4 «Текущий квест», окно (367 квестов за ~230 мс, 230 локаций, дерево категорий, расстояния).
Ошибка: мир Содружества в окне — «LOOKUP FAILED!». Причина (проверено по файлам AE): WRLD 0x3C переопределяют все 4 DLC
и ~50 cc-плагинов с FULL = строковый ID Fallout4.esm 0003613F; последний — `ccVLTFO4001-Homes.esm` (последний в
`Fallout4.ccc`), а в его `ccVLTFO4001-Homes_en.STRINGS` такого ID нет (в `Fallout4_en.STRINGS` и `DLCCoast_en.STRINGS`
есть). Баг игры; в NQT — `WorldName()` в `Window.cpp`: при «LOOKUP FAILED!» для 0x3C — «Commonwealth»/«Содружество»,
иначе editor ID (bb899ec) — проверено в игре: «Commonwealth». Скрипты проверки: `tools/ae_wrld_overrides.py <Data>` (кто переопределяет WRLD 0x3C и его FULL),
`tools/ae_strings_lookup.py <ba2> <конец имени .STRINGS> <hex id>` (есть ли строка в таблице внутри BA2 v1/v7/v8).
Повторный запуск 2026-10-18: пользователь проверил всё — F1, F4, окно (обе вкладки, отметка кликом), Pip-Boy-текст
цели старта, предмет в избранном. **AE-3 пройден.** Дальше — AE-4 (квесты Creation Club).

## AE-5: релиз 2.2.0 (2026-10-18, ВЫПУЩЕН)

Решения пользователя: 2.2.0 сразу для OG и AE (на Nexus два файла), NG — позже; DIAG HUD убрана из релиза (745d269).
Address Library «All In One» 1.11.240 (Nexus 47327 file 408268) распакована в `_research\address-library-aio`: все 7
наших AE-адресов и все 1092 ID libxse `IDs.h` есть в базах 1.11.137 / 159 / 169 / 191 / 221 / 240 (rva сдвигаются на
десятки байт — те же функции). Там же база NG 1.10.984 — для NG-версии.
`python tools/package.py 2.2.0 [--ae]` → `release/NearestQuestTracker[-AE]-2.2.0.zip`. Release-DLL разложены: OG — в
основную игру, AE — на стенд. Проверено пользователем в игре (Release-сборки): основная игра OG и AE-стенд — всё работает.
Выпущено 2026-10-18: GitHub — 358a465 + тег v2.2.0, описание репозитория; Nexus 110040 — файл «Nearest Quest Tracker
2.2.0» (fileId 416782, update 2.1.0, старый в архиве, Primary) и новый «Nearest Quest Tracker AE 2.2.0» (fileId 416783,
Main, не Primary), описания файлов через перехват `edit-file`, версия мода 2.2.0, changelog 2.2.0 (из формы загрузки
сохранился), Full description = `release/nexus-description.bbcode.txt` (12464 симв., сверено по хэшам строк).
Дальше: версия для NG (стенд NG — когда докачается; база NG 1.10.984 уже в `_researchddress-library-aio`).

## AE-4: квесты Creation Club (2026-10-18, сделано)

**Проверено в игре 2026-10-18:** раздел Creation Club 0/65, CC-старты с расстояниями, «Dog Rescue – Find the …»
различаются; отметки Neon Winter (интерьер, Добрососедство), Dog Rescue (Dalmatian, снаружи), A Home for the Family
(записка в интерьере Boston Bugle) — маркер есть, Pip-Boy в порядке. Один раз (сеанс 14:44) после отметки Pip-Boy
открылся пустым (только размытие и нижняя панель, без корпуса): в том сеансе игра при старте спросила «включить
Creations», пользователь согласился, данные загрузились 3 раза (индекс NQT FE0AC→FE0AE; AE создала
`%LOCALAPPDATA%\Fallout4\UserDownloadedContent.txt` — OG его не читает, строки нет в exe 1.10.163). В следующем сеансе
(1 загрузка данных) те же отметки — Pip-Boy нормальный; не воспроизводится → скорее последствие перезагрузки данных AE.

**Старты — сделано** (84ea537 + 0fd8de7): 51 строка в `tools/quest_starts.txt`
(раздел Creation Club; 182 точки всего). Ref = цель первой задачи квеста с маркером (принудительный ref алиаса:
записка/голодиск/терминал/контейнер/NPC; для 4 без принудительного — найден по ячейке в плагине); отчёт —
`tools/cc_starts_research.py` → `_research\nqt-quests\cc\cc_starts_report.txt`. DLL менять не пришлось: старт «взят»,
только если квест завершён / показана цель / пройдена stage=N — «запущен без целей» (перенесённые сохранения) = можно
взять. `gen_quest_starts.py` по умолчанию на AE-индексе (ванильные 131 строка — те же байты). Окно: одноимённые квесты
(12 «Dog Rescue») получают « – <первая цель>», только если цели их различают (8 «Rogue Robot» — нет).
Не взяты: продолжения (их выдаёт предыдущий квест) и The Endless War.

**Категории — сделано** (5ed5a49, на стенде, в игре не проверено): `esm_index.py --ae` (мастера + 173 плагина из
`Fallout4.ccc` → `_research\nqt-quests\esm_index_ae.pickle`), `ba2_strings.py` (таблицы строк плагинов из BA2: у cc-
плагинов есть и `_en`, и `_ru`), `gen_quest_categories.py` → «Дополнения / Creation Club» (`dlc.cc`): 65 квестов
cc-плагинов с целями (из 66; `ccOTMFO4001_VB` — служебный). Вики: ru-страницы про CC нет; en «Creation Club#Quests» —
46 названий, 43 совпали с FULL квестов AE 1.11.191; «Home Away from Home» / «Legendary Hunt» — `vcHGS001FO4_*`
(Verified Creation `vchgs001fo4_ncrbeasthunter.esm`, не из `Fallout4.ccc`) — у игрока попадут в «Другие квесты».
Окно скрывает категории с 0 квестов → на OG без CC раздела не видно.

**Как стартуют CC-квесты (для стартов, не закончено):**
- 54 квеста (почти все основные) несут скрипт `CreationClub:StartAfterCharGenScript` (исходник — AE CK
  `Data\Scripts\Source\CreationClub\CreationClub.zip`, распакован в `_research\ae-scripts\CreationClub`): OnInit →
  если MQ102 стадия 10 (выход из Убежища 111) пройдена — `SetStage(MyStageToSet)`, иначе ждёт её. Флаг Start Game
  Enabled — только у 7 (BestOfThree, Nuka MainQuest, EndlessRaidManager, OutcastQuest + OptionalVoice, ScavengerTwo,
  RingQuest). Кто стартует остальные — не найдено: ссылок на квест нет ни в его плагине (кроме Hellfire: SeedQuest),
  ни в Fallout4.esm, ни в .pex (поиск по всем BA2).
- Сохранение стенда (OG-сейв 90 ур., загружен в AE с CC): квесты CC **работают** (рантайм-флаги 0x8101 = Enabled +
  RunOnce + HasDialogueData; BFG fe01381e, Hellfire fe01486c, 12 собак fe03x803 …), но **ни одной пройденной стадии и
  ни одной показанной цели** — в Pip-Boy их нет (DumpQuests.java, JDK 11:
  `C:\Program Files\Microsoft\jdk-11.0.16.101-hotspot`).
- Вики: в новой игре многие CC-квесты сами дают цель после выхода из Убежища («Deathmatch … will automatically appear»,
  «A Place to Call Home: begins the next time a save is loaded»). Выжимка вики (given by / location / начало
  прохождения): `_research\nqt-quests\cc\cc_infobox_en.txt`.

## AE-1/AE-2: сделано (2026-10-18)

**Сборка.** Один исходник, две DLL: `cmake --preset vs` (OG+NG, alandtse) и `cmake --preset ae` (`NQT_AE=1`,
libxse/commonlibf4 — подмодуль `f4se_plugin/extern/commonlibf4-ae`; её xmake не нужен: commonlib-shared собирается
своим CMake, сама commonlibf4 — glob `src/**.cpp` в нашем `CMakeLists.txt`; spdlog из vcpkg с фичей `wchar`).
`python tools/build.py --ae` / `python tools/deploy.py --ae` (в MO2-мод стенда, основную игру не трогает).
Различия API — в `PCH.h` (логгер через `REX::Impl::Log`, `RE::Scaleform` = `::Scaleform`, `DIFFICULTY_LEVEL`) и
точечные `#ifdef NQT_AE` (`GetAliasedRef` возвращает handle, `BSStringT::Set` есть в libxse, `sLanguage` — свой
поиск без учёта регистра по `INISettingCollection::settings`). `MapMarkerData` в libxse с private-полями → флаги
читаются сырым байтом +0x10 в обеих сборках. AE-DLL без `F4SEPlugin_Query` (старый F4SE её не загрузит); флаги
`UsesAddressLibrary` + `IsLayoutDependent` = биты 1.11.137 → F4SE AE грузит её на любой 1.11.x без списка версий
(проверено по исходнику ianpatt/f4se `PluginManager.cpp`, `CheckPluginCompatibility`).

**Свои вызовы движка — AE-ID** (`NQT::GameID(og, ae)` в `PCH.h`), найдены дизассемблером AE 1.11.191:

| Функция | OG id | AE id (rva) | Как найдена |
|---|---|---|---|
| PlayerCharacterQuestEvent::Notify | 395737 | 2232882 (D52320) | vtable PipboyQuestData → Populate → RegisterSink → QEventSource → её вызывающие |
| TESQuest::GetAliasedLoc | 1552479 | 2207812 (671D60) | строка `LocAliasIsLocation >>` → функция условия |
| TESFaction::GetPCAnEnemy | 586385 | 2207180 (6470C0) | таблица команд `GetPCEnemyofFaction` → функция условия |
| TESReactionForm::GetFactionCombatRelation | 167267 | 2193377 (31EA80) | таблица команд `GetFactionCombatReaction` |
| TESConditionItem::IsTrue(ConditionCheckParams&) | 43452 | 2212009 (769DA0) | libxse `IsTrue2` (IsTrue1 = обёртка (REFR,REFR)) |
| Actor::CalculateFactionFightReaction | 339469 | 2230091 (C8DFF0) | общий вызывающий GetPCAnEnemy×2 + GetFactionCombatRelation, тот же порядок вызовов |
| BGSLocAlias::IsLocAlias | 78630 | нет (встроена) | на AE — сравнение vtable с `VTABLE::BGSLocAlias` (RTTI проверен) |

**Раскладки, проверенные по коду AE:** TeleportPath (ComputeLength AE 7A4F80: +0x10/+0x18/+0x28, двери по 0x18,
start +0x30, end +0x3C, позиция двери +0xD0); PlayerCharacter::objectives +0x7D8 (PipboyQuestData::Populate);
TESQuest: aliasAccessLock +0xD0, карта алиас→локация +0x90..+0xB0, questType +0xF7, флаги +0xF4 (бит 11 —
GetActive), stages +0x100 / номер стадии +0x18 / «выполнена» бит 0 +0x1A (IsStageDone AE 66B3C0); TESFaction:
флаги +0x58, +0xFC, TESReactionForm +0x30; IntSeenData (ctor AE C44440: биты +0x08..+0x27, x/y +0x28/+0x29, next
+0x30); ExtraSeenData (тип 5, данные +0x18), ExtraRadius (тип 0x59, float +0x18); MapMarkerData флаги +0x10 бит 0
(натив `IsMapMarkerVisible`); слоты vtable TESObjectREFR 0x8C Get3D, 0xBE GetSaveParentCell, 0xC0 IsDead.
По заголовкам: 390 полей, к которым NQT обращается по имени, в alandtse и libxse на тех же смещениях; размеры
классов расходятся только у 13 (из используемых — `UI`, он идёт через функции libxse). Ни одна функция libxse с
ID 0 (неизвестна на AE) в коде не вызывается. libxse описывает BGSQuestObjective / TESQuestTarget / REF_DATA /
QuestTargetArray / BGSLocAlias (+0x28) так же, как наш `EngineTypes.h`.
Не проверено кодом AE: MapMarkerData тип +0x12 (libxse говорит то же).

**Инструмент:** `D:\Work\vs_projects\fallout4\tools\fo4_ae.py` — AE-аналог `fo4_pdb.py`: имена из libxse `IDs.h` /
`IDs_VTABLE.h` через Address Library AE, границы функций из `.pdata` (с chained-кусками), `sym/dis/id/at/xref/calls/
str/ptr/q`. Приёмы: vtable-ID в Address Library AE остались OG-шными (у 88 тыс. ID данные/vtable общие с OG);
строки отладки функций условий (`<Имя> >> %0.2f`) и таблица команд (`ptr` на строку имени → +0x40 = функция
условия) — надёжная привязка между версиями.

**F4SE 0.7.7 (AE):** в `f4se_1_11_191.dll` и Scripts 0.7.7 есть всё, что зовут скрипты и окно (UI.RegisterCustomMenu,
OpenMenu/CloseMenu/IsMenuOpen/IsMenuRegistered/Invoke, AllowTextInput, SendExternalEvent, RegisterForExternalEvent,
Game.GetForm).

**Стенд:** профиль «Nearest Quest Tracker» = копия «I Am Alive» (ini, Quicksave0 lvl 90 из основной игры — в нём уже
есть данные NQT), мод I Am Alive выключен, `NearestQuestTracker.esp` в plugins.txt; в dev-мод положен
`MCM/Settings/Keybinds.json` с клавишами пользователя (F1 отметить, F2 исключить, F3 авто, F4 текущий квест, F7 окно).
Лог DLL — тот же `My Games\Fallout4\F4SE\NearestQuestTracker.log`, что у основной игры (перезапишется).

## Находки 2026-10-18 (NG и AE, проверено по файлам)

- **ID у AE свои.** Address Library AE 1.11.191 (`version-1-11-191-0.bin`, 651 571 ID, диапазон 15..8717431) не содержит
  ни NG-ID из alandtse (ParseString NG 2189084 там нет; на AE это 2206630), ни наших OG-ID. Значит, DLL на alandtse
  на AE не заработает даже с правкой `main.cpp`: alandtse считает 1.11.x «NG» (`Module.cpp`: `_version[2] > 163`) и
  ищет NG-ID.
- **Библиотека для AE есть: libxse/commonlibf4, ветка `main`** (живая, коммиты сентябрь 2026). `RUNTIME_LATEST =
  1.11.240`, версии 1.11.137/159/169/191/221/240. Все 1092 ненулевых ID из её `include/RE/IDs.h` находятся в
  Address Library 1.11.191. Только AE: NG и OG она не поддерживает (ветка `frozen-1.10.63` — старая OG, 2023).
  Сборка — xmake (не CMake), раскладка заголовков другая (`RE/P/PlayerCharacter.h`, ID в `RE/IDs.h`), REL — в
  подмодуле `libxse/commonlib-shared`.
- **Флаги F4SE для AE** (`F4SE/Interfaces.h` libxse): `kAddressIndependence_AddressLibrary_1_11_137` (1<<2) и
  `kStructureIndependence_1_11_137Layout` (1<<2) — отдельные от NG (`..._1_10_980`). F4SE для AE в описании плагина
  требует именно их (Buffout4 NG с флагом только NG на AE отключается — видели 2026-10-15). Раз есть отдельный флаг
  раскладки 1.11.137, раскладки структур в AE менялись — сверять.
- **NG в alandtse — неполный:** `REL::RelocationID(og, ng)` — 2938 мест, простой `REL::ID(og)` — 10 476 (на NG такой ID
  ищется в NG-базе как есть = неверный адрес / отказ). Каждую функцию CommonLibF4, которую зовёт NQT, проверять на
  NG-ID. Наши свои ID только OG: 395737, 78630, 1552479, 586385, 167267, 43452, 339469 (ParseString уже
  `RelocationID(407804, 2189084)`).
- NQT использует ~60 типов RE:: (больше всего TESQuest, TESObjectREFR, TESWorldSpace, PlayerCharacter, TESObjectCELL),
  своих раскладок — `EngineTypes.h` (160 строк) + смещения в комментариях `Interiors.cpp` / `MapMarkers.cpp` /
  `QuestStarts.cpp`. Вызовов Papyrus из C++ (DispatchStaticCall) нет — проблема OG/NG-обёртки не касается.
- Стенда NG нет (нет ни exe 1.10.984, ни F4SE для NG); стенд AE есть (`D:\Games\MO2 Fallout 4 AE`, exe 1.11.191).

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
