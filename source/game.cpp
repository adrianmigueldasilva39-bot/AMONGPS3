// source/game.cpp
#include "game.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>

bool Game::init() {
    ioPadInit(7);
    init_game();
    running = true;
    return true;
}

void Game::shutdown() {
    running = false;
    ioPadEnd();
}

void Game::init_game() {
    state = 0;
    timer = 0;
    frame = 0;
    padOldCross = 0;
    padOldTriangle = 0;
    tasksTotal = 6;
    tasksDoneGlobal = 0;
    emergencyCalled = false;
    reportedBody = -1;
    votedOut = -1;
    voteTimer = 0;
    player.x = 640; player.y = 360;
    player.vx = 0; player.vy = 0;
    player.cor = 0xFFFF0000;
    player.id = 0;
    player.vivo = true;
    player.isImpostor = false;
    player.isPlayer = true;
    player.inVent = false;
    player.killCooldown = 0;
    player.taskIndex = 0;
    player.tasksDone = 0;
    player.dir = 1;
    player.moving = false;
    bots.clear();
    bodies.clear();
    tasks.clear();
    int cores[9] = {0xFF0000FF, 0xFF00FF00, 0xFFFFFF00, 0xFFFF00FF, 0xFF00FFFF, 0xFFFF8000, 0xFFFFFFFF, 0xFF808080, 0xFF800080};
    impostorId = 3;
    for (int i = 0; i < 9; i++) {
        PlayerBean b;
        b.x = 200 + (i % 3) * 400;
        b.y = 150 + (i / 3) * 220;
        b.vx = 0; b.vy = 0;
        b.cor = cores[i];
        b.id = i + 1;
        b.vivo = true;
        b.isImpostor = (i == impostorId);
        b.isPlayer = false;
        b.inVent = false;
        b.killCooldown = 0;
        b.taskIndex = 0;
        b.tasksDone = 0;
        b.dir = 1;
        b.moving = false;
        bots.push_back(b);
    }
    Task t0; t0.id = 0; t0.x = 300; t0.y = 150; t0.done = false; t0.nome = "Fios Eletricos"; t0.progresso = 0;
    Task t1; t1.id = 1; t1.x = 950; t1.y = 150; t1.done = false; t1.nome = "Download Dados"; t1.progresso = 0;
    Task t2; t2.id = 2; t2.x = 300; t2.y = 550; t2.done = false; t2.nome = "Combustivel"; t2.progresso = 0;
    Task t3; t3.id = 3; t3.x = 950; t3.y = 550; t3.done = false; t3.nome = "Escudo"; t3.progresso = 0;
    Task t4; t4.id = 4; t4.x = 640; t4.y = 120; t4.done = false; t4.nome = "Reator"; t4.progresso = 0;
    Task t5; t5.id = 5; t5.x = 640; t5.y = 620; t5.done = false; t5.nome = "Oxigenio"; t5.progresso = 0;
    tasks.push_back(t0); tasks.push_back(t1); tasks.push_back(t2); tasks.push_back(t3); tasks.push_back(t4); tasks.push_back(t5);
}

void Game::update_player(int crossPressed, int triPressed, int lx, int ly) {
    if (!player.vivo) return;
    if (state == 1) return;
    float speed = 3.2f;
    player.vx = 0; player.vy = 0;
    player.moving = false;
    if (lx < 100) { player.vx = -speed; player.dir = -1; player.moving = true; }
    if (lx > 150) { player.vx = speed; player.dir = 1; player.moving = true; }
    if (ly < 100) { player.vy = -speed; player.moving = true; }
    if (ly > 150) { player.vy = speed; player.moving = true; }
    player.x += player.vx;
    player.y += player.vy;
    if (player.x < 60) player.x = 60;
    if (player.x > 1220) player.x = 1220;
    if (player.y < 60) player.y = 60;
    if (player.y > 660) player.y = 660;
    if (crossPressed) {
        for (size_t i = 0; i < tasks.size(); i++) {
            float dx = tasks[i].x - player.x;
            float dy = tasks[i].y - player.y;
            if (sqrt(dx*dx + dy*dy) < 70 && !tasks[i].done) {
                do_task(i);
            }
        }
        for (size_t i = 0; i < bodies.size(); i++) {
            if (bodies[i].reported) continue;
            float dx = bodies[i].x - player.x;
            float dy = bodies[i].y - player.y;
            if (sqrt(dx*dx + dy*dy) < 80) {
                report_body(i);
                break;
            }
        }
    }
    if (triPressed) {
        if (!emergencyCalled) {
            emergencyCalled = true;
            start_meeting(-1);
        }
    }
    if (player.killCooldown > 0) player.killCooldown--;
}

void Game::do_task(int idx) {
    tasks[idx].progresso += 25;
    if (tasks[idx].progresso >= 100) {
        tasks[idx].done = true;
        tasks[idx].progresso = 100;
        player.tasksDone++;
        tasksDoneGlobal++;
        if (tasksDoneGlobal >= tasksTotal) {
            state = 3;
        }
    }
}

void Game::update_bots() {
    for (size_t i = 0; i < bots.size(); i++) {
        PlayerBean &b = bots[i];
        if (!b.vivo) continue;
        if (b.isImpostor) continue;
        if (state == 1) continue;
        if (b.taskIndex < (int)tasks.size()) {
            Task &t = tasks[b.taskIndex];
            float dx = t.x - b.x;
            float dy = t.y - b.y;
            float dist = sqrt(dx*dx + dy*dy);
            if (dist < 30) {
                b.tasksDone++;
                tasksDoneGlobal++;
                b.taskIndex++;
                if (tasksDoneGlobal >= tasksTotal) state = 3;
            } else {
                float sp = 1.8f;
                b.x += (dx / dist) * sp;
                b.y += (dy / dist) * sp;
                b.moving = true;
                b.dir = (dx > 0) ? 1 : -1;
            }
        } else {
            b.x += sin(frame * 0.01f + i) * 0.5f;
            b.y += cos(frame * 0.01f + i) * 0.5f;
        }
        if (b.x < 60) b.x = 60;
        if (b.x > 1220) b.x = 1220;
        if (b.y < 60) b.y = 60;
        if (b.y > 660) b.y = 660;
    }
}

void Game::update_impostor() {
    for (size_t i = 0; i < bots.size(); i++) {
        PlayerBean &imp = bots[i];
        if (!imp.isImpostor || !imp.vivo) continue;
        if (state == 1) continue;
        float targetX = player.x;
        float targetY = player.y;
        float best = 999999;
        bool targetIsPlayer = true;
        if (player.vivo) {
            float dx = player.x - imp.x;
            float dy = player.y - imp.y;
            best = sqrt(dx*dx + dy*dy);
        } else {
            best = 999999;
            targetIsPlayer = false;
        }
        for (size_t j = 0; j < bots.size(); j++) {
            if (bots[j].isImpostor || !bots[j].vivo) continue;
            float dx = bots[j].x - imp.x;
            float dy = bots[j].y - imp.y;
            float d = sqrt(dx*dx + dy*dy);
            if (d < best) { best = d; targetX = bots[j].x; targetY = bots[j].y; targetIsPlayer = false; }
        }
        float dx = targetX - imp.x;
        float dy = targetY - imp.y;
        float dist = sqrt(dx*dx + dy*dy);
        if (dist > 1) {
            float sp = 2.6f;
            imp.x += (dx / dist) * sp;
            imp.y += (dy / dist) * sp;
            imp.dir = (dx > 0) ? 1 : -1;
            imp.moving = true;
        }
        if (imp.killCooldown > 0) imp.killCooldown--;
        if (dist < 55 && imp.killCooldown <= 0) {
            if (targetIsPlayer && player.vivo) {
                kill_player(-1);
                imp.killCooldown = 300;
            } else {
                for (size_t j = 0; j < bots.size(); j++) {
                    if (bots[j].isImpostor || !bots[j].vivo) continue;
                    float ddx = bots[j].x - imp.x;
                    float ddy = bots[j].y - imp.y;
                    if (sqrt(ddx*ddx + ddy*ddy) < 55) {
                        bots[j].vivo = false;
                        Body bd; bd.x = bots[j].x; bd.y = bots[j].y; bd.cor = bots[j].cor; bd.reported = false;
                        bodies.push_back(bd);
                        imp.killCooldown = 300;
                        imp.inVent = true;
                        break;
                    }
                }
            }
        }
        if (imp.inVent) {
            imp.x += 4;
            if (imp.x > 1200) { imp.inVent = false; imp.x = 200; }
        }
    }
}

void Game::kill_player(int botIdx) {
    (void)botIdx;
    player.vivo = false;
    Body bd; bd.x = player.x; bd.y = player.y; bd.cor = player.cor; bd.reported = false;
    bodies.push_back(bd);
    state = 4;
}

void Game::report_body(int bodyIdx) {
    bodies[bodyIdx].reported = true;
    start_meeting(bodyIdx);
}

void Game::start_meeting(int bodyIdx) {
    reportedBody = bodyIdx;
    state = 1;
    voteTimer = 600;
}

void Game::vote_logic() {
    voteTimer--;
    if (voteTimer <= 0) {
        int impostorAlive = 0;
        for (size_t i = 0; i < bots.size(); i++) if (bots[i].vivo && bots[i].isImpostor) impostorAlive++;
        if (impostorAlive == 0) { state = 3; return; }
        if (rand() % 100 < 70) {
            for (size_t i = 0; i < bots.size(); i++) {
                if (bots[i].isImpostor && bots[i].vivo) {
                    bots[i].vivo = false;
                    votedOut = bots[i].id;
                    state = 3;
                    return;
                }
            }
        } else {
            int victim = -1;
            for (size_t i = 0; i < bots.size(); i++) if (bots[i].vivo && !bots[i].isImpostor) { victim = i; break; }
            if (victim >= 0) bots[victim].vivo = false;
            state = 0;
            emergencyCalled = false;
            for (size_t i = 0; i < bodies.size(); i++) bodies[i].reported = true;
        }
    }
}

void Game::update_game() {
    frame++;
    timer++;
    ioPadInfo padinfo;
    ioPadData paddata;
    ioPadGetInfo(0, &padinfo);
    int crossNow = 0;
    int triNow = 0;
    int lx = 128;
    int ly = 128;
    int crossPressed = 0;
    int triPressed = 0;
    if (padinfo.status) {
        ioPadGetData(0, &paddata);
        crossNow = (paddata.button[PAD_BUTTON_OFFSET_DIGITAL2] & PAD_CTRL_CROSS) ? 1 : 0;
        triNow = (paddata.button[PAD_BUTTON_OFFSET_DIGITAL2] & PAD_CTRL_TRIANGLE) ? 1 : 0;
        lx = paddata.button[PAD_BUTTON_OFFSET_ANALOG_LEFT_X];
        ly = paddata.button[PAD_BUTTON_OFFSET_ANALOG_LEFT_Y];
        if (crossNow && !padOldCross) crossPressed = 1;
        if (triNow && !padOldTriangle) triPressed = 1;
        padOldCross = crossNow;
        padOldTriangle = triNow;
    }
    if (state == 1) {
        vote_logic();
    } else if (state == 0) {
        update_player(crossPressed, triPressed, lx, ly);
        update_bots();
        update_impostor();
    }
    if (state == 0) {
        int aliveCrew = 0;
        for (size_t i = 0; i < bots.size(); i++) if (bots[i].vivo && !bots[i].isImpostor) aliveCrew++;
        if (player.vivo) aliveCrew++;
        int impAlive = 0;
        for (size_t i = 0; i < bots.size(); i++) if (bots[i].vivo && bots[i].isImpostor) impAlive++;
        if (impAlive == 0) state = 3;
        if (aliveCrew <= 1 && impAlive > 0) state = 4;
    }
    if (state == 3 || state == 4) {
        if (crossPressed) init_game();
    }
}

void Game::run() {
    while (running) {
        sysUtilCheckCallback();
        update_game();
        render_game();
    }
}