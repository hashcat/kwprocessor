#!/usr/bin/env python3

# Turn an xkeyboard-config layout into a kwprocessor keymap.
#
# kwprocessor wants 12 lines: four rows of the unshifted keyboard, four of the same rows with
# shift held, four with altgr. xkb holds the same information per key, as a list of levels, so
# the conversion is a matter of placing xkb's key names on kwprocessor's grid and resolving
# each level's keysym to a character.
#
# Keysym names come from X11's own keysymdef.h, whose U+ comments are the authoritative
# name to codepoint mapping. A name without one is a key that produces no character by
# itself, a dead key for instance, and it becomes a blank.

import os
import re
import sys

XKB_DIR = '/usr/share/X11/xkb/symbols'
KEYSYMDEF = '/usr/include/X11/keysymdef.h'

KEYMAP_WIDTH = 14
KEYMAP_HEIGHT = 4

# Where each xkb key sits on kwprocessor's grid.
#
# Two physical geometries, and xkb cannot tell them apart: ANSI and ISO carry the same keycodes
# and differ only in where two of them sit. On an ANSI board BKSL is the wide key above Enter,
# right of the bracket in the qwerty row, and there is no key between LeftShift and Z. On an ISO
# board BKSL moves down beside the apostrophe and LSGT appears between LeftShift and Z. Which
# one a layout is typed on is a property of the keyboard rather than of the layout, so
# xkeyboard-config keeps it out of the symbols and it has to be said here.
#
# This matters because the whole point of a keymap is which keys are adjacent. Put a UK layout
# on the ANSI grid and the hash sign lands above Enter instead of beside the apostrophe, so
# every walk through that corner of the keyboard is wrong.
#
# The shipped keymaps disagree about where LSGT goes: de, dk, es, fi, fr, it and sv put it at
# x=0 of the bottom row, while en-gb and pt leave x=0 blank and put it at x=1. x=0 is both the
# majority and the one that keeps the letter columns lined up with en-us, so that is the one
# used here.

ANSI = 'ansi'
ISO = 'iso'

# Layouts typed on an ANSI board. Everything else defaults to ISO.

ANSI_LAYOUTS = ('us', 'cn', 'kr', 'tw', 'au', 'cm', 'id', 'nz', 'ca', 'ph')

def make_grid(geometry: str) -> dict:
  out = {'TLDE': (0, 0)}

  for n in range(1, 13): out['AE%02d' % n] = (n, 0)
  for n in range(1, 13): out['AD%02d' % n] = (n, 1)
  for n in range(1, 12): out['AC%02d' % n] = (n, 2)
  for n in range(1, 12): out['AB%02d' % n] = (n, 3)

  if geometry == ANSI:
    out['BKSL'] = (13, 1)

    return out

  out['BKSL'] = (12, 2)
  out['LSGT'] = (0, 3)

  return out

GRID_ANY = {}
GRID_ANY.update(make_grid(ANSI))
GRID_ANY.update(make_grid(ISO))

# xkb's sentinels, and they do not mean the same thing. In a block that overrides an included
# one, "any" and "NoSymbol" leave that level as the include left it, which is how a layout adds
# a level 3 without restating its letters: gr's own block writes [any, any, U2015]. Treating
# them as blank wipes the inherited alphabet, which is what emptied Greek, Croatian and
# Slovenian. "VoidSymbol" is the one that really means nothing.

INHERIT = ('any', 'nosymbol', 'none', '')
NOTHING = ('voidsymbol',)

def load_keysyms(path: str) -> dict:
  out = {}

  pat = re.compile(r'^#define\s+XK_(\S+)\s+0x([0-9a-fA-F]+)\s*(?:/\*\s*U\+([0-9A-Fa-f]{4,6}))?')

  with open(path, encoding='utf-8', errors='replace') as fp:
    for line in fp:
      m = pat.match(line)

      if m is None: continue

      name = m.group(1)
      value = int(m.group(2), 16)
      uni = m.group(3)

      if uni is not None:
        out[name] = int(uni, 16)
        continue

      # Legacy Latin-1 range: the keysym value is the codepoint.

      if 0x20 <= value <= 0xff: out[name] = value

  return out

def resolve_keysym(name: str, syms: dict) -> str:
  key = name.strip()

  if key.lower() in NOTHING: return ''
  if key.lower() in INHERIT: return ''

  # Direct Unicode form, UXXXX.

  m = re.fullmatch(r'U([0-9a-fA-F]{4,6})', key)

  if m is not None: return chr(int(m.group(1), 16))

  m = re.fullmatch(r'0x([0-9a-fA-F]+)', key)

  if m is not None:
    v = int(m.group(1), 16)

    if (v & 0xff000000) == 0x01000000: return chr(v & 0x00ffffff)
    if 0x20 <= v <= 0xff: return chr(v)

    return ''

  if key.startswith('dead_'): return ''

  cp = syms.get(key)

  if cp is None: return ''

  return chr(cp)

def merge_key(out: dict, name: str, levels: list) -> None:
  have = out.get(name)

  if have is None:
    out[name] = list(levels)

    return

  merged = list(have)

  while len(merged) < len(levels): merged.append('any')

  for i, lv in enumerate(levels):
    if lv.strip().lower() in INHERIT: continue

    merged[i] = lv

  out[name] = merged

def key_levels(inner: str) -> list:
  # A key body may carry a type before its symbol list, as in
  #   key <AC01> { type[Group1]="FOUR_LEVEL", [ a, A, ae, AE ] };
  # and it may name the list explicitly, as symbols[Group1]= [ ... ]. Both put a bracketed
  # group in front of the one that holds the keysyms, so the qualifiers go before the split.

  body = re.sub(r'type\s*\[[^\]]*\]\s*=\s*"[^"]*"', ' ', inner)
  body = re.sub(r'(symbols|actions)\s*\[[^\]]*\]\s*=', ' ', body)

  groups = re.findall(r'\[([^\]]*)\]', body)

  if len(groups) == 0: return []

  return [s.strip() for s in groups[0].split(',')]

def read_block(layout: str, variant: str, seen: set) -> dict:
  path = os.path.join(XKB_DIR, layout)

  if os.path.isfile(path) == False: return {}

  tag = (layout, variant)

  if tag in seen: return {}

  seen.add(tag)

  with open(path, encoding='utf-8', errors='replace') as fp:
    text = fp.read()

  # Comments go first. xkb files annotate a key with the glyphs it produces, and those
  # annotations contain braces often enough that counting braces over the raw text cuts a
  # block short. ara's AB03 ends in "// U+0624 }", which stops the block seven keys early.

  text = re.sub(r'/\*.*?\*/', ' ', text, flags=re.S)
  text = re.sub(r'//[^\n]*', '', text)

  blocks = re.split(r'xkb_symbols\s+"([^"]+)"\s*\{', text)

  chosen = None

  for i in range(1, len(blocks) - 1, 2):
    if variant == '' or blocks[i] == variant:
      chosen = blocks[i + 1]
      break

  if chosen is None: return {}

  depth = 1
  end = len(chosen)

  for i, ch in enumerate(chosen):
    if ch == '{': depth += 1

    if ch == '}':
      depth -= 1

      if depth == 0:
        end = i
        break

  body = chosen[0:end]

  out = {}

  # Includes in source order, each merged level by level rather than assigned. A layout
  # routinely stacks several, and a later one that writes "any" where an earlier one put a
  # letter is saying "leave it alone", so a dict update here loses the alphabet.

  for inc in re.finditer(r'include\s+"([^"(]+)(?:\(([^)]*)\))?"', body):
    part = read_block(inc.group(1).strip(), (inc.group(2) or '').strip(), seen)

    for name, levels in part.items(): merge_key(out, name, levels)

  for m in re.finditer(r'key\s*<([A-Z0-9]+)>\s*\{(.*?)\}\s*;', body, re.S):
    name = m.group(1)

    if name not in GRID_ANY: continue

    levels = key_levels(m.group(2))

    if len(levels) == 0: continue

    merge_key(out, name, levels)

  return out

def build(layout: str, variant: str, syms: dict, geometry: str) -> list:
  grid_map = make_grid(geometry)

  keys = read_block(layout, variant, set())

  if len(keys) == 0: return []

  grid = [[[' '] * KEYMAP_WIDTH for _ in range(KEYMAP_HEIGHT)] for _ in range(3)]

  for name, levels in keys.items():
    if name not in grid_map: continue

    x, y = grid_map[name]

    for level in range(3):
      if level >= len(levels): continue

      ch = resolve_keysym(levels[level], syms)

      if ch == '': continue
      if ch == ' ': continue

      grid[level][y][x] = ch

  out = []

  for level in range(3):
    for y in range(KEYMAP_HEIGHT):
      out.append(''.join(grid[level][y]).rstrip())

  return out

def basechars(lines: list) -> str:
  out = []

  for line in lines:
    for c in line:
      if c == ' ': continue
      if c in out: continue

      out.append(c)

  return ''.join(out)

USAGE = '''usage: xkb2kwp.py [--ansi|--iso] <layout>[(variant)] ...

Writes a kwprocessor keymap and a matching base-character file for each layout named, into the
directory it runs in. The files are named after the xkb layout, with a variant in brackets, so
in(deva) writes in-deva.keymap and in-deva.base.

  --ansi      the layout is typed on an ANSI board, where the backslash sits above Enter
  --iso       the layout is typed on an ISO board, where an extra key sits left of Z
  --help, -h  this text

Without a geometry flag, a short list of layouts typed on an ANSI board is used and every other
layout is taken as ISO. The geometry belongs to the keyboard rather than to the layout, so
xkeyboard-config does not carry it and it cannot be detected.

  xkb2kwp.py ara th
  xkb2kwp.py --ansi us
  xkb2kwp.py 'in(deva)'
'''

def main() -> int:
  args = sys.argv[1:]

  if ('--help' in args) or ('-h' in args):
    print(USAGE, end='')

    return 0

  forced = ''

  if '--ansi' in args:
    forced = ANSI
    args.remove('--ansi')

  if '--iso' in args:
    forced = ISO
    args.remove('--iso')

  if len(args) == 0:
    print(USAGE, end='', file=sys.stderr)

    return 1

  syms = load_keysyms(KEYSYMDEF)

  for arg in args:
    m = re.fullmatch(r'([^(]+)(?:\(([^)]*)\))?', arg)

    layout = m.group(1)
    variant = m.group(2) or ''

    geometry = forced

    if geometry == '': geometry = ANSI if layout in ANSI_LAYOUTS else ISO

    lines = build(layout, variant, syms, geometry)

    if len(lines) == 0:
      print('%s: no usable block' % arg, file=sys.stderr)
      continue

    name = layout if variant == '' else '%s-%s' % (layout, variant)

    with open('%s.keymap' % name, 'w', encoding='utf-8') as fp:
      fp.write('\n'.join(lines) + '\n')

    with open('%s.base' % name, 'w', encoding='utf-8') as fp:
      fp.write(basechars(lines) + '\n')

    filled = sum(1 for line in lines for c in line if c != ' ')

    print('%-22s %-4s %2d lines, %3d keys, %3d basechars' % (
      name, geometry, len(lines), filled, len(basechars(lines))))

  return 0

if __name__ == '__main__':
  sys.exit(main())
