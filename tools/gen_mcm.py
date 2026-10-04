"""
Генератор MCM и переводов Nearest Quest Tracker.

Выход (в mod/, откуда их раскладывает tools/deploy.py):
    MCM/Config/NearestQuestTracker/config.json             — страница, тексты токенами $NQT_*
    MCM/Config/NearestQuestTracker/keybinds.json           — хоткеи -> NQT_MainScript.TrackNearest/ShowNearestStart/
                                                             ExcludeCurrent/RestoreLast
    Interface/Translations/NearestQuestTracker_{en,ru}.txt — UTF-16 LE с BOM, TAB, CRLF

Настройки — switcher прямо на глобальные переменные (sourceType GlobalValue, как во FSOC):
NQT_UntrackOthers «снимать отметку с остальных», NQT_Notifications «уведомления», NQT_AutoTrack
«автоматически» (его action OnAutoTrackChanged запускает/останавливает таймер), NQT_IncludeStarts
«учитывать не начатые квесты», NQT_IncludeNoMarker «учитывать квесты без маркера», NQT_NoSpoilers «без спойлеров» (action OnNoSpoilersChanged переписывает
текст показанного маркера старта). Значения живут
в сохранении, скрипт читает их при каждом нажатии.

Список исключённых — dropdown с недокументированным ключом MCM `valueOptions.listFromProperty`
(пункты = String[]-свойство ExcludedNames скрипта). Проверено по исходникам MCM: Neanka/MCM_0.1_AS3
MCM_Menu.as перечитывает список при загрузке страницы и в RefreshMCM, reg2k/f4mcm GetPropertyValueEx
отдаёт массивы. Значение — PropertyValueInt MCMRestoreChoice; выбор пункта вызывает
RestoreFromMenu({value}), тот убирает квест и зовёт MCM.RefreshMenu(), чтобы список перечитался.

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
NO_SPOILERS_GLOBAL = PLUGIN + '|807'
INCLUDE_NO_MARKER_GLOBAL = PLUGIN + '|808'

HOTKEYS = [
    ('TrackNearestHotkey', 'KEY_TRACK_NEAREST', 'TrackNearest'),
    ('ShowStartHotkey', 'KEY_SHOW_START', 'ShowNearestStart'),
    ('ExcludeHotkey', 'KEY_EXCLUDE', 'ExcludeCurrent'),
    ('RestoreHotkey', 'KEY_RESTORE', 'RestoreLast'),
]

STRINGS = {
    'en': {
        'MOD_NAME': 'Nearest Quest Tracker',
        'ABOUT': 'Press the hotkey to track the quest from your Pip-Boy whose marker is closest to you. '
                 'Quests in your current world come first, then the Commonwealth, Far Harbor and Nuka-World; '
                 'within a world, distance follows the route the compass marker takes, through doors.',
        'SEC_KEYS': 'Hotkeys',
        'KEY_TRACK_NEAREST': 'Track nearest quest',
        'KEY_TRACK_NEAREST_HELP': 'Tracks the Pip-Boy quest with the closest marker.',
        'KEY_SHOW_START': 'Show nearest new quest',
        'KEY_SHOW_START_HELP': 'Puts a marker on the nearest place where a quest you have not started yet begins '
                               '(a quest giver). The marker goes away once the quest starts.',
        'KEY_EXCLUDE': 'Exclude tracked quest',
        'KEY_EXCLUDE_HELP': 'Excludes the tracked quest from the search and tracks the next nearest one. '
                            'A quest leaves the list when it is completed.',
        'KEY_RESTORE': 'Restore last excluded quest',
        'KEY_RESTORE_HELP': 'Takes the last excluded quest back and tracks the nearest quest again.',
        'SEC_MAIN': 'Settings',
        'UNTRACK_OTHERS': 'Untrack other quests',
        'UNTRACK_OTHERS_HELP': 'When on, the hotkey untracks every other quest, so only the nearest one '
                               'shows on the compass.',
        'NOTIFICATIONS': 'Notifications',
        'NOTIFICATIONS_HELP': 'When on, a notification names the quest that is now tracked.',
        'AUTO_TRACK': 'Automatic',
        'AUTO_TRACK_HELP': 'When on, the nearest quest is tracked by itself whenever an objective is completed '
                           'or a new quest starts.',
        'INCLUDE_STARTS': 'Include quests not started',
        'INCLUDE_STARTS_HELP': 'When on, "Track nearest quest" also considers quests you have not started yet and '
                               'marks the start of one if it is closer than any Pip-Boy quest.',
        'INCLUDE_NO_MARKER': 'Include quests without a marker',
        'INCLUDE_NO_MARKER_HELP': 'When on, Pip-Boy quests that have no marker (e.g. "find it yourself" quests) are '
                                  'tracked too, after all quests with a marker in the same world.',
        'NO_SPOILERS': 'No spoilers',
        'NO_SPOILERS_HELP': 'When on, the marker of a quest not started yet does not say which quest it is.',
        'SEC_EXCLUDED': 'Excluded quests',
        'EXCLUDED_ABOUT': 'This mod never tracks excluded quests. Pick one to take it back.',
        'RESTORE_PICK': 'Restore quest',
        'RESTORE_PICK_HELP': 'Pick an excluded quest to take it back into the search.',
        'RESTORE_ALL': 'Restore all',
        'RESTORE_ALL_HELP': 'Empties the list of excluded quests.',
    },
    'ru': {
        'MOD_NAME': 'Nearest Quest Tracker',
        'ABOUT': 'Нажмите хоткей, чтобы отметить квест из Pip-Boy, маркер которого ближе всего к вам. '
                 'Сначала квесты мира, где вы сейчас, потом Содружества, Фар-Харбора и Ядер-Мира; внутри мира '
                 'расстояние считается по маршруту маркера компаса, через двери.',
        'SEC_KEYS': 'Хоткеи',
        'KEY_TRACK_NEAREST': 'Отметить ближайший квест',
        'KEY_TRACK_NEAREST_HELP': 'Отмечает квест из Pip-Boy с ближайшим маркером.',
        'KEY_SHOW_START': 'Показать ближайший новый квест',
        'KEY_SHOW_START_HELP': 'Ставит маркер на ближайшее место, где начинается ещё не начатый квест '
                               '(квестодатель). Маркер пропадает, когда квест начат.',
        'KEY_EXCLUDE': 'Исключить отмеченный квест',
        'KEY_EXCLUDE_HELP': 'Исключает отмеченный квест из поиска и отмечает следующий ближайший. '
                            'Выполненный квест сам уходит из списка.',
        'KEY_RESTORE': 'Вернуть последний исключённый',
        'KEY_RESTORE_HELP': 'Возвращает последний исключённый квест и снова отмечает ближайший.',
        'SEC_MAIN': 'Настройки',
        'UNTRACK_OTHERS': 'Снимать отметку с остальных',
        'UNTRACK_OTHERS_HELP': 'Если включено, хоткей снимает отметку со всех остальных квестов, и на компасе '
                               'остаётся только ближайший.',
        'NOTIFICATIONS': 'Уведомления',
        'NOTIFICATIONS_HELP': 'Если включено, уведомление называет квест, который теперь отмечен.',
        'AUTO_TRACK': 'Автоматически',
        'AUTO_TRACK_HELP': 'Если включено, ближайший квест отмечается сам, когда выполнена цель или начат '
                           'новый квест.',
        'INCLUDE_STARTS': 'Учитывать не начатые квесты',
        'INCLUDE_STARTS_HELP': 'Если включено, «Отметить ближайший квест» учитывает и ещё не начатые квесты и '
                               'отмечает начало такого квеста, если оно ближе квестов из Pip-Boy.',
        'INCLUDE_NO_MARKER': 'Учитывать квесты без маркера',
        'INCLUDE_NO_MARKER_HELP': 'Если включено, отмечаются и квесты Pip-Boy без маркера (например, «найди сам») — '
                                  'после всех квестов с маркером в том же мире.',
        'NO_SPOILERS': 'Без спойлеров',
        'NO_SPOILERS_HELP': 'Если включено, маркер не начатого квеста не называет, что это за квест.',
        'SEC_EXCLUDED': 'Исключённые квесты',
        'EXCLUDED_ABOUT': 'Исключённые квесты мод никогда не отмечает. Выберите квест, чтобы вернуть его.',
        'RESTORE_PICK': 'Вернуть квест',
        'RESTORE_PICK_HELP': 'Выберите исключённый квест, чтобы вернуть его в поиск.',
        'RESTORE_ALL': 'Вернуть все',
        'RESTORE_ALL_HELP': 'Очищает список исключённых квестов.',
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


def config():
    content = [
        {'type': 'text', 'text': t('ABOUT')},
        {'type': 'spacer'},
        {'type': 'section', 'text': t('SEC_KEYS')},
    ]
    for hid, key, _ in HOTKEYS:
        content.append({'id': hid, 'type': 'hotkey', 'text': t(key), 'help': t(key + '_HELP')})
    content += [
        {'type': 'spacer'},
        {'type': 'section', 'text': t('SEC_MAIN')},
        global_switcher('UNTRACK_OTHERS', UNTRACK_OTHERS_GLOBAL),
        global_switcher('NOTIFICATIONS', NOTIFICATIONS_GLOBAL),
        global_switcher('AUTO_TRACK', AUTO_TRACK_GLOBAL, 'OnAutoTrackChanged'),
        global_switcher('INCLUDE_STARTS', INCLUDE_STARTS_GLOBAL),
        global_switcher('INCLUDE_NO_MARKER', INCLUDE_NO_MARKER_GLOBAL),
        global_switcher('NO_SPOILERS', NO_SPOILERS_GLOBAL, 'OnNoSpoilersChanged'),
        {'type': 'spacer'},
        {'type': 'section', 'text': t('SEC_EXCLUDED')},
        {'type': 'text', 'text': t('EXCLUDED_ABOUT')},
        {'type': 'dropdown', 'text': t('RESTORE_PICK'), 'help': t('RESTORE_PICK_HELP'),
         'valueOptions': {'sourceType': 'PropertyValueInt', 'sourceForm': QUEST, 'scriptName': SCRIPT,
                          'propertyName': 'MCMRestoreChoice',
                          'listFromProperty': {'form': QUEST, 'scriptName': SCRIPT, 'propertyName': 'ExcludedNames'}},
         'action': call('RestoreFromMenu', ['{value}'])},
        {'type': 'button', 'text': t('RESTORE_ALL'), 'help': t('RESTORE_ALL_HELP'), 'action': call('RestoreAll')},
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
