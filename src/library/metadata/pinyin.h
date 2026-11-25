#ifndef PINYIN_H
#define PINYIN_H

#include <stdbool.h>
#include <stddef.h>

/* Pinyin initials for the library's search box.
 *
 * The device has no pinyin data of its own (the stock firmware does not ship
 * any), so a first-letter table for the CJK Unified Ideographs block is
 * compiled in -- see pinyin_table.h, generated from pinyin-data by
 * scripts in the fork's repository.
 *
 * What this is for: typing "zjl" should find 周杰伦, the way the search box
 * already finds "jay" in "Jay Chou". Only initials are supported; full pinyin
 * spelling would need several hundred kilobytes of readings and is not
 * compiled in. */

/* True when the string contains at least one byte off the ASCII range, i.e.
 * something that could carry a reading. */
bool pinyin_has_non_ascii(const char * text);

/* Write the initials for text into out:
 *   - ASCII letters are lower-cased and kept,
 *   - digits are kept,
 *   - an ideograph contributes the first letter of its first reading,
 *   - any other character becomes a single space between words, so initials
 *     from different words never run together.
 * Returns the number of bytes written, excluding the terminator. 0 means the
 * string had nothing usable. */
size_t pinyin_initials(const char * text, char * out, size_t out_size);

/* First letter of the character's first reading, or 0 when it has none or the
 * codepoint is outside the compiled table. */
unsigned char pinyin_initial_for(unsigned int codepoint);

/* True when the query is a run of ASCII letters/digits, i.e. something that
 * could be a pinyin-initials query. */
bool pinyin_is_initials_query(const char * query);

/* True when the query, a run of letters/digits, appears in the pinyin initials
 * of text: needle "zjl" matches 周杰伦. False for an empty/非 alphanumeric query
 * or a text with no ideographs, so callers can fall back to a plain substring
 * match first. */
bool pinyin_matches(const char * text, const char * needle);

#endif /* PINYIN_H */
