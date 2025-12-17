#include "scoreboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void scoreboard_reserve(Scoreboard *sb, int needed) {
    if (sb->capacity >= needed) return;
    int newCap = sb->capacity == 0 ? 16 : sb->capacity * 2;
    if (newCap < needed) newCap = needed;

    ScoreEntry *newEntries = realloc(sb->entries, newCap * sizeof(ScoreEntry));
    if (!newEntries) return;

    sb->entries = newEntries;
    sb->capacity = newCap;
}

void scoreboard_init(Scoreboard *sb, const char *filename) {
    sb->entries = NULL;
    sb->count = 0;
    sb->capacity = 0;
    strncpy(sb->filename, filename, sizeof(sb->filename) - 1);
    sb->filename[sizeof(sb->filename) - 1] = '\0';
}

void scoreboard_load(Scoreboard *sb) {
    sb->count = 0;

    FILE *f = fopen(sb->filename, "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';

        char *sep = strchr(line, ';');
        if (!sep) continue;

        *sep = '\0';
        char *name = line;
        char *scoreStr = sep + 1;
        int score = atoi(scoreStr);

        scoreboard_reserve(sb, sb->count + 1);
        ScoreEntry *e = &sb->entries[sb->count++];
        strncpy(e->name, name, SB_MAX_NAME_LEN - 1);
        e->name[SB_MAX_NAME_LEN - 1] = '\0';
        e->score = score;
    }

    fclose(f);
}

void scoreboard_save(const Scoreboard *sb) {
    FILE *f = fopen(sb->filename, "w");
    if (!f) return;

    for (int i = 0; i < sb->count; ++i) {
        const ScoreEntry *e = &sb->entries[i];
        fprintf(f, "%s;%d\n", e->name, e->score);
    }

    fclose(f);
}

void scoreboard_add(Scoreboard *sb, const char *name, int score) {
    scoreboard_reserve(sb, sb->count + 1);
    ScoreEntry *e = &sb->entries[sb->count++];
    strncpy(e->name, name, SB_MAX_NAME_LEN - 1);
    e->name[SB_MAX_NAME_LEN - 1] = '\0';
    e->score = score;
}

static int compare_entries_desc(const void *a, const void *b) {
    const ScoreEntry *ea = (const ScoreEntry *)a;
    const ScoreEntry *eb = (const ScoreEntry *)b;
    if (ea->score < eb->score) return 1;
    if (ea->score > eb->score) return -1;
    return 0;
}

void scoreboard_sort(Scoreboard *sb) {
    if (sb->count > 1) {
        qsort(sb->entries, sb->count, sizeof(ScoreEntry), compare_entries_desc);
    }
}

void scoreboard_free(Scoreboard *sb) {
    free(sb->entries);
    sb->entries = NULL;
    sb->count = 0;
    sb->capacity = 0;
}
