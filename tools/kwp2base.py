#!/usr/bin/env python3

# Derive a kwprocessor base-character file from a keymap.
#
# A base-character file lists the characters a walk may start from, and kwprocessor ignores a
# listed character that the keymap does not carry. The useful file for one layout is therefore
# every character that layout can reach, which is the set of non-blank cells in its keymap.
#
# xkb2kwp.py already writes this file for a layout it generates from xkeyboard-config. This
# script covers the keymaps that were written by hand, and reproduces any of the others from
# the keymap alone.

import sys

KEYMAP_WIDTH = 14
KEYMAP_HEIGHT = 4
KEYMAP_LEVELS = 3

BOM = '\ufeff'

def basechars(lines: list) -> str:
  out = []

  for line in lines:
    for c in line:
      if c == ' ': continue
      if c in out: continue

      out.append(c)

  return ''.join(out)

def load(path: str) -> list:
  try:
    with open(path, 'rb') as fp:
      raw = fp.read()
  except OSError as e:
    print('%s: %s' % (path, e.strerror), file=sys.stderr)

    return []

  try:
    text = raw.decode('utf-8')
  except UnicodeDecodeError:
    print('%s: not UTF-8' % path, file=sys.stderr)

    return []

  if text.startswith(BOM) == True: text = text[len(BOM):]

  text = text.replace('\r\n', '\n')

  if text.endswith('\n') == True: text = text[:-1]

  lines = text.split('\n')

  expect = KEYMAP_HEIGHT * KEYMAP_LEVELS

  if len(lines) != expect:
    print('%s: %d lines, expected %d' % (path, len(lines), expect), file=sys.stderr)

    return []

  for i in range(len(lines)):
    if len(lines[i]) <= KEYMAP_WIDTH: continue

    print('%s: line %d holds %d keys, at most %d fit' % (
      path, i + 1, len(lines[i]), KEYMAP_WIDTH), file=sys.stderr)

    return []

  return lines

USAGE = '''usage: kwp2base.py <keymap>

Writes the base-character file for one keymap to standard output. It holds every character the
keymap carries, in the order the keymap reads, which is what a walk on that layout can start
from.

  kwp2base.py keymaps/en-us.keymap > basechars/en-us.base

Each shipped base-character file is the output of this script on the keymap beside it, so the
whole set rebuilds with a loop:

  for f in keymaps/*.keymap; do
    kwp2base.py "$f" > "basechars/$(basename "$f" .keymap).base"
  done
'''

def main() -> int:
  args = sys.argv[1:]

  if ('--help' in args) or ('-h' in args):
    print(USAGE, end='')

    return 0

  if len(args) != 1:
    print(USAGE, end='', file=sys.stderr)

    return 1

  lines = load(args[0])

  if len(lines) == 0: return 1

  sys.stdout.write(basechars(lines) + '\n')

  return 0

if __name__ == '__main__':
  sys.exit(main())
