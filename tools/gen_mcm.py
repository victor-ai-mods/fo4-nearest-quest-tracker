"""
Генератор MCM и переводов Nearest Quest Tracker.

Выход (в mod/, откуда их раскладывает tools/deploy.py):
    MCM/Config/NearestQuestTracker/config.json             — страница, тексты токенами $NQT_*
    MCM/Config/NearestQuestTracker/keybinds.json           — хоткеи -> NQT_MainScript.TrackNearest/ExcludeCurrent/RestoreLast
    Interface/Translations/NearestQuestTracker_{en,ru}.txt — UTF-16 LE с BOM, TAB, CRLF

Один хоткей «Отметить ближайшее» (TrackNearest); что он выбирает — switcher'ы «Что выбирать» прямо на глобальные
переменные (sourceType GlobalValue, как во FSOC): NQT_IncludeQuests, NQT_IncludeNoMarker, NQT_IncludeStarts,
NQT_NoSpoilers (action OnNoSpoilersChanged переписывает текст показанного маркера старта), NQT_IncludeLocations,
NQT_AllLocations, NQT_IncludeInteriors, NQT_SkipOwnedLocked; настройки NQT_UntrackOthers «только один маркер», NQT_AutoTrack (action OnAutoTrackChanged
запускает/останавливает таймер), NQT_Notifications, NQT_ShowQuestInWindow (хоткей «Показать текущий квест» — окном). Значения живут в сохранении, скрипт читает их при каждом нажатии.

Общий список исключённого (квесты, новые квесты, локации, интерьеры) — dropdown с недокументированным ключом MCM
`valueOptions.listFromProperty` (пункты = String[]-свойство ExcludedNames скрипта). Проверено по исходникам MCM:
Neanka/MCM_0.1_AS3 MCM_Menu.as перечитывает список при загрузке страницы и в RefreshMCM, reg2k/f4mcm
GetPropertyValueEx отдаёт массивы. Значение — PropertyValueInt MCMRestoreChoice; выбор пункта вызывает
RestoreFromMenu({value}), тот убирает пункт и зовёт MCM.RefreshMenu(), чтобы список перечитался. Кнопки «Вернуть всё /
активные квесты / новые квесты / локации / интерьеры» — RestoreAll / RestoreKind(вид).

    python tools/gen_mcm.py
"""

import json
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'mod')
MOD = 'NearestQuestTracker'
PLUGIN = 'NearestQuestTracker.esp'
QUEST = PLUGIN + '|800'
SCRIPT = 'NQT_MainScript'
UNTRACK_OTHERS_GLOBAL = PLUGIN + '|801'
NOTIFICATIONS_GLOBAL = PLUGIN + '|802'
AUTO_TRACK_GLOBAL = PLUGIN + '|804'
INCLUDE_STARTS_GLOBAL = PLUGIN + '|806'
INCLUDE_RADIANT_GLOBAL = PLUGIN + '|813'
NO_SPOILERS_GLOBAL = PLUGIN + '|807'
INCLUDE_NO_MARKER_GLOBAL = PLUGIN + '|808'
ALL_LOCATIONS_GLOBAL = PLUGIN + '|80A'
INCLUDE_QUESTS_GLOBAL = PLUGIN + '|80C'
INCLUDE_LOCATIONS_GLOBAL = PLUGIN + '|80D'
INCLUDE_INTERIORS_GLOBAL = PLUGIN + '|80E'
SKIP_OWNED_LOCKED_GLOBAL = PLUGIN + '|80F'
SHOW_QUEST_IN_WINDOW_GLOBAL = PLUGIN + '|810'

# id хоткеев те же, что в 1.0: назначенные пользователем клавиши сохраняются.
HOTKEYS = [
    ('TrackNearestHotkey', 'KEY_TRACK_NEAREST', 'TrackNearest'),
    ('ExcludeHotkey', 'KEY_EXCLUDE', 'ExcludeCurrent'),
    ('RestoreHotkey', 'KEY_RESTORE', 'RestoreLast'),
    ('AutoTrackHotkey', 'KEY_AUTO_TRACK', 'ToggleAutoTrack'),
    ('ShowQuestHotkey', 'KEY_SHOW_QUEST', 'ShowCurrentQuest'),
    ('WindowHotkey', 'KEY_WINDOW', 'ToggleWindow'),
]

# Виды целей для кнопок «Вернуть …» (NQT_MainScript: KindQuest, KindStart, KindLocation, KindInterior).
KIND_QUEST = 1
KIND_START = 3
KIND_LOCATION = 4
KIND_INTERIOR = 5

STRINGS = {
    'en': {
        'MOD_NAME': 'Nearest Quest Tracker',
        'ABOUT': 'The hotkey marks the nearest target: a Pip-Boy quest, the start of a new quest, an undiscovered '
                 'location or an interior you have not been inside, whichever are turned on below. Targets in your current world come first, then the '
                 'Commonwealth, Far Harbor and Nuka-World.',
        'SEC_KEYS': 'Hotkeys',
        'KEY_TRACK_NEAREST': 'Mark nearest',
        'KEY_TRACK_NEAREST_HELP': 'Marks the nearest target of the kinds picked below. Quest distance follows the route '
                                  'the compass marker takes, through doors.',
        'KEY_EXCLUDE': 'Exclude marked',
        'KEY_EXCLUDE_HELP': 'Excludes the marked quest, location or interior from the search and marks the next nearest '
                            'one.',
        'KEY_RESTORE': 'Restore last excluded',
        'KEY_RESTORE_HELP': 'Takes the last excluded target back and marks the nearest one again.',
        'KEY_AUTO_TRACK': 'Toggle automatic mode',
        'KEY_AUTO_TRACK_HELP': 'Turns the "Automatic" setting below on or off; a notification tells which.',
        'KEY_SHOW_QUEST': 'Show current quest',
        'KEY_SHOW_QUEST_HELP': 'Shows the name of the tracked quest and its current objectives - what to do now.',
        'KEY_WINDOW': 'Open window',
        'KEY_WINDOW_HELP': 'Opens the Nearest Quest Tracker window (the game pauses). Esc closes it.',
        'MENU_TITLE': 'Nearest Quest Tracker',
        'MENU_EMPTY': 'The quest and location lists will be here.',
        'MENU_CLOSE': 'Close',
        'SEC_PICK': 'What to pick',
        'INCLUDE_QUESTS': 'Pick active quests',
        'INCLUDE_QUESTS_HELP': 'Quests from your Pip-Boy that have a marker.',
        'INCLUDE_NO_MARKER': 'Quests without a marker',
        'INCLUDE_NO_MARKER_HELP': 'With active quests on: also Pip-Boy quests without a marker ("find it yourself"), '
                                  'after the quests with a marker in the same world.',
        'INCLUDE_STARTS': 'Pick new quests',
        'INCLUDE_STARTS_HELP': 'Places where quests you have not started yet begin (quest givers). The marker goes '
                               'away once the quest starts.',
        'INCLUDE_RADIANT': 'Pick radiant quest givers',
        'INCLUDE_RADIANT_HELP': 'Givers of repeatable quests (Preston Garvey, Proctor Quinlan, PAM...) while they have '
                                'a task to give right now.',
        'NO_SPOILERS': 'No spoilers',
        'NO_SPOILERS_HELP': 'The marker of a new quest does not say which quest it is.',
        'INCLUDE_LOCATIONS': 'Pick undiscovered locations',
        'INCLUDE_LOCATIONS_HELP': 'Map locations you have not discovered yet; settlements are skipped. The marker goes '
                                  'away once you discover the location.',
        'ALL_LOCATIONS': 'All locations (spoilers)',
        'ALL_LOCATIONS_HELP': 'Off: only locations on your map, or on your compass. On: any undiscovered location, '
                              'with its name.',
        'INCLUDE_INTERIORS': 'Pick interiors',
        'INCLUDE_INTERIORS_HELP': 'Buildings, basements and caves you have never been inside (the marker is on the door '
                                  'from outside). Doors that need a key are skipped.',
        'SKIP_OWNED_LOCKED': 'Skip locked doors of others',
        'SKIP_OWNED_LOCKED_HELP': 'Interiors behind a locked door that is a crime to pick (red "Pick" prompt) are skipped. '
                                  "With its key, or in the owner's faction, the door is yours.",
        'SEC_MAIN': 'Settings',
        'UNTRACK_OTHERS': 'Only one marker',
        'UNTRACK_OTHERS_HELP': 'When on, the hotkey removes all other markers: other tracked quests, the new quest '
                               'marker and the location or interior marker.',
        'AUTO_TRACK': 'Automatic',
        'AUTO_TRACK_HELP': 'When on, the nearest target is marked by itself when an objective is completed, a quest '
                           'starts, or the marked location is discovered or interior entered.',
        'NOTIFICATIONS': 'Notifications',
        'NOTIFICATIONS_HELP': 'When on, a notification names the target that is now marked.',
        'SHOW_QUEST_IN_WINDOW': 'Current quest in a window',
        'SHOW_QUEST_IN_WINDOW_HELP': 'The "Show current quest" hotkey shows the quest in a message window (the game '
                                     'pauses until you close it) instead of notifications.',
        'SEC_EXCLUDED': 'Excluded',
        'RESTORE_PICK': 'Restore',
        'RESTORE_PICK_HELP': 'Pick an excluded target to take it back into the search. Completed quests, discovered '
                             'locations and visited interiors leave the list by themselves.',
        'SEC_WINDOW': 'Window',
        'OPEN_WINDOW': 'Open window',
        'OPEN_WINDOW_HELP': 'Opens the window once you leave the menu. With a gamepad: put the item "Nearest Quest '
                            'Tracker" (Aid) into your favorites.',
        'GIVE_ITEM': 'Give the item',
        'GIVE_ITEM_HELP': 'Puts the item "Nearest Quest Tracker" into the inventory (Aid) if it is not there. Using it '
                          'opens the window; in the favorites it opens with one gamepad button.',
        'RESTORE_ALL': 'Restore all',
        'RESTORE_ALL_HELP': 'Empties the list of excluded targets.',
        'RESTORE_QUESTS': 'Restore active quests',
        'RESTORE_QUESTS_HELP': 'Takes back all excluded Pip-Boy quests.',
        'RESTORE_STARTS': 'Restore new quests',
        'RESTORE_STARTS_HELP': 'Takes back all excluded quests not started yet.',
        'RESTORE_LOCATIONS': 'Restore locations',
        'RESTORE_LOCATIONS_HELP': 'Takes back all excluded locations.',
        'RESTORE_INTERIORS': 'Restore interiors',
        'RESTORE_INTERIORS_HELP': 'Takes back all excluded interiors.',
    },
    'ru': {
        'MOD_NAME': 'Nearest Quest Tracker',
        'ABOUT': 'Хоткей отмечает ближайшую цель: квест из Pip-Boy, начало нового квеста, неоткрытую локацию или '
                 'интерьер, где вы не были, — что включено ниже. Сначала цели мира, где вы сейчас, потом Содружества, Фар-Харбора и Ядер-Мира.',
        'SEC_KEYS': 'Хоткеи',
        'KEY_TRACK_NEAREST': 'Отметить ближайшее',
        'KEY_TRACK_NEAREST_HELP': 'Отмечает ближайшую цель из выбранных ниже. Расстояние до квеста — по маршруту '
                                  'маркера компаса, через двери.',
        'KEY_EXCLUDE': 'Исключить отмеченное',
        'KEY_EXCLUDE_HELP': 'Исключает отмеченный квест, локацию или интерьер из поиска и отмечает следующее '
                            'ближайшее.',
        'KEY_RESTORE': 'Вернуть последнее исключённое',
        'KEY_RESTORE_HELP': 'Возвращает последнее исключённое и снова отмечает ближайшее.',
        'KEY_AUTO_TRACK': 'Вкл/выкл автоматический режим',
        'KEY_AUTO_TRACK_HELP': 'Включает или выключает настройку «Автоматически» ниже; уведомление сообщает, что стало.',
        'KEY_SHOW_QUEST': 'Показать текущий квест',
        'KEY_SHOW_QUEST_HELP': 'Показывает название отслеживаемого квеста и его текущие цели — что нужно сделать.',
        'KEY_WINDOW': 'Открыть окно',
        'KEY_WINDOW_HELP': 'Открывает окно Nearest Quest Tracker (игра на паузе). Закрыть — Esc.',
        'MENU_TITLE': 'Nearest Quest Tracker',
        'MENU_EMPTY': 'Здесь будут списки квестов и локаций.',
        'MENU_CLOSE': 'Закрыть',
        'SEC_PICK': 'Что выбирать',
        'INCLUDE_QUESTS': 'Выбирать активные квесты',
        'INCLUDE_QUESTS_HELP': 'Квесты из Pip-Boy, у которых есть маркер.',
        'INCLUDE_NO_MARKER': 'Квесты без маркера',
        'INCLUDE_NO_MARKER_HELP': 'Если включены активные квесты: и квесты Pip-Boy без маркера («найди сам») — после '
                                  'квестов с маркером в том же мире.',
        'INCLUDE_STARTS': 'Выбирать новые квесты',
        'INCLUDE_STARTS_HELP': 'Места, где начинаются ещё не начатые квесты (квестодатели). Маркер пропадает, когда '
                               'квест начат.',
        'INCLUDE_RADIANT': 'Выбирать квестодателей радиантных квестов',
        'INCLUDE_RADIANT_HELP': 'Квестодатели повторяемых квестов (Престон Гарви, проктор Квинлан, ПАМ…), пока у них '
                                'есть задание для вас.',
        'NO_SPOILERS': 'Без спойлеров',
        'NO_SPOILERS_HELP': 'Маркер нового квеста не называет, что это за квест.',
        'INCLUDE_LOCATIONS': 'Выбирать неоткрытые локации',
        'INCLUDE_LOCATIONS_HELP': 'Локации карты, которые вы ещё не открыли; поселения пропускаются. Маркер пропадает, '
                                  'когда локация открыта.',
        'ALL_LOCATIONS': 'Все локации (спойлеры)',
        'ALL_LOCATIONS_HELP': 'Выключено: только локации на вашей карте или на компасе. Включено: любые неоткрытые, '
                              'с названием.',
        'INCLUDE_INTERIORS': 'Выбирать интерьеры',
        'INCLUDE_INTERIORS_HELP': 'Здания, подвалы и пещеры, где вы ещё не были внутри (маркер — на двери снаружи). '
                                  'Двери, которым нужен ключ, пропускаются.',
        'SKIP_OWNED_LOCKED': 'Пропускать чужие запертые',
        'SKIP_OWNED_LOCKED_HELP': 'Пропускать интерьеры за запертой дверью, взлом которой — преступление (красное «Взломать»). С ключом или во фракции владельца дверь ваша.',
        'SEC_MAIN': 'Настройки',
        'UNTRACK_OTHERS': 'Только один маркер',
        'UNTRACK_OTHERS_HELP': 'Если включено, хоткей убирает все остальные маркеры: другие отмеченные квесты, маркер '
                               'нового квеста и маркер локации или интерьера.',
        'AUTO_TRACK': 'Автоматически',
        'AUTO_TRACK_HELP': 'Если включено, ближайшая цель отмечается сама, когда выполнена цель, начат квест или '
                           'открыта отмеченная локация или вы вошли в отмеченный интерьер.',
        'NOTIFICATIONS': 'Уведомления',
        'NOTIFICATIONS_HELP': 'Если включено, уведомление называет цель, которая теперь отмечена.',
        'SHOW_QUEST_IN_WINDOW': 'Текущий квест — в окне',
        'SHOW_QUEST_IN_WINDOW_HELP': 'Хоткей «Показать текущий квест» показывает квест в окне сообщения (игра на паузе, '
                                     'пока окно не закрыто), а не уведомлениями.',
        'SEC_EXCLUDED': 'Исключённое',
        'RESTORE_PICK': 'Вернуть',
        'RESTORE_PICK_HELP': 'Выберите исключённое, чтобы вернуть в поиск. Выполненные квесты, открытые локации и '
                             'посещённые интерьеры уходят из списка сами.',
        'SEC_WINDOW': 'Окно',
        'OPEN_WINDOW': 'Открыть окно',
        'OPEN_WINDOW_HELP': 'Окно откроется, когда вы выйдете из меню. С геймпада удобнее предмет «Nearest Quest '
                            'Tracker» (Помощь) в избранном.',
        'GIVE_ITEM': 'Выдать предмет',
        'GIVE_ITEM_HELP': 'Кладёт в инвентарь (Помощь) предмет «Nearest Quest Tracker», если его нет. Использование '
                          'открывает окно; в избранном — одной кнопкой геймпада.',
        'RESTORE_ALL': 'Вернуть всё',
        'RESTORE_ALL_HELP': 'Очищает список исключённого.',
        'RESTORE_QUESTS': 'Вернуть активные квесты',
        'RESTORE_QUESTS_HELP': 'Возвращает все исключённые квесты из Pip-Boy.',
        'RESTORE_STARTS': 'Вернуть новые квесты',
        'RESTORE_STARTS_HELP': 'Возвращает все исключённые не начатые квесты.',
        'RESTORE_LOCATIONS': 'Вернуть локации',
        'RESTORE_LOCATIONS_HELP': 'Возвращает все исключённые локации.',
        'RESTORE_INTERIORS': 'Вернуть интерьеры',
        'RESTORE_INTERIORS_HELP': 'Возвращает все исключённые интерьеры.',
    },
}


def t(key):
    return '$NQT_' + key


def call(function, params=()):
    return {'type': 'CallFunction', 'form': QUEST, 'scriptName': SCRIPT, 'function': function, 'params': list(params)}


def global_switcher(key, form, action=None):
    control = {'type': 'switcher', 'text': t(key), 'help': t(key + '_HELP'),
               'valueOptions': {'sourceType': 'GlobalValue', 'sourceForm': form}}
    if action:
        control['action'] = call(action)
    return control


def button(key, function, params=()):
    return {'type': 'button', 'text': t(key), 'help': t(key + '_HELP'), 'action': call(function, params)}


def config():
    content = [
        {'type': 'text', 'text': t('ABOUT')},
        {'type': 'spacer'},
        {'type': 'section', 'text': t('SEC_WINDOW')},
        button('OPEN_WINDOW', 'OpenWindowFromMenu'),
        button('GIVE_ITEM', 'GiveWindowItemFromMenu'),
        {'type': 'spacer'},
        {'type': 'section', 'text': t('SEC_KEYS')},
    ]
    for hid, key, _ in HOTKEYS:
        content.append({'id': hid, 'type': 'hotkey', 'text': t(key), 'help': t(key + '_HELP')})
    content += [
        {'type': 'spacer'},
        {'type': 'section', 'text': t('SEC_PICK')},
        global_switcher('INCLUDE_QUESTS', INCLUDE_QUESTS_GLOBAL),
        global_switcher('INCLUDE_NO_MARKER', INCLUDE_NO_MARKER_GLOBAL),
        global_switcher('INCLUDE_STARTS', INCLUDE_STARTS_GLOBAL),
        global_switcher('INCLUDE_RADIANT', INCLUDE_RADIANT_GLOBAL),
        global_switcher('NO_SPOILERS', NO_SPOILERS_GLOBAL, 'OnNoSpoilersChanged'),
        global_switcher('INCLUDE_LOCATIONS', INCLUDE_LOCATIONS_GLOBAL),
        global_switcher('ALL_LOCATIONS', ALL_LOCATIONS_GLOBAL),
        global_switcher('INCLUDE_INTERIORS', INCLUDE_INTERIORS_GLOBAL),
        global_switcher('SKIP_OWNED_LOCKED', SKIP_OWNED_LOCKED_GLOBAL),
        {'type': 'spacer'},
        {'type': 'section', 'text': t('SEC_MAIN')},
        global_switcher('UNTRACK_OTHERS', UNTRACK_OTHERS_GLOBAL),
        global_switcher('AUTO_TRACK', AUTO_TRACK_GLOBAL, 'OnAutoTrackChanged'),
        global_switcher('NOTIFICATIONS', NOTIFICATIONS_GLOBAL),
        global_switcher('SHOW_QUEST_IN_WINDOW', SHOW_QUEST_IN_WINDOW_GLOBAL),
        {'type': 'spacer'},
        {'type': 'section', 'text': t('SEC_EXCLUDED')},
        {'type': 'dropdown', 'text': t('RESTORE_PICK'), 'help': t('RESTORE_PICK_HELP'),
         'valueOptions': {'sourceType': 'PropertyValueInt', 'sourceForm': QUEST, 'scriptName': SCRIPT,
                          'propertyName': 'MCMRestoreChoice',
                          'listFromProperty': {'form': QUEST, 'scriptName': SCRIPT, 'propertyName': 'ExcludedNames'}},
         'action': call('RestoreFromMenu', ['{value}'])},
        button('RESTORE_ALL', 'RestoreAll'),
        button('RESTORE_QUESTS', 'RestoreKind', [KIND_QUEST]),
        button('RESTORE_STARTS', 'RestoreKind', [KIND_START]),
        button('RESTORE_LOCATIONS', 'RestoreKind', [KIND_LOCATION]),
        button('RESTORE_INTERIORS', 'RestoreKind', [KIND_INTERIOR]),
    ]
    return {
        'modName': MOD,
        'displayName': t('MOD_NAME'),
        'minMcmVersion': 2,
        'pluginRequirements': [PLUGIN],
        'content': content,
    }


def keybinds():
    return {'modName': MOD, 'keybinds': [
        {'id': hid, 'desc': t(key), 'action': call(function)} for hid, key, function in HOTKEYS]}

def write_json(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w', encoding='utf-8', newline='\r\n') as f:
        json.dump(data, f, ensure_ascii=False, indent=4)
        f.write('\n')


def main():
    cfg_dir = os.path.join(OUT, 'MCM', 'Config', MOD)
    write_json(os.path.join(cfg_dir, 'config.json'), config())
    write_json(os.path.join(cfg_dir, 'keybinds.json'), keybinds())
    keys = set(STRINGS['en'])
    tr_dir = os.path.join(OUT, 'Interface', 'Translations')
    os.makedirs(tr_dir, exist_ok=True)
    for lang, table in STRINGS.items():
        assert set(table) == keys, '%s: ключи не совпадают с en: %s' % (lang, keys ^ set(table))
        for k, v in table.items():
            assert '\t' not in v and '\n' not in v, (lang, k)
            if k.endswith('_HELP') and len(v) > 165:
                print('  ! %s %s: подсказка %d символов (> 165 — мелкий шрифт)' % (lang, k, len(v)))
        text = ''.join('%s\t%s\r\n' % (t(k), v) for k, v in table.items())
        with open(os.path.join(tr_dir, '%s_%s.txt' % (MOD, lang)), 'wb') as f:
            f.write(b'\xff\xfe' + text.encode('utf-16-le'))
    print('MCM: %s, переводы: %s' % (os.path.relpath(cfg_dir, ROOT), ', '.join(STRINGS)))


if __name__ == '__main__':
    main()
