"""
Bobbleheads for the "Bobbleheads" kind of targets -> mod/F4SE/Plugins/NearestQuestTracker/Bobbleheads.json.

A bobblehead is a MISC item with one of the vanilla scripts that give its bonus when the player picks it up
(Data/Scripts/Source/Base):
  BobbleheadSkillScript  - OnActivate by the player: AddPerk(PerkToAdd)         -> "perk": the PERK
  BobbleheadSPECIALscrip - OnActivate by the player: PerksQuest.SetStage(stage) -> "quest" + "stage"
The DLL counts a bobblehead as found once its bonus is there (HasPerk / the stage is done) - what the game itself
checks before giving it again. The places are the persistent REFRs of those items from the master index
(tools/esm_index.py; all 20 vanilla ones are persistent, so the engine keeps them in memory in every cell).

python tools/gen_bobbleheads.py [--index <pickle>]
Default index: AE (esm_index_ae.pickle: AE masters + Creation Club plugins); the VMAD of the MISC records is read
from the plugins of the AE game (D:\\Games\\F4_AE\\Data), falling back to the OG game for the masters.
"""

import argparse
import json
import os
import pickle
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from esm import Plugin  # noqa: E402
import esm_index  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, 'mod', 'F4SE', 'Plugins', 'NearestQuestTracker')
DEFAULT_INDEX = os.path.join(ROOT, '..', '_research', 'nqt-quests', 'esm_index_ae.pickle')
GAMES = [r'D:\Games\F4_AE', r'D:\Games\Fallout 4']

SKILL_SCRIPT = 'bobbleheadskillscript'
SPECIAL_SCRIPT = 'bobbleheadspecialscrip'


def wstring(d, pos):
    n = struct.unpack_from('<H', d, pos)[0]
    return d[pos + 2:pos + 2 + n].decode('cp1252'), pos + 2 + n


def read_value(d, pos, ptype, obj_format):
    """Returns (value, new pos) for the property types the bobblehead scripts use; raises on others."""
    if ptype == 1:  # Object: objFormat 2 = unused u16, alias i16, FormID u32
        if obj_format != 2:
            raise ValueError('object format %d' % obj_format)
        return struct.unpack_from('<I', d, pos + 4)[0], pos + 8
    if ptype == 2:
        return wstring(d, pos)
    if ptype == 3:
        return struct.unpack_from('<i', d, pos)[0], pos + 4
    if ptype == 4:
        return struct.unpack_from('<f', d, pos)[0], pos + 4
    if ptype == 5:
        return d[pos] != 0, pos + 1
    raise ValueError('property type %d' % ptype)


def scripts_of(vmad):
    """[(script name lower, {property name lower: value})] of a plain VMAD (no fragments)."""
    _version, obj_format, count = struct.unpack_from('<hhH', vmad, 0)
    pos = 6
    result = []
    for _ in range(count):
        name, pos = wstring(vmad, pos)
        pos += 1  # status
        props = {}
        pcount = struct.unpack_from('<H', vmad, pos)[0]
        pos += 2
        for _ in range(pcount):
            pname, pos = wstring(vmad, pos)
            ptype = vmad[pos]
            pos += 2  # type, status
            props[pname.lower()], pos = read_value(vmad, pos, ptype, obj_format)
        result.append((name.lower(), props))
    return result


def plugin_path(name):
    for game in GAMES:
        path = os.path.join(game, 'Data', name)
        if os.path.exists(path):
            return path
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--index', default=DEFAULT_INDEX)
    args = ap.parse_args()
    with open(args.index, 'rb') as f:
        index = pickle.load(f)

    plugins = list(esm_index.MASTERS)
    if os.path.exists(os.path.join(GAMES[0], 'Fallout4.ccc')):
        plugins += esm_index.cc_plugins(GAMES[0])

    items = {}  # "Plugin|XXXXXX" of the MISC -> entry without "ref"
    for name in plugins:
        path = plugin_path(name)
        if not path:
            print('skipped (not found): %s' % name)
            continue
        plugin = Plugin(path)

        def key(form_id):
            owner, local = plugin.resolve(form_id)
            return '%s|%06X' % (owner, local)

        for rec in plugin.records(b'MISC'):
            vmad = rec.first(b'VMAD')
            if not vmad:
                continue
            lower = vmad.lower()
            if SKILL_SCRIPT.encode() not in lower and SPECIAL_SCRIPT.encode() not in lower:
                continue  # other scripts may have property types read_value does not know (arrays, structs)
            for script, props in scripts_of(vmad):
                entry = None
                if script == SKILL_SCRIPT and props.get('perktoadd'):
                    entry = {'perk': key(props['perktoadd'])}
                elif script == SPECIAL_SCRIPT and props.get('perksquest') and 'stage' in props:
                    entry = {'quest': key(props['perksquest']), 'stage': props['stage']}
                if entry:
                    entry['name'] = rec.editor_id()
                    items[key(rec.form_id)] = entry
    print('%d bobblehead items' % len(items))

    found = []
    for ref_key, ref in index['refs'].items():
        item = items.get(ref.get('base'))
        if item is None or ref.get('sig') != 'REFR':
            continue
        cell = index['cells'].get(ref.get('cell'), {})
        world = index['worlds'].get(ref.get('world'), {})
        where = ((cell.get('edid') if isinstance(cell, dict) else None) or (world.get('edid') if isinstance(world, dict) else world)
                 or ref.get('world') or ref.get('cell'))
        entry = {'ref': ref_key, 'item': ref['base']}
        entry.update({k: v for k, v in item.items() if k != 'name'})
        entry['name'] = '%s @ %s' % (item['name'], where)
        found.append(entry)
    found.sort(key=lambda e: e['name'])
    missing = sorted(set(items) - {e['item'] for e in found})
    for k in missing:
        print('no persistent ref: %s %s' % (k, items[k]['name']))

    doc = {
        '_comment': [
            'Nearest Quest Tracker: bobbleheads (persistent refs of the MISC items with BobbleheadSkillScript /',
            'BobbleheadSPECIALscrip). Generated by tools/gen_bobbleheads.py - do not edit. A bobblehead is found once',
            'the player has its "perk", or the "stage" of its "quest" is done.',
        ],
        'bobbleheads': found,
    }
    out = os.path.join(OUT_DIR, 'Bobbleheads.json')
    with open(out, 'w', encoding='utf-8', newline='\n') as f:
        f.write('{\n    "_comment": %s,\n    "bobbleheads": [\n' % json.dumps(doc['_comment'], ensure_ascii=False))
        f.write(',\n'.join('        ' + json.dumps(e, ensure_ascii=False) for e in found))
        f.write('\n    ]\n}\n')
    print('%d bobbleheads -> %s' % (len(found), out))


if __name__ == '__main__':
    main()
