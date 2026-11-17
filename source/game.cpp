// game.cpp
#include "game.h"
#include "render.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

Game g_game;

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float distance2D(float ax, float az, float bx, float bz) {
    float dx = bx - ax;
    float dz = bz - az;
    return sqrtf(dx * dx + dz * dz);
}

static int checkWallCollision(float x, float z, float radius) {
    int i;
    for (i = 0; i < g_game.wallCount; i++) {
        Wall *w = &g_game.walls[i];
        float halfW = w->width * 0.5f + radius;
        float halfD = w->depth * 0.5f + radius;
        if (x > w->x - halfW && x < w->x + halfW && z > w->z - halfD && z < w->z + halfD) {
            return 1;
        }
    }
    return 0;
}

static void moveCrew(Crew *c, float dx, float dz, float dt) {
    float newX = c->x + dx * c->speed * dt;
    float newZ = c->z + dz * c->speed * dt;
    if (!checkWallCollision(newX, c->z, 0.4f)) c->x = newX;
    if (!checkWallCollision(c->x, newZ, 0.4f)) c->z = newZ;
    c->x = clampf(c->x, -18.0f, 18.0f);
    c->z = clampf(c->z, -12.0f, 12.0f);
    if (fabsf(dx) > 0.01f || fabsf(dz) > 0.01f) {
        c->walkPhase += dt * 8.0f;
        c->angle = atan2f(dx, dz);
    }
}

static void initMap(void) {
    g_game.wallCount = 0;
    g_game.walls[g_game.wallCount++] = (Wall){0, -12, 40, 1, 3};
    g_game.walls[g_game.wallCount++] = (Wall){0, 12, 40, 1, 3};
    g_game.walls[g_game.wallCount++] = (Wall){-18, 0, 1, 24, 3};
    g_game.walls[g_game.wallCount++] = (Wall){18, 0, 1, 24, 3};
    g_game.walls[g_game.wallCount++] = (Wall){-6, -5, 1, 10, 3};
    g_game.walls[g_game.wallCount++] = (Wall){6, -5, 1, 10, 3};
    g_game.walls[g_game.wallCount++] = (Wall){-6, 5, 1, 8, 3};
    g_game.walls[g_game.wallCount++] = (Wall){6, 5, 1, 8, 3};
    g_game.walls[g_game.wallCount++] = (Wall){0, 7, 10, 1, 3};
    g_game.walls[g_game.wallCount++] = (Wall){-12, 0, 1, 4, 3};
    g_game.walls[g_game.wallCount++] = (Wall){12, 0, 1, 4, 3};

    g_game.terminalCount = 6;
    g_game.terminals[0] = (TaskTerminal){-14, -9, 0, 0, -1};
    g_game.terminals[1] = (TaskTerminal){14, -9, 0, 0, -1};
    g_game.terminals[2] = (TaskTerminal){-14, 9, 0, 0, -1};
    g_game.terminals[3] = (TaskTerminal){14, 9, 0, 0, -1};
    g_game.terminals[4] = (TaskTerminal){0, -9, 0, 0, -1};
    g_game.terminals[5] = (TaskTerminal){0, 9, 0, 0, -1};

    g_game.table.x = 0;
    g_game.table.z = 0;
    g_game.table.radius = 2.5f;
    g_game.emergencyButtonX = 0;
    g_game.emergencyButtonZ = 0;
}

static void assignBotTasks(void) {
    int i, j;
    for (i = 0; i < g_game.crewCount; i++) {
        if (g_game.crew[i].isBot && g_game.crew[i].alive) {
            g_game.crew[i].targetTask = -1;
            for (j = 0; j < g_game.terminalCount; j++) {
                if (!g_game.terminals[j].completed && g_game.terminals[j].assignedTo < 0) {
                    g_game.terminals[j].assignedTo = i;
                    g_game.crew[i].targetTask = j;
                    break;
                }
            }
        }
    }
}

void gameInit(int playerIsImpostor) {
    int i;
    memset(&g_game, 0, sizeof(Game));
    g_game.state = GAME_STATE_PLAYING;
    g_game.lightsOn = 1;
    g_game.sabotageTimer = 0;
    g_game.meetingTimer = 0;
    g_game.ejectedIndex = -1;
    g_game.playerIndex = 0;
    g_game.crewCount = MAX_CREW;
    g_game.worldTime = 0;
    g_game.taskBarProgress = 0;

    initMap();

    for (i = 0; i < MAX_CREW; i++) {
        Crew *c = &g_game.crew[i];
        c->alive = 1;
        c->isBot = (i != g_game.playerIndex);
        c->role = ROLE_CREWMATE;
        c->taskProgress = 0;
        c->targetTask = -1;
        c->attackCooldown = 0;
        c->votedFor = -1;
        c->walkPhase = 0;
        c->angle = 0;
        c->speed = 3.5f;
        if (i == g_game.playerIndex) {
            c->x = 0;
            c->z = 2;
            c->role = playerIsImpostor ? ROLE_IMPOSTOR : ROLE_CREWMATE;
            c->color = 0xFF3333;
        } else {
            c->x = -12 + (i % 5) * 6;
            c->z = -8 + (i / 5) * 6;
            c->role = ROLE_CREWMATE;
            switch (i) {
                case 1: c->color = 0x33FF33; break;
                case 2: c->color = 0x3333FF; break;
                case 3: c->color = 0xFFFF33; break;
                case 4: c->color = 0xFF33FF; break;
                case 5: c->color = 0x33FFFF; break;
                case 6: c->color = 0xFF8833; break;
                case 7: c->color = 0x8833FF; break;
                case 8: c->color = 0x33FF88; break;
                case 9: c->color = 0xFFFFFF; break;
            }
        }
    }

    if (!playerIsImpostor) {
        int impostorCount = 0;
        for (i = 1; i < MAX_CREW && impostorCount < 2; i++) {
            if ((i * 7 + 3) % 5 == 0) {
                g_game.crew[i].role = ROLE_IMPOSTOR;
                impostorCount++;
            }
        }
    } else {
        int crewImpostor = 1;
        for (i = 1; i < MAX_CREW && crewImpostor < 2; i++) {
            if ((i * 11 + 5) % 7 == 0) {
                g_game.crew[i].role = ROLE_IMPOSTOR;
                crewImpostor++;
            }
        }
    }

    assignBotTasks();
}

static void updateBots(float dt) {
    int i, j;
    for (i = 0; i < g_game.crewCount; i++) {
        Crew *c = &g_game.crew[i];
        if (!c->isBot || !c->alive) continue;

        if (c->role == ROLE_CREWMATE) {
            if (c->targetTask >= 0 && !g_game.terminals[c->targetTask].completed) {
                TaskTerminal *t = &g_game.terminals[c->targetTask];
                float dx = t->x - c->x;
                float dz = t->z - c->z;
                float dist = sqrtf(dx * dx + dz * dz);
                if (dist > 0.8f) {
                    moveCrew(c, dx / dist, dz / dist, dt);
                } else {
                    t->progress += dt * 0.4f;
                    c->taskProgress = t->progress;
                    if (t->progress >= 1.0f) {
                        t->completed = 1;
                        t->progress = 1.0f;
                        c->taskProgress = 1.0f;
                        c->targetTask = -1;
                        for (j = 0; j < g_game.terminalCount; j++) {
                            if (!g_game.terminals[j].completed && g_game.terminals[j].assignedTo < 0) {
                                g_game.terminals[j].assignedTo = i;
                                c->targetTask = j;
                                break;
                            }
                        }
                    }
                }
            } else {
                for (j = 0; j < g_game.terminalCount; j++) {
                    if (!g_game.terminals[j].completed && g_game.terminals[j].assignedTo < 0) {
                        g_game.terminals[j].assignedTo = i;
                        c->targetTask = j;
                        break;
                    }
                }
            }
        } else if (c->role == ROLE_IMPOSTOR) {
            if (c->attackCooldown > 0) c->attackCooldown -= dt;
            for (j = 0; j < g_game.crewCount; j++) {
                if (j == i || !g_game.crew[j].alive || g_game.crew[j].role == ROLE_IMPOSTOR) continue;
                float dist = distance2D(c->x, c->z, g_game.crew[j].x, g_game.crew[j].z);
                if (dist < 2.0f && c->attackCooldown <= 0) {
                    g_game.crew[j].alive = 0;
                    c->attackCooldown = 5.0f;
                    break;
                }
            }
            if (c->attackCooldown <= 0) {
                float targetX = g_game.crew[g_game.playerIndex].x;
                float targetZ = g_game.crew[g_game.playerIndex].z;
                float dx = targetX - c->x;
                float dz = targetZ - c->z;
                float dist = sqrtf(dx * dx + dz * dz);
                if (dist > 0.5f) moveCrew(c, dx / dist, dz / dist, dt);
            }
        }
    }
}

static void updateTaskProgress(void) {
    int i;
    int total = 0, done = 0;
    for (i = 0; i < g_game.terminalCount; i++) {
        total++;
        if (g_game.terminals[i].completed) done++;
    }
    if (total > 0) g_game.taskBarProgress = (float)done / (float)total;
    else g_game.taskBarProgress = 0;
}

static void checkVictoryConditions(void) {
    int i;
    int aliveCrew = 0, aliveImpostor = 0;
    for (i = 0; i < g_game.crewCount; i++) {
        if (!g_game.crew[i].alive) continue;
        if (g_game.crew[i].role == ROLE_IMPOSTOR) aliveImpostor++;
        else aliveCrew++;
    }
    if (aliveImpostor == 0) {
        g_game.state = GAME_STATE_VICTORY_CREW;
    } else if (aliveImpostor >= aliveCrew) {
        g_game.state = GAME_STATE_VICTORY_IMPOSTOR;
    } else if (g_game.taskBarProgress >= 1.0f) {
        g_game.state = GAME_STATE_VICTORY_CREW;
    }
}

static void triggerMeeting(int reporterIdx) {
    int i;
    g_game.state = GAME_STATE_VOTING;
    g_game.meetingTimer = 0;
    g_game.ejectedIndex = -1;
    g_game.voteSkipCount = 0;
    for (i = 0; i < g_game.crewCount; i++) {
        g_game.votes[i] = -1;
        g_game.crew[i].votedFor = -1;
    }
    if (reporterIdx >= 0) {
        g_game.votes[reporterIdx] = -2;
    }
}

static void resolveVote(void) {
    int i;
    int voteCount[MAX_CREW];
    memset(voteCount, 0, sizeof(voteCount));
    int maxVotes = 0;
    int maxIdx = -1;
    int skipVotes = 0;

    for (i = 0; i < g_game.crewCount; i++) {
        if (!g_game.crew[i].alive) continue;
        int v = g_game.votes[i];
        if (v >= 0 && v < g_game.crewCount && g_game.crew[i].alive) {
            voteCount[v]++;
            if (voteCount[v] > maxVotes) {
                maxVotes = voteCount[v];
                maxIdx = v;
            }
        } else if (v == -3) {
            skipVotes++;
        }
    }

    if (maxVotes > 0 && maxIdx >= 0 && maxVotes > skipVotes) {
        g_game.ejectedIndex = maxIdx;
        g_game.crew[maxIdx].alive = 0;
    } else {
        g_game.ejectedIndex = -1;
    }

    g_game.state = GAME_STATE_PLAYING;
    checkVictoryConditions();
    if (g_game.state == GAME_STATE_PLAYING) {
        assignBotTasks();
    }
}

static void drawCrew(Crew *c, float dt) {
    float bx = c->x;
    float bz = c->z;
    float legSwing = sinf(c->walkPhase) * 0.15f;
    uint32_t col = c->color;
    uint32_t dark = (col >> 1) & 0x7F7F7F;
    uint32_t light = col | 0x404040;

    float bodyR = 0.45f;
    float bodyH = 1.2f;
    int i;

    for (i = 0; i < 8; i++) {
        float a0 = (float)i / 8.0f * 6.28318f;
        float a1 = (float)(i + 1) / 8.0f * 6.28318f;
        float x0 = bx + cosf(a0) * bodyR;
        float z0 = bz + sinf(a0) * bodyR;
        float x1 = bx + cosf(a1) * bodyR;
        float z1 = bz + sinf(a1) * bodyR;

        Vec3 v0 = {x0, bodyH, z0};
        Vec3 v1 = {x1, bodyH, z1};
        Vec3 v2 = {x1, 0.3f, z1};
        Vec3 v3 = {x0, 0.3f, z0};
        renderFillTriangle(v0, v1, v2, col, 0.3f, 0.8f, 0.5f);
        renderFillTriangle(v0, v2, v3, col, 0.3f, 0.8f, 0.5f);

        Vec3 top0 = {bx + cosf(a0) * bodyR * 0.7f, bodyH + 0.3f, bz + sinf(a0) * bodyR * 0.7f};
        Vec3 top1 = {bx + cosf(a1) * bodyR * 0.7f, bodyH + 0.3f, bz + sinf(a1) * bodyR * 0.7f};
        renderFillTriangle(v0, v1, top1, light, 0.5f, 0.9f, 0.2f);
        renderFillTriangle(v0, top1, top0, light, 0.5f, 0.9f, 0.2f);

        Vec3 cap0 = {bx + cosf(a0) * bodyR * 0.4f, bodyH + 0.55f, bz + sinf(a0) * bodyR * 0.4f};
        Vec3 cap1 = {bx + cosf(a1) * bodyR * 0.4f, bodyH + 0.55f, bz + sinf(a1) * bodyR * 0.4f};
        renderFillTriangle(top0, top1, cap1, light, 0.5f, 0.9f, 0.2f);
        renderFillTriangle(top0, cap1, cap0, light, 0.5f, 0.9f, 0.2f);
    }

    float legOffX = cosf(c->angle) * 0.25f;
    float legOffZ = sinf(c->angle) * 0.25f;
    Vec3 l0 = {bx - legOffX, 0.3f, bz - legOffZ};
    Vec3 l1 = {bx + legOffX, 0.3f, bz + legOffZ};
    Vec3 l2 = {bx + legOffX + legSwing, 0.0f, bz + legOffZ};
    Vec3 l3 = {bx - legOffX + legSwing, 0.0f, bz - legOffZ};
    renderFillTriangle(l0, l1, l2, dark, 0.4f, 0.4f, 0.8f);
    renderFillTriangle(l0, l2, l3, dark, 0.4f, 0.4f, 0.8f);

    Vec3 back0 = {bx - cosf(c->angle) * 0.5f - 0.2f, bodyH - 0.2f, bz - sinf(c->angle) * 0.5f - 0.2f};
    Vec3 back1 = {bx - cosf(c->angle) * 0.5f + 0.2f, bodyH - 0.2f, bz - sinf(c->angle) * 0.5f + 0.2f};
    Vec3 back2 = {bx - cosf(c->angle) * 0.5f + 0.2f, bodyH + 0.5f, bz - sinf(c->angle) * 0.5f + 0.2f};
    Vec3 back3 = {bx - cosf(c->angle) * 0.5f - 0.2f, bodyH + 0.5f, bz - sinf(c->angle) * 0.5f - 0.2f};
    renderFillTriangle(back0, back1, back2, dark, 0.2f, 0.3f, 0.9f);
    renderFillTriangle(back0, back2, back3, dark, 0.2f, 0.3f, 0.9f);

    float visX = bx + cosf(c->angle) * 0.5f;
    float visZ = bz + sinf(c->angle) * 0.5f;
    Vec3 vA = {visX - 0.15f, 1.1f, visZ - 0.15f};
    Vec3 vB = {visX + 0.15f, 1.1f, visZ + 0.15f};
    Vec3 vC = {visX + 0.15f, 0.9f, visZ + 0.15f};
    Vec3 vD = {visX - 0.15f, 0.9f, visZ - 0.15f};
    renderFillTriangle(vA, vB, vC, 0x00FFFF, 0.6f, 0.6f, 0.2f);
    renderFillTriangle(vA, vC, vD, 0x00FFFF, 0.6f, 0.6f, 0.2f);
}

static void drawMap(void) {
    int i;
    float tileSize = 2.0f;
    int tx, tz;
    for (tx = -20; tx < 20; tx++) {
        for (tz = -12; tz < 12; tz++) {
            float x0 = tx * tileSize;
            float z0 = tz * tileSize;
            float x1 = x0 + tileSize;
            float z1 = z0 + tileSize;
            uint32_t col = ((tx + tz) & 1) ? 0x555555 : 0x666666;
            Vec3 v0 = {x0, 0, z0};
            Vec3 v1 = {x1, 0, z0};
            Vec3 v2 = {x1, 0, z1};
            Vec3 v3 = {x0, 0, z1};
            renderFillTriangle(v0, v1, v2, col, 0.5f, 0.9f, 0.3f);
            renderFillTriangle(v0, v2, v3, col, 0.5f, 0.9f, 0.3f);
        }
    }

    for (i = 0; i < g_game.wallCount; i++) {
        Wall *w = &g_game.walls[i];
        float hw = w->width * 0.5f;
        float hd = w->depth * 0.5f;
        float h = w->height;
        uint32_t wc = 0x888888;
        Vec3 p0 = {w->x - hw, 0, w->z - hd};
        Vec3 p1 = {w->x + hw, 0, w->z - hd};
        Vec3 p2 = {w->x + hw, 0, w->z + hd};
        Vec3 p3 = {w->x - hw, 0, w->z + hd};
        Vec3 t0 = {w->x - hw, h, w->z - hd};
        Vec3 t1 = {w->x + hw, h, w->z - hd};
        Vec3 t2 = {w->x + hw, h, w->z + hd};
        Vec3 t3 = {w->x - hw, h, w->z + hd};

        renderFillTriangle(p0, p1, t1, wc, 0.4f, 0.8f, 0.5f);
        renderFillTriangle(p0, t1, t0, wc, 0.4f, 0.8f, 0.5f);
        renderFillTriangle(p1, p2, t2, wc, 0.4f, 0.8f, 0.5f);
        renderFillTriangle(p1, t2, t1, wc, 0.4f, 0.8f, 0.5f);
        renderFillTriangle(p2, p3, t3, wc, 0.4f, 0.8f, 0.5f);
        renderFillTriangle(p2, t3, t2, wc, 0.4f, 0.8f, 0.5f);
        renderFillTriangle(t0, t1, t2, 0xAAAAAA, 0.7f, 0.9f, 0.2f);
        renderFillTriangle(t0, t2, t3, 0xAAAAAA, 0.7f, 0.9f, 0.2f);
    }

    {
        float r = g_game.table.radius;
        float cx = g_game.table.x;
        float cz = g_game.table.z;
        Vec3 tc = {cx, 0.9f, cz};
        int seg = 8;
        int s;
        for (s = 0; s < seg; s++) {
            float a0 = (float)s / seg * 6.28318f;
            float a1 = (float)(s + 1) / seg * 6.28318f;
            Vec3 e0 = {cx + cosf(a0) * r, 0.9f, cz + sinf(a0) * r};
            Vec3 e1 = {cx + cosf(a1) * r, 0.9f, cz + sinf(a1) * r};
            renderFillTriangle(tc, e0, e1, 0xAA8844, 0.6f, 0.7f, 0.3f);
            Vec3 b0 = {cx + cosf(a0) * r, 0, cz + sinf(a0) * r};
            Vec3 b1 = {cx + cosf(a1) * r, 0, cz + sinf(a1) * r};
            renderFillTriangle(e0, e1, b1, 0x886633, 0.5f, 0.5f, 0.6f);
            renderFillTriangle(e0, b1, b0, 0x886633, 0.5f, 0.5f, 0.6f);
        }

        Vec3 btn0 = {cx - 0.3f, 1.1f, cz - 0.3f};
        Vec3 btn1 = {cx + 0.3f, 1.1f, cz - 0.3f};
        Vec3 btn2 = {cx + 0.3f, 1.1f, cz + 0.3f};
        Vec3 btn3 = {cx - 0.3f, 1.1f, cz + 0.3f};
        renderFillTriangle(btn0, btn1, btn2, 0xFF0000, 0.7f, 0.7f, 0.3f);
        renderFillTriangle(btn0, btn2, btn3, 0xFF0000, 0.7f, 0.7f, 0.3f);
    }

    for (i = 0; i < g_game.terminalCount; i++) {
        TaskTerminal *t = &g_game.terminals[i];
        uint32_t tc = t->completed ? 0x00FF00 : 0xFF8800;
        float s = 0.5f;
        Vec3 b0 = {t->x - s, 0, t->z - s};
        Vec3 b1 = {t->x + s, 0, t->z - s};
        Vec3 b2 = {t->x + s, 0, t->z + s};
        Vec3 b3 = {t->x - s, 0, t->z + s};
        Vec3 u0 = {t->x - s, 1.2f, t->z - s};
        Vec3 u1 = {t->x + s, 1.2f, t->z - s};
        Vec3 u2 = {t->x + s, 1.2f, t->z + s};
        Vec3 u3 = {t->x - s, 1.2f, t->z + s};
        renderFillTriangle(b0, b1, u1, tc, 0.5f, 0.8f, 0.3f);
        renderFillTriangle(b0, u1, u0, tc, 0.5f, 0.8f, 0.3f);
        renderFillTriangle(b1, b2, u2, tc, 0.5f, 0.8f, 0.3f);
        renderFillTriangle(b1, u2, u1, tc, 0.5f, 0.8f, 0.3f);
        renderFillTriangle(u0, u1, u2, tc, 0.7f, 0.9f, 0.2f);
        renderFillTriangle(u0, u2, u3, tc, 0.7f, 0.9f, 0.2f);
    }
}

static void drawHUD(void) {
    char buf[64];
    renderFillRect(10, 10, 260, 50, 0x222222);
    renderDrawText(20, 18, g_game.crew[g_game.playerIndex].role == ROLE_IMPOSTOR ? "IMPOSTOR" : "CREWMATE", 0xFFFFFF, 2);
    renderFillRect(20, 40, 200, 12, 0x444444);
    int barW = (int)(200 * g_game.taskBarProgress);
    renderFillRect(20, 40, barW, 12, 0x00FF00);

    if (g_game.crew[g_game.playerIndex].role == ROLE_IMPOSTOR) {
        renderFillRect(10, 70, 180, 30, 0x222222);
        sprintf(buf, "COOLDOWN: %.1f", g_game.crew[g_game.playerIndex].attackCooldown);
        renderDrawText(20, 78, buf, 0xFF4444, 1);
        renderDrawText(20, 92, "CIRCLE: SABOTAGE", 0xFFFF00, 1);
    }

    renderFillRect(10, FB_HEIGHT - 60, 400, 50, 0x222222);
    renderDrawText(20, FB_HEIGHT - 50, "X: ACTION  SQUARE: ATTACK", 0xCCCCCC, 1);
    renderDrawText(20, FB_HEIGHT - 35, "TRIANGLE: REPORT  CIRCLE: SABOTAGE", 0xCCCCCC, 1);

    if (!g_game.lightsOn) {
        renderFillRect(0, 0, FB_WIDTH, FB_HEIGHT, 0x000000);
        renderDrawText(FB_WIDTH / 2 - 60, FB_HEIGHT / 2, "LIGHTS OFF", 0xFF0000, 3);
    }
}

static void drawVotingScreen(void) {
    int i;
    char buf[32];
    renderFillRect(100, 80, FB_WIDTH - 200, FB_HEIGHT - 160, 0x1A1A2E);
    renderDrawText(FB_WIDTH / 2 - 100, 100, "EMERGENCY MEETING", 0xFF4444, 3);
    renderDrawText(FB_WIDTH / 2 - 120, 140, "VOTE FOR THE IMPOSTOR", 0xFFFFFF, 2);

    for (i = 0; i < g_game.crewCount; i++) {
        int col = i % 2;
        int row = i / 2;
        int bx = 180 + col * 460;
        int by = 180 + row * 90;
        uint32_t c = g_game.crew[i].color;
        renderFillRect(bx, by, 380, 70, 0x333333);
        renderFillRect(bx + 10, by + 10, 50, 50, c);
        if (!g_game.crew[i].alive) {
            renderDrawText(bx + 20, by + 25, "DEAD", 0xFF0000, 2);
        } else {
            if (i == g_game.playerIndex) {
                renderDrawText(bx + 70, by + 20, "YOU", 0x00FF00, 2);
            } else {
                renderDrawText(bx + 70, by + 20, "CREW", 0xFFFFFF, 2);
            }
        }
        if (g_game.votes[i] == -3) {
            renderDrawText(bx + 150, by + 20, "SKIP", 0x8888FF, 1);
        } else if (g_game.votes[i] >= 0 && g_game.votes[i] < g_game.crewCount) {
            sprintf(buf, "VOTED: %d", g_game.votes[i]);
            renderDrawText(bx + 150, by + 20, buf, 0xFFFF00, 1);
        }
    }

    renderDrawText(FB_WIDTH / 2 - 80, FB_HEIGHT - 120, "X: CONFIRM", 0xFFFFFF, 2);
    renderDrawText(FB_WIDTH / 2 - 80, FB_HEIGHT - 90, "CIRCLE: SKIP", 0xFFFFFF, 2);
}

static void drawVictoryScreen(void) {
    if (g_game.state == GAME_STATE_VICTORY_CREW) {
        renderFillRect(0, 0, FB_WIDTH, FB_HEIGHT, 0x003300);
        renderDrawTextCentered(FB_WIDTH / 2, FB_HEIGHT / 2 - 40, "CREWMATES WIN", 0x00FF00, 4);
    } else {
        renderFillRect(0, 0, FB_WIDTH, FB_HEIGHT, 0x330000);
        renderDrawTextCentered(FB_WIDTH / 2, FB_HEIGHT / 2 - 40, "IMPOSTOR WINS", 0xFF0000, 4);
    }
       renderDrawTextCentered(FB_WIDTH / 2, FB_HEIGHT / 2 + 40, "PRESS START TO RESTART", 0xFFFFFF, 2);
}

void gameUpdate(float deltaTime, int btnCross, int btnSquare, int btnTriangle, int btnCircle, int btnStart, float analogX, float analogZ) {
    int i;
    if (g_game.state == GAME_STATE_VICTORY_CREW || g_game.state == GAME_STATE_VICTORY_IMPOSTOR) {
        if (btnStart) {
            gameReset(g_game.crew[g_game.playerIndex].role == ROLE_IMPOSTOR);
        }
        return;
    }

    if (g_game.state == GAME_STATE_VOTING) {
        g_game.meetingTimer += deltaTime;
        if (g_game.meetingTimer > 1.0f && g_game.ejectedIndex == -1) {
            int playerVote = -1;
            if (btnCircle) playerVote = -3;
            else if (btnCross) {
                int cursor = -1;
                for (i = 0; i < g_game.crewCount; i++) {
                    if (g_game.crew[i].alive && i != g_game.playerIndex) {
                        cursor = i;
                        break;
                    }
                }
                if (cursor >= 0) playerVote = cursor;
            }
            if (playerVote != -1) {
                g_game.votes[g_game.playerIndex] = playerVote;
                for (i = 0; i < g_game.crewCount; i++) {
                    if (g_game.crew[i].isBot && g_game.crew[i].alive && g_game.votes[i] == -1) {
                        if ((i * 13 + 7) % 4 == 0) {
                            g_game.votes[i] = -3;
                        } else {
                            int target = (i + 1) % g_game.crewCount;
                            if (!g_game.crew[target].alive || target == i) {
                                target = -3;
                            }
                            g_game.votes[i] = target;
                        }
                    }
                }
                resolveVote();
            }
        }
        return;
    }

    g_game.worldTime += deltaTime;

    if (!g_game.lightsOn && g_game.sabotageTimer > 0) {
        g_game.sabotageTimer -= deltaTime;
        if (g_game.sabotageTimer <= 0) g_game.lightsOn = 1;
    }

    Crew *p = &g_game.crew[g_game.playerIndex];
    if (!p->alive) return;

    if (fabsf(analogX) > 0.2f || fabsf(analogZ) > 0.2f) {
        float len = sqrtf(analogX * analogX + analogZ * analogZ);
        if (len < 0.001f) len = 1.0f;
        moveCrew(p, analogX / len, analogZ / len, deltaTime);
    }

    if (p->role == ROLE_CREWMATE) {
        if (btnCross) {
            for (i = 0; i < g_game.terminalCount; i++) {
                TaskTerminal *t = &g_game.terminals[i];
                if (t->completed) continue;
                float dist = distance2D(p->x, p->z, t->x, t->z);
                if (dist < 1.5f) {
                    t->progress += deltaTime * 0.5f;
                    if (t->progress >= 1.0f) {
                        t->completed = 1;
                        t->progress = 1.0f;
                    }
                    break;
                }
            }
        }
    }

    if (p->role == ROLE_IMPOSTOR) {
        if (p->attackCooldown > 0) p->attackCooldown -= deltaTime;
        if (btnSquare && p->attackCooldown <= 0) {
            for (i = 0; i < g_game.crewCount; i++) {
                if (i == g_game.playerIndex || !g_game.crew[i].alive) continue;
                if (g_game.crew[i].role == ROLE_IMPOSTOR) continue;
                float dist = distance2D(p->x, p->z, g_game.crew[i].x, g_game.crew[i].z);
                if (dist < 2.0f) {
                    g_game.crew[i].alive = 0;
                    p->attackCooldown = 5.0f;
                    break;
                }
            }
        }
        if (btnCircle && g_game.lightsOn) {
            g_game.lightsOn = 0;
            g_game.sabotageTimer = 10.0f;
        }
    }

    if (btnTriangle) {
        int canReport = 0;
        for (i = 0; i < g_game.crewCount; i++) {
            if (i == g_game.playerIndex || !g_game.crew[i].alive) continue;
            float dist = distance2D(p->x, p->z, g_game.crew[i].x, g_game.crew[i].z);
            if (dist < 2.0f) { canReport = 1; break; }
        }
        float btnDist = distance2D(p->x, p->z, g_game.emergencyButtonX, g_game.emergencyButtonZ);
        if (canReport || btnDist < 2.5f) {
            triggerMeeting(g_game.playerIndex);
            return;
        }
    }

    updateBots(deltaTime);
    updateTaskProgress();
    checkVictoryConditions();
}

void gameReset(int playerIsImpostor) {
    gameInit(playerIsImpostor);
}

void gameRender(void) {
    int i;
    renderClear(0x101010);

    if (g_game.state == GAME_STATE_VICTORY_CREW || g_game.state == GAME_STATE_VICTORY_IMPOSTOR) {
        drawVictoryScreen();
        return;
    }

    if (g_game.state == GAME_STATE_VOTING) {
        drawVotingScreen();
        return;
    }

    renderSetCamera(0.6f, 0.8f, 14.0f, 0, 0, 10.0f);
    drawMap();
    for (i = 0; i < g_game.crewCount; i++) {
        if (g_game.crew[i].alive) {
            drawCrew(&g_game.crew[i], 1.0f / 60.0f);
        }
    }
    drawHUD();
}
