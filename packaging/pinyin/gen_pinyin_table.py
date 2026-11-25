#!/usr/bin/env python3
"""Turn pinyin-data's pinyin.txt into a compact first-letter table.

Output: a C header with one byte per codepoint over the CJK Unified Ideographs
block (U+4E00..U+9FA5). 0 means "no reading", otherwise the value is the ASCII
first letter of the character's first listed pronunciation.

    gen_pinyin_table.py pinyin.txt out.h
"""
import re
import unicodedata
import sys

FIRST = 0x4E00
LAST = 0x9FA5

LINE = re.compile(r'^U\+([0-9A-Fa-f]{4,6}):\s*([^\s#]+)')


def main():
    if len(sys.argv) != 3:
        print(__doc__.strip())
        return 2
    src, out = sys.argv[1], sys.argv[2]

    table = bytearray(LAST - FIRST + 1)
    filled = 0
    with open(src, encoding='utf-8') as f:
        for line in f:
            m = LINE.match(line)
            if not m:
                continue
            cp = int(m.group(1), 16)
            if cp < FIRST or cp > LAST:
                continue
            reading = m.group(2).split(',')[0].strip()
            if not reading:
                continue
            # Readings carry tone marks, and the first syllable of e.g. 阿 is
            # "ā": normalise the diacritics away (ā -> a + combining macron,
            # ü -> u + diaeresis) and take the first ASCII letter. Checking the
            # raw first character dropped every vowel-initial reading, so 阿,
            # 爱, 安 and friends had no entry at all.
            base = unicodedata.normalize('NFKD', reading)
            letter = ''
            for ch in base:
                low = ch.lower()
                if 'a' <= low <= 'z':
                    letter = low
                    break
            if letter:
                table[cp - FIRST] = ord(letter)
                filled += 1

    with open(out, 'w', encoding='utf-8', newline='\n') as f:
        f.write('/* Generated from pinyin-data pinyin.txt by gen_pinyin_table.py.\n')
        f.write(' * One byte per codepoint in U+%04X..U+%04X: 0 = no reading,\n' % (FIRST, LAST))
        f.write(' * otherwise the ASCII first letter of the first pronunciation. */\n')
        f.write('#ifndef PINYIN_TABLE_H\n#define PINYIN_TABLE_H\n\n')
        f.write('#define PINYIN_TABLE_FIRST 0x%04X\n' % FIRST)
        f.write('#define PINYIN_TABLE_LAST  0x%04X\n\n' % LAST)
        f.write('static const unsigned char pinyin_initial_table[] = {\n')
        for i in range(0, len(table), 16):
            chunk = table[i:i + 16]
            f.write('    ' + ','.join('%3d' % b for b in chunk) + ',\n')
        f.write('};\n\n#endif /* PINYIN_TABLE_H */\n')

    print('mapped %d of %d codepoints (%.0f%%), %d bytes of table'
          % (filled, len(table), 100.0 * filled / len(table), len(table)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
