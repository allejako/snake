#ifndef SCOREBOARD_H
#define SCOREBOARD_H

#define SB_MAX_NAME_LEN 32

typedef struct {
    char name[SB_MAX_NAME_LEN];
    int score;
} ScoreEntry;

typedef struct {
    ScoreEntry *entries;
    int count;
    int capacity;
    char filename[256];
} Scoreboard;

void scoreboard_init(Scoreboard *sb, const char *filename);
void scoreboard_load(Scoreboard *sb);
void scoreboard_save(const Scoreboard *sb);
void scoreboard_add(Scoreboard *sb, const char *name, int score);
void scoreboard_sort(Scoreboard *sb);
void scoreboard_trim_to_top_n(Scoreboard *sb, int n);
int scoreboard_qualifies_for_top_n(const Scoreboard *sb, int score, int n);
void scoreboard_free(Scoreboard *sb);

#endif
