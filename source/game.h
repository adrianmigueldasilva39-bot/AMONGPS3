// game.h
#ifndef GAME_H
#define GAME_H

#include <stdint.h>

#define MAX_CREW 10
#define MAX_TASKS 8
#define MAX_WALLS 32
#define MAX_TERMINALS 6

typedef enum {
    ROLE_CREWMATE,
    ROLE_IMPOSTOR
} Role;

typedef enum {
    GAME_STATE_PLAYING,
    GAME_STATE_VOTING,
    GAME_STATE_VICTORY_CREW,
    GAME_STATE_VICTORY_IMPOSTOR,
    GAME_STATE_MENU
} GameState;

typedef struct {
    float x, z;
    float angle;
    float walkPhase;
    int alive;
    Role role;
    uint32_t color;
    float speed;
    int isBot;
    float taskProgress;
    int targetTask;
    float attackCooldown;
    int votedFor;
} Crew;

typedef struct {
    float x, z;
    float width, depth;
    float height;
} Wall;

typedef struct {
    float x, z;
    int completed;
    float progress;
    int assignedTo;
} TaskTerminal;

typedef struct {
    float x, z;
    float radius;
} Table;

typedef struct {
    Wall walls[MAX_WALLS];
    int wallCount;
    TaskTerminal terminals[MAX_TERMINALS];
    int terminalCount;
    Crew crew[MAX_CREW];
    int crewCount;
    Table table;
    float emergencyButtonX, emergencyButtonZ;
    int emergencyButtonPressed;
    GameState state;
    int lightsOn;
    float sabotageTimer;
    float meetingTimer;
    int ejectedIndex;
    int voteSkipCount;
    int votes[MAX_CREW];
    int playerIndex;
    float worldTime;
    float taskBarProgress;
} Game;

extern Game g_game;

void gameInit(int playerIsImpostor);
void gameUpdate(float deltaTime, int btnCross, int btnSquare, int btnTriangle, int btnCircle, int btnStart, float analogX, float analogZ);
void gameRender(void);
void gameReset(int playerIsImpostor);

#endif