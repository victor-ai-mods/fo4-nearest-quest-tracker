# Nearest Quest Tracker — версия для NG (Next-Gen, 1.10.984)

Статус (2026-10-18): **NG-0..NG-3 сделаны** — DLL OG/NG собрана (83f7fa4), стенд NG готов, **в игре работает**
(Debug-DLL, пользователь 2026-10-18); дальше NG-4 релиз. Решение: одна DLL на OG и NG (alandtse/CommonLibF4 выбирает ID по версии игры), AE — своя.

## NG-1: ID движка (сделано, проверено по файлам)

- **Опасность alandtse на NG:** `REL::IDDB::id2offset` — `lower_bound` без проверки равенства (вне VR). ID «только OG»
  (`REL::ID(og)`) на NG молча даёт адрес *следующего* ID базы — без ошибки, падение или мусор потом.
- **Какие ID реально в DLL:** `dumpbin /disasm` Release-DLL (с PDB) → все вызовы `IDDB::id2offset` /
  `RelocationID::address|offset` и константы перед ними (`D:\Work\vs_projects\fallout4\tools\dll_id_sites.py <disasm.txt> <ng.bin> <og.bin>`: ищет
  `mov edx|qword ptr [rsp/rbp±X], imm` перед call, имя static из `?$TSS0@...` / `mov [?...],rax`). /OPT:REF ⇒ только
  то, что достижимо. Итог до правки: 12 ID «только OG» на путях NG — 7 своих + `BSScript::Object::DecRef` 541793,
  `ObjectBindPolicy::BindObject` 709728, `Stack::GetPageForFrame` 1429302, `TESObjectREFR::GetLock` 930785,
  `IsCrimeToActivate` 836011 (+ `GetPtr` 504957 и `IsLocAlias` 78630 — под `!IsNG()`). Статические глобалы заголовков
  (ImageSpaceEffect…, Workshop…, fWorldGravity) — только вычисляют адрес при загрузке DLL, не используются.
- **NG и AE делят ID:** в базах 1.10.984 и 1.11.191 общих 383 582 ID (meh321 оставил NG-ID совпавшим функциям AE).
  Все 7 наших AE-ID и AE-ID 5 функций выше (из libxse `IDs.h`) есть в базе NG. Код NG по этим ID = код AE
  инструкция в инструкцию (`tools\fo4_ng.py cmp <id>`; отличаются только адреса глобалов, у которых в AE новые ID).
  ⇒ `GameID(og, ngae)` в `PCH.h`: в OG/NG-DLL — `REL::RelocationID(og, ngae)`, в AE-DLL — `REL::ID(ngae)`.
- **Пары alandtse (OG, NG), которые зовёт DLL** (~40): NG-ID = ID той же функции в libxse (AE) — совпали все, кроме
  глобалов-синглтонов (у AE свои ID). Синглтоны PlayerCharacter 2690919, TESDataHandler 2688883, GameSettingCollection
  2690301, GameVM 2689134, TESForm allForms 2689178 / allFormsMapLock 2689189 проверены `fo4_ng.py gcheck id:<n>`:
  функции NG (с общим с AE ID), читающие глобал, в AE на той же инструкции читают одноимённый глобал libxse.
  MemoryManager::GetSingleton 2193197 — тот же адрес, что берёт BSStringT::Set. BSStringT::Set NG 2189084 —
  дизассемблировано (strlen, ёмкость +0A, Allocate, размер +08).
- **Подмодуль:** `extern/CommonLibF4` → форк https://github.com/victor-ai-mods/CommonLibF4, ветка `nqt-ng-ids` (от
  закреплённого ba22620e + коммит 299790ef: `RelocationID` с NG-ID для DecRef 2314437, BindObject 2314370,
  GetPageForFrame 2314680, GetLock 2202648, IsCrimeToActivate 2201180). В upstream (2026-10-15, #46..#51) эти 5 не
  исправлены; там же поправили NG-ID ImageSpaceEffectRadialBlur::bDisable и Actor::IsPathing (нам не нужны).
  Можно отправить PR в alandtse — спросить пользователя.
- **IsLocAlias** на NG — сравнение vtable `VTABLE::BGSLocAlias` (ID 1011963 общий для OG/NG/AE, RTTI NG проверен),
  как на AE. **sLanguage** на NG — свой поиск без учёта регистра по `INISettingCollection::settings` (как GetPtr OG).
- После правки аудит повторён: «голых» OG-ID на путях NG нет, все 11 новых пар в DLL.

## NG-2: раскладки (сделано, проверено по коду NG)

Код NG = код AE в функциях, по которым сверялись раскладки для AE: TeleportPath::ComputeLength (NG 751840 = AE
7A4F80), TESQuestStage IsStageDone (617A30 = 66B3C0), IntSeenData ctor (BBE960 = C44440), все слоты vtable
PipboyQuestData; vtable TESObjectREFR / PlayerCharacter / TESQuest — слот в слот (нужные 0x8C, 0xBE, 0xC0 совпадают),
GetAliasedLoc, GetPCAnEnemy, GetFactionCombatRelation, IsTrue, CalculateFactionFightReaction — те же смещения.
Раскладки AE = OG для полей NQT (PLAN-AE.md) ⇒ на NG те же. F4SE 0.7.2: в `UI.psc`, `ScriptObject.psc` и
`f4se_1_10_984.dll` есть всё, что зовут скрипты и окно (RegisterCustomMenu, OpenMenu/CloseMenu/IsMenuOpen/
IsMenuRegistered/Invoke, RegisterForExternalEvent, AllowTextInput, SendExternalEvent).

**Инструмент:** `D:\Work\vs_projects\fallout4\tools\fo4_ng.py` — `fo4_ae.py` на exe NG (`sym/dis/id/at/xref/calls/str/
ptr/q`, имена: libxse по общим ID + пары alandtse через PDB OG с префиксом `og:`), плюс `cmp <id>` (NG и AE рядом,
адреса замаскированы), `cmpx <ng> <ae>`, `cmpog <og> <ng>`, `gcheck <ng-id>` (проверка глобала по выровненной
инструкции AE).

## NG-0: стенд (сделано 2026-10-18)

- Игра `D:\Games\Fallout 4 NG` (1.10.984, скачана пользователем; `Fallout4_Default.ini` — sLanguage=ru, строки всех
  языков в `Fallout4 - Interface.ba2`; часть cc-файлов). `Fallout4Launcher.exe` → `.exe_` (не запустить случайно);
  F4SE 0.7.2 (`f4se_loader.exe`, `f4se_1_10_984.dll`, Nexus 42147 file 316287) — в корень игры;
  HKCU AppCompatFlags `D:\Games\Fallout 4 NG\Fallout4.exe = ~ HIGHDPIAWARE` (как у остальных копий).
- Портативный MO2 `D:\Games\MO2 Fallout 4 NG` (программа скопирована из AE-стенда), профиль «Nearest Quest Tracker»:
  LocalSaves + LocalSettings; Fallout4.ini = NG `Fallout4_Default.ini` + те же правки, что в AE-профиле
  (`sResourceDataDirsFinal=`, `[Launcher] bEnableFileSelection=1`); Prefs/Custom — из AE-профиля.
- Моды (как на AE-стенде, где нужно — NG-сборки): F4SE Scripts 0.7.2, Address Library 1.10.984 (bin из AIO),
  Mod Configuration Menu 1.40 BETA 2 (file 316359, mcm.dll объявляет ровно 1.10.984), Buffout4 NG (на NG родной —
  включён, на AE был выключен), High FPS Physics Fix 0.8.23 (объявляет 1.10.984), Unlimited Survival Mode (сигнатуры),
  Fix - DLC01 Track Loop, I Am Alive - Intimidation (выключен, как на AE), Weight Fixes (AE) — включены 6 патчей из 8
  (BFG и Scavenger: их cc-мастеров в NG нет), Nearest Quest Tracker (NG dev) = `python tools/deploy.py --ng`
  (+ `MCM/Settings/Keybinds.json` с клавишами пользователя: F1 отметить, F2 исключить, F3 авто, F4 текущий квест,
  F7 окно).
- Сохранение: исходный OG Quicksave0 ур. 90 (15.10, Goodneighbor) — сейв AE-профиля уже в формате AE (заголовок 107
  байт против 105). Убиты потоки `dlc01_tracksystemtrack` ×2 и `automedicquestscript` (KillThreads.java, как для AE);
  бэкапы ReSaver — `saves_backup_20261018`.
- Копия общих настроек до первого запуска NG: `D:\Games\Fallout 4\Backup\settings-20261018-ng\` (My Games ini +
  AppData\Local\Fallout4). Не изолируется MO2: логи F4SE/Papyrus в My Games (перезапишут логи OG/AE),
  `%LOCALAPPDATA%\Fallout4\UserDownloadedContent.txt` (создан AE, строка есть и в exe NG — NG его читает).

## NG-3: первый запуск (пользователь) — ПРОЙДЕН 2026-10-18

MO2 `D:\Games\MO2 Fallout 4 NG` → F4SE → загрузить Quicksave0 (предупреждение о недостающем контенте — нормально).
Минимально: F1 «Отметить ближайшее», F4 «Текущий квест», F7 окно (обе вкладки, отметка кликом), предмет в избранном,
меню MCM мода. Сразу после выхода — прочитать `My Games\Fallout4\F4SE\NearestQuestTracker.log` (runtime 1.10.984,
язык ru, JSON загружены, без [W]/[E]) и `f4se.log` — до следующего запуска любой копии игры.

**Итог 2026-10-18 (лог):** runtime 1.10.984.0, F4SE 0.7.2.0, нативы и окно зарегистрированы, language ru (свой поиск
sLanguage работает), QuestStarts 138 / QuestCategories 377 в 62 (CC-файлов нет — пропущены), без [W]/[E]. F4
«Зачистить склады в Добрососедстве», окно 378 квестов за ~240 мс + 227 локаций, миры Содружество/Остров/Ядер-Мир,
отметка из окна («Отслеживается: Горнило»), окно из предмета избранного. Пользователь: «Работает».
За 9 с до этого сеанса — crash-лог Buffout4 (18:48:13): access violation в `nvwgf2umx.dll` (драйвер NVIDIA), стек
только из драйвера (его рабочий поток), NQT/игры в стеке нет; повторный запуск — нормально.

**Release-DLL 2.3.0 на NG (18:59):** F1 / окно / отметка из окна работают, без [W]/[E]. Находка: F1 выбрал «Интерьер:
Ячейка быстрого тестирования» — дверь `0024A1B1` (мир Goodneighbor, persistent, без замка и родителя включения) →
`ccTOSFO4002Interior01` (FULL 0x3687A = «Quick Test Cell»). Это заготовка Creation Club (Neon Flats) в самом
Fallout4.esm — и OG 1.10.163, и NG; на AE её перекрывает `cctosfo4002_neonflats.esm`. Таких заготовок с дверями из
миров 7: Neon Flats, Shroud Manor (`cczsefo4002-smanor.esm`) ×3 двери, Vault-Tec homes (`ccvltfo4001-homes.esm`) ×3.
Фикс 8911e25: `IsCreationClubStub` в `Interiors.cpp` — editor ID ячейки на `cc` и ни один из её `sourceFiles` не
cc-плагин ⇒ пропуск («creation club stub»). Касается и AE без этих CC ⇒ AE-файл тоже обновить до 2.3.0.

**PR в alandtse:** https://github.com/alandtse/CommonLibF4/pull/52 (ветка форка `ng-ids-bsscript-refr` = upstream
master + тот же патч). Если примут — вернуть подмодуль на upstream.

## NG-4: релиз

Одна DLL на OG и NG ⇒ основной файл Nexus («Nearest Quest Tracker») теперь для OG + NG; AE — отдельный файл.
README / описание Nexus: строка версий игры. Решения пользователя: номер версии, PR в alandtse.
