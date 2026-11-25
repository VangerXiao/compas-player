#include "pinyin.h"

#include "pinyin_table.h"

#include <ctype.h>
#include <string.h>

unsigned char pinyin_initial_for(unsigned int codepoint) {
    if (codepoint < PINYIN_TABLE_FIRST || codepoint > PINYIN_TABLE_LAST) {
        return 0;
    }
    return pinyin_initial_table[codepoint - PINYIN_TABLE_FIRST];
}

bool pinyin_has_non_ascii(const char * text) {
    if (!text) return false;
    for (const unsigned char * p = (const unsigned char *) text; *p; p++) {
        if (*p >= 0x80) return true;
    }
    return false;
}

/* Decodes one UTF-8 sequence and advances *cursor. Returns 0xFFFFFFFF for a
 * byte that does not start a valid sequence (the caller then treats it as an
 * unknown character rather than aborting the string). */
static unsigned int utf8_next(const char ** cursor) {
    const unsigned char * s = (const unsigned char *) *cursor;
    unsigned int lead = s[0];

    if (lead < 0x80) {
        *cursor += 1;
        return lead;
    }
    if ((lead & 0xE0) == 0xC0 && s[1]) {
        *cursor += 2;
        return ((lead & 0x1Fu) << 6) | (s[1] & 0x3Fu);
    }
    if ((lead & 0xF0) == 0xE0 && s[1] && s[2]) {
        *cursor += 3;
        return ((lead & 0x0Fu) << 12) | ((s[1] & 0x3Fu) << 6) | (s[2] & 0x3Fu);
    }
    if ((lead & 0xF8) == 0xF0 && s[1] && s[2] && s[3]) {
        *cursor += 4;
        return ((lead & 0x07u) << 18) | ((s[1] & 0x3Fu) << 12) | ((s[2] & 0x3Fu) << 6) | (s[3] & 0x3Fu);
    }
    *cursor += 1;
    return 0xFFFFFFFFu;
}

size_t pinyin_initials(const char * text, char * out, size_t out_size) {
    if (!text || !out || out_size == 0) return 0;

    size_t written = 0;
    bool pending_space = false;
    const char * cursor = text;

    while (*cursor) {
        unsigned int codepoint = utf8_next(&cursor);
        char emit = 0;

        if (codepoint < 0x80) {
            unsigned char c = (unsigned char) codepoint;
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
                emit = (char) tolower(c);
            } else if (c >= '0' && c <= '9') {
                emit = (char) c;
            } else if (written > 0) {
                pending_space = true;
            }
        } else {
            unsigned char initial = pinyin_initial_for(codepoint);
            if (initial) {
                emit = (char) initial;
            } else if (written > 0) {
                pending_space = true;
            }
        }

        if (!emit) continue;

        if (pending_space && written + 1 < out_size) {
            out[written++] = ' ';
        }
        pending_space = false;

        if (written + 1 >= out_size) break;
        out[written++] = emit;
    }

    out[written] = '\0';
    return written;
}
/* Case-insensitive substring search for an ASCII needle in an ASCII haystack. */
static bool ascii_contains(const char * hay, const char * needle) {
    if (!hay || !needle) return false;
    size_t n = strlen(needle);
    if (n == 0) return true;
    for (const char * h = hay; *h; h++) {
        size_t i = 0;
        while (i < n && h[i] && tolower((unsigned char) h[i]) == tolower((unsigned char) needle[i])) i++;
        if (i == n) return true;
    }
    return false;
}

bool pinyin_matches(const char * text, const char * needle) {
    if (!text || !needle || !needle[0]) return false;
    for (const char * p = needle; *p; p++) {
        if (!isalnum((unsigned char) *p)) return false;
    }
    if (!pinyin_has_non_ascii(text)) return false;

    char initials[128];
    if (pinyin_initials(text, initials, sizeof(initials)) == 0) return false;
    return ascii_contains(initials, needle);
}
bool pinyin_is_initials_query(const char * query) {
    if (!query || !query[0]) return false;
    for (const char * p = query; *p; p++) {
        if (!isalnum((unsigned char) *p)) return false;
    }
    return true;
}