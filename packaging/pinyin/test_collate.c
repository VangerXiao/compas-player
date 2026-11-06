#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "pinyin.h"

/* Mirrors tc_collate_compare(): sort key first, raw text as the tiebreak. */
static int collate(const char * a, const char * b) {
    char ka[128], kb[128];
    pinyin_sort_key(a, ka, sizeof(ka));
    pinyin_sort_key(b, kb, sizeof(kb));
    int c = strcasecmp(ka, kb);
    if (c) return c;
    return strcasecmp(a, b);
}

static int cmp_qsort(const void * a, const void * b) { return collate(*(const char **) a, *(const char **) b); }

int main(void) {
    const char * names[] = {"周杰伦", "邓紫棋", "稻香", "阿杜", "Beatles", "Adele", "青花瓷", "七里香"};
    size_t n = sizeof(names) / sizeof(names[0]);
    int failures = 0;

    printf("keys:\n");
    for (size_t i = 0; i < n; i++) {
        char k[128];
        pinyin_sort_key(names[i], k, sizeof(k));
        printf("  %-10s -> %s\n", names[i], k);
    }

    const char * sorted[8];
    memcpy(sorted, names, sizeof(names));
    qsort(sorted, n, sizeof(sorted[0]), cmp_qsort);
    printf("sorted:\n");
    for (size_t i = 0; i < n; i++) printf("  %s\n", sorted[i]);

    /* 稻香 must sit in the D block, 青花瓷/七里香 in Q, 阿杜 before the B names. */
    int d_dao = -1, d_deng = -1, q_qing = -1, a_adu = -1, b_beatles = -1;
    for (size_t i = 0; i < n; i++) {
        if (!strcmp(sorted[i], "稻香")) d_dao = (int) i;
        if (!strcmp(sorted[i], "邓紫棋")) d_deng = (int) i;
        if (!strcmp(sorted[i], "青花瓷")) q_qing = (int) i;
        if (!strcmp(sorted[i], "阿杜")) a_adu = (int) i;
        if (!strcmp(sorted[i], "Beatles")) b_beatles = (int) i;
    }
    if (!(d_dao >= 0 && d_dao == d_deng - 1)) { printf("FAIL: 稻香 not before 邓紫棋\n"); failures++; }
    if (q_qing < 0 || sorted[q_qing][0] == '\0') { printf("FAIL: 青花瓷 missing\n"); failures++; }
    if (!(a_adu < b_beatles)) { printf("FAIL: 阿杜 should sort before Beatles\n"); failures++; }
    if (!(q_qing > d_deng)) { printf("FAIL: Q block should follow D\n"); failures++; }

    printf("%s (%d failures)\n", failures ? "FAILED" : "ALL PASS", failures);
    return failures ? 1 : 0;
}
