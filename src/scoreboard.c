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

        // Format: name;score;game_mode
        char *sep1 = strchr(line, ';');
        if (!sep1) continue;

        *sep1 = '\0';
        char *name = line;
        char *scoreStr = sep1 + 1;

        char *sep2 = strchr(scoreStr, ';');
        ScoreGameMode game_mode = SCORE_MODE_CLASSIC; // Default
        if (sep2) {
            *sep2 = '\0';
            char *modeStr = sep2 + 1;
            game_mode = (ScoreGameMode)atoi(modeStr);
        }

        int score = atoi(scoreStr);

        scoreboard_reserve(sb, sb->count + 1);
        ScoreEntry *e = &sb->entries[sb->count++];
        strncpy(e->name, name, SB_MAX_NAME_LEN - 1);
        e->name[SB_MAX_NAME_LEN - 1] = '\0';
        e->score = score;
        e->game_mode = game_mode;
    }

    fclose(f);
}

void scoreboard_save(const Scoreboard *sb) {
    FILE *f = fopen(sb->filename, "w");
    if (!f) return;

    for (int i = 0; i < sb->count; ++i) {
        const ScoreEntry *e = &sb->entries[i];
        fprintf(f, "%s;%d;%d\n", e->name, e->score, (int)e->game_mode);
    }

    fclose(f);
}

void scoreboard_add(Scoreboard *sb, const char *name, int score, ScoreGameMode game_mode) {
    scoreboard_reserve(sb, sb->count + 1);
    ScoreEntry *e = &sb->entries[sb->count++];
    strncpy(e->name, name, SB_MAX_NAME_LEN - 1);
    e->name[SB_MAX_NAME_LEN - 1] = '\0';
    e->score = score;
    e->game_mode = game_mode;
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

void scoreboard_trim_to_top_n(Scoreboard *sb, int max_entries_per_mode) {
    if (sb->count == 0) return;

    // Count entries per mode
    int classic_count = 0;
    int modern_count = 0;

    // First pass: count entries for each mode (assuming sorted)
    for (int i = 0; i < sb->count; ++i) {
        if (sb->entries[i].game_mode == SCORE_MODE_CLASSIC) {
            classic_count++;
        } else if (sb->entries[i].game_mode == SCORE_MODE_MODERN) {
            modern_count++;
        }
    }

    // Second pass: mark entries to keep
    classic_count = 0;
    modern_count = 0;
    int new_count = 0;

    for (int i = 0; i < sb->count; ++i) {
        int keep = 0;
        if (sb->entries[i].game_mode == SCORE_MODE_CLASSIC) {
            if (classic_count < max_entries_per_mode) {
                keep = 1;
                classic_count++;
            }
        } else if (sb->entries[i].game_mode == SCORE_MODE_MODERN) {
            if (modern_count < max_entries_per_mode) {
                keep = 1;
                modern_count++;
            }
        }

        if (keep) {
            if (new_count != i) {
                sb->entries[new_count] = sb->entries[i];
            }
            new_count++;
        }
    }

    sb->count = new_count;
}

int scoreboard_qualifies_for_top_n(const Scoreboard *sb, int score, ScoreGameMode game_mode, int n) {
    // Count how many entries of the same mode have higher scores
    int better_count = 0;

    for (int i = 0; i < sb->count; ++i) {
        if (sb->entries[i].game_mode == game_mode && sb->entries[i].score > score) {
            better_count++;
        }
    }

    return better_count < n;
}
