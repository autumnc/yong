/*
 * yong-status-file.c - Read Yong input method status from /tmp/yong_status
 *
 * The status file is written in GBK encoding by yong.  This program reads it,
 * converts GBK to UTF-8 using a built-in table, and prints to stdout.
 *
 * No external dependencies (no iconv needed).
 *
 * Usage:
 *   yong-status-file          print current status (UTF-8) to stdout
 *   yong-status-file -w       watch mode: poll and print on changes
 *   yong-status-file -j       JSON output with UTF-8 values
 *   yong-status-file -h       help
 *
 * Compile: gcc -O2 -Wall yong-status-file.c -o yong-status-file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STATUS_FILE "/tmp/yong_status"
#define POLL_INTERVAL_MS 200

/* ---- GBK to UTF-8 conversion tables ---- */

typedef struct {
    const char *gbk;
    int         gbk_len;
    const char *utf8;
} gbk_utf8_pair;

/* All GBK strings that can appear in the status file */
static const gbk_utf8_pair gbk_utf8_table[] = {
    /* IM names */
    {"\xd3\xc0\xc2\xeb", 4, "\xe6\xb0\xb8\xe7\xa0\x81"},           /* 永码 */
    {"\xce\xe5\xb1\xca", 4, "\xe4\xba\x94\xe7\xac\x94"},           /* 五笔 */
    {"\xc1\xbd\xb7\xd6", 4, "\xe4\xb8\xa4\xe5\x88\x86"},           /* 两分 */
    {"\xb6\xfe\xb1\xca", 4, "\xe4\xba\x8c\xe7\xac\x94"},           /* 二笔 */
    {"\xc4\xda\xc2\xeb", 4, "\xe5\x86\x85\xe7\xa0\x81"},           /* 内码 */
    {"\xc6\xb4\xd2\xf4", 4, "\xe6\x8b\xbc\xe9\x9f\xb3"},           /* 拼音 */
    /* Language labels */
    {"\xd6\xd0\xce\xc4", 4, "\xe4\xb8\xad\xe6\x96\x87"},           /* 中文 */
    {"\xd3\xa2\xce\xc4", 4, "\xe8\x8b\xb1\xe6\x96\x87"},           /* 英文 */
    /* Corner indicators */
    {"\xa1\xf1",         2, "\xe2\x97\x8f"},                       /* ● full */
    {"\xa1\xf0",         2, "\xe2\x97\x8b"},                       /* ○ half */
};

static int table_count(void)
{
    return sizeof(gbk_utf8_table) / sizeof(gbk_utf8_table[0]);
}

/*
 * Convert a GBK string to UTF-8 by matching against the table.
 * Falls back to raw copy if no match found.
 */
static void gbk_to_utf8_simple(const char *gbk, size_t gbk_len,
                               char *utf8, size_t utf8_size)
{
    /* Try exact match first (whole line) */
    for (int i = 0; i < table_count(); i++) {
        if ((int)gbk_len == gbk_utf8_table[i].gbk_len &&
            memcmp(gbk, gbk_utf8_table[i].gbk, gbk_len) == 0) {
            size_t n = strlen(gbk_utf8_table[i].utf8);
            if (n < utf8_size) {
                memcpy(utf8, gbk_utf8_table[i].utf8, n + 1);
                return;
            }
        }
    }

    /*
     * No full match — try converting piece by piece.
     * Status format: <lang><corner> where lang is a table entry
     * and corner is a 2-byte GBK character at the end.
     */
    if (gbk_len >= 2) {
        /* Try to match last 2 bytes as corner */
        const char *corner_gbk = gbk + gbk_len - 2;
        int corner_found = -1;
        for (int i = 0; i < table_count(); i++) {
            if (gbk_utf8_table[i].gbk_len == 2 &&
                memcmp(corner_gbk, gbk_utf8_table[i].gbk, 2) == 0) {
                corner_found = i;
                break;
            }
        }

        /* Try to match first part as lang */
        size_t lang_len = gbk_len - 2;
        int lang_found = -1;
        if (corner_found >= 0 && lang_len >= 2) {
            for (int i = 0; i < table_count(); i++) {
                if (gbk_utf8_table[i].gbk_len == (int)lang_len &&
                    memcmp(gbk, gbk_utf8_table[i].gbk, lang_len) == 0) {
                    lang_found = i;
                    break;
                }
            }
        }

        if (lang_found >= 0 && corner_found >= 0) {
            size_t llen = strlen(gbk_utf8_table[lang_found].utf8);
            size_t clen = strlen(gbk_utf8_table[corner_found].utf8);
            if (llen + clen < utf8_size) {
                memcpy(utf8, gbk_utf8_table[lang_found].utf8, llen);
                memcpy(utf8 + llen, gbk_utf8_table[corner_found].utf8, clen + 1);
                return;
            }
        }
    }

    /* Fallback: raw copy */
    size_t n = gbk_len < utf8_size - 1 ? gbk_len : utf8_size - 1;
    memcpy(utf8, gbk, n);
    utf8[n] = '\0';
}

static int read_status(char *buf, size_t size)
{
    FILE *fp = fopen(STATUS_FILE, "r");
    if (!fp) return -1;

    if (!fgets(buf, (int)size, fp)) {
        fclose(fp);
        return -1;
    }

    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n')
        buf[len - 1] = '\0';

    fclose(fp);
    return 0;
}

static void print_plain(void)
{
    char buf[256];
    if (read_status(buf, sizeof(buf)) != 0) {
        printf("off\n");
        return;
    }

    char utf8[512];
    gbk_to_utf8_simple(buf, strlen(buf), utf8, sizeof(utf8));
    printf("%s\n", utf8);
}

static void print_json(void)
{
    char buf[256];
    if (read_status(buf, sizeof(buf)) != 0) {
        printf("{\"status\":\"off\"}\n");
        return;
    }

    size_t len = strlen(buf);

    /* Corner: last 2 bytes if they match */
    const char *corner = "unknown";
    size_t lang_len = len;
    if (len >= 2) {
        const char *tail = buf + len - 2;
        if (memcmp(tail, "\xa1\xf1", 2) == 0) {       /* ● full */
            corner = "full";
            lang_len = len - 2;
        } else if (memcmp(tail, "\xa1\xf0", 2) == 0) { /* ○ half */
            corner = "half";
            lang_len = len - 2;
        }
    }

    char lang_utf8[128];
    gbk_to_utf8_simple(buf, lang_len, lang_utf8, sizeof(lang_utf8));

    printf("{\"lang\":\"%s\",\"corner\":\"%s\"}\n", lang_utf8, corner);
}

static int watch_mode(void)
{
    char prev_raw[256] = "";
    char curr_raw[256] = "";
    char prev_disp[512] = "off";

    if (read_status(curr_raw, sizeof(curr_raw)) == 0) {
        gbk_to_utf8_simple(curr_raw, strlen(curr_raw), prev_disp, sizeof(prev_disp));
        printf("%s\n", prev_disp);
        strcpy(prev_raw, curr_raw);
    } else {
        printf("off\n");
        strcpy(prev_raw, "off");
    }
    fflush(stdout);

    for (;;) {
        usleep(POLL_INTERVAL_MS * 1000);

        if (read_status(curr_raw, sizeof(curr_raw)) != 0) {
            if (strcmp(prev_raw, "off") != 0) {
                printf("off\n");
                fflush(stdout);
                strcpy(prev_raw, "off");
                strcpy(prev_disp, "off");
            }
            continue;
        }

        if (strcmp(curr_raw, prev_raw) != 0) {
            gbk_to_utf8_simple(curr_raw, strlen(curr_raw), prev_disp, sizeof(prev_disp));
            printf("%s\n", prev_disp);
            fflush(stdout);
            strcpy(prev_raw, curr_raw);
        }
    }
    return 0;
}

static void print_verbose(void)
{
    char buf[256];
    if (read_status(buf, sizeof(buf)) != 0) {
        printf("file: (not found)\n");
        return;
    }

    printf("raw hex: ");
    for (size_t i = 0; buf[i]; i++)
        printf("%02x ", (unsigned char)buf[i]);
    printf("\n");

    char utf8[512];
    gbk_to_utf8_simple(buf, strlen(buf), utf8, sizeof(utf8));
    printf("utf8:    %s\n", utf8);

    /* Parse and show components */
    size_t len = strlen(buf);
    const char *corner = "unknown";
    size_t lang_len = len;
    if (len >= 2) {
        const char *tail = buf + len - 2;
        if (memcmp(tail, "\xa1\xf1", 2) == 0) {
            corner = "full";
            lang_len = len - 2;
        } else if (memcmp(tail, "\xa1\xf0", 2) == 0) {
            corner = "half";
            lang_len = len - 2;
        }
    }
    char lang_utf8[128];
    gbk_to_utf8_simple(buf, lang_len, lang_utf8, sizeof(lang_utf8));
    printf("parsed:  lang=\"%s\" corner=%s\n", lang_utf8, corner);
}

int main(int argc, char *argv[])
{
    int watch = 0;
    int json = 0;
    int verbose = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-w") || !strcmp(argv[i], "--watch"))
            watch = 1;
        else if (!strcmp(argv[i], "-j") || !strcmp(argv[i], "--json"))
            json = 1;
        else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
            verbose = 1;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            printf("Usage: yong-status-file [-w] [-j] [-v]\n");
            printf("  -w, --watch    Watch mode: poll file and print on changes\n");
            printf("  -j, --json     JSON output format (UTF-8 values)\n");
            printf("  -v, --verbose  Debug: show raw hex and parsed values\n");
            printf("  -h, --help     Show this help\n");
            return 0;
        }
    }

    if (verbose)
        print_verbose();
    else if (watch)
        return watch_mode();
    else if (json)
        print_json();
    else
        print_plain();

    return 0;
}
