// source/render.cpp
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>
#include <math.h>

extern gcmContextData *ctx;

void Game::draw_rect(int x, int y, int w, int h, int cor) {
    (void)x; (void)y; (void)w; (void)h; (void)cor;
    if (!ctx) return;
    rsxSetColor(ctx, cor);
    rsxDrawQuad(ctx, x, y, w, h);
}

void Game::draw_circle(int x, int y, int r, int cor) {
    (void)x; (void)y; (void)r; (void)cor;
    if (!ctx) return;
    rsxSetColor(ctx, cor);
    rsxDrawQuad(ctx, x - r, y - r, r*2, r*2);
}

void Game::draw_bean(int x, int y, int cor, bool vivo, bool impostor) {
    if (!vivo) {
        draw_rect(x - 18, y + 10, 36, 10, 0xFFAAAAAA);
        draw_circle(x, y - 5, 16, cor);
        return;
    }
    draw_rect(x - 18, y - 10, 36, 38, cor);
    draw_circle(x, y - 12, 20, cor);
    draw_rect(x - 12, y - 18, 24, 12, 0xFFB0E0FF);
    draw_rect(x - 8, y - 16, 16, 8, 0xFF000000);
    draw_rect(x - 14, y + 28, 10, 12, cor);
    draw_rect(x + 4, y + 28, 10, 12, cor);
    if (impostor) {
        draw_rect(x - 4, y - 22, 8, 6, 0xFFFF0000);
    }
}

void Game::draw_text(int x, int y, const char* texto) {
    (void)x; (void)y; (void)texto;
}

void Game::draw_map() {
    draw_rect(0, 0, 1280, 720, 0xFF1A1A2E);
    draw_rect(40, 40, 1200, 640, 0xFF2D2D44);
    draw_rect(40, 40, 1200, 30, 0xFF3A3A5C);
    draw_rect(40, 650, 1200, 30, 0xFF3A3A5C);
    draw_rect(250, 120, 180, 120, 0xFF444466);
    draw_rect(850, 120, 180, 120, 0xFF444466);
    draw_rect(250, 480, 180, 120, 0xFF444466);
    draw_rect(850, 480, 180, 120, 0xFF444466);
    draw_rect(580, 80, 120, 80, 0xFF555577);
    draw_rect(580, 560, 120, 80, 0xFF555577);
    draw_rect(580, 300, 120, 120, 0xFFFF0000);
    draw_rect(600, 320, 80, 80, 0xFFFFAA00);
    for (size_t i = 0; i < tasks.size(); i++) {
        if (tasks[i].done) draw_circle(tasks[i].x, tasks[i].y, 18, 0xFF00FF00);
        else draw_circle(tasks[i].x, tasks[i].y, 18, 0xFFFFFF00);
    }
    for (size_t i = 0; i < bodies.size(); i++) {
        draw_bean(bodies[i].x, bodies[i].y, bodies[i].cor, false, false);
    }
}

void Game::draw_hud() {
    char buf[128];
    sprintf(buf, "TAREFAS: %d / %d", tasksDoneGlobal, tasksTotal);
    draw_text(20, 20, buf);
    sprintf(buf, "VOCE: TRIPULANTE");
    draw_text(20, 45, buf);
    if (!player.vivo) {
        draw_text(500, 350, "VOCE FOI ELIMINADO");
    }
    if (player.vivo) {
        for (size_t i = 0; i < tasks.size(); i++) {
            float dx = tasks[i].x - player.x;
            float dy = tasks[i].y - player.y;
            if (sqrt(dx*dx + dy*dy) < 70 && !tasks[i].done) {
                draw_text(480, 680, "APERTE X PARA FAZER TAREFA");
            }
        }
        for (size_t i = 0; i < bodies.size(); i++) {
            if (bodies[i].reported) continue;
            float dx = bodies[i].x - player.x;
            float dy = bodies[i].y - player.y;
            if (sqrt(dx*dx + dy*dy) < 80) {
                draw_text(480, 680, "APERTE X PARA REPORTAR CORPO");
            }
        }
    }
    draw_text(1050, 20, "TRIANGULO: EMERGENCIA");
}

void Game::draw_meeting() {
    draw_rect(0, 0, 1280, 720, 0xFF000000);
    draw_text(450, 80, "REUNIAO DE EMERGENCIA");
    draw_text(420, 130, "QUEM E O IMPOSTOR? VOTANDO...");
    char buf[64];
    sprintf(buf, "TEMPO: %d", voteTimer / 60);
    draw_text(550, 170, buf);
    for (size_t i = 0; i < bots.size(); i++) {
        int bx = 150 + (i % 5) * 220;
        int by = 250 + (i / 5) * 160;
        draw_bean(bx, by, bots[i].cor, bots[i].vivo, false);
    }
    draw_bean(150, 570, player.cor, player.vivo, false);
}

void Game::render_game() {
    if (!ctx) return;
    rsxSetClearColor(ctx, 0xFF1A1A2E);
    rsxClearSurface(ctx, GCM_CLEAR_R | GCM_CLEAR_G | GCM_CLEAR_B | GCM_CLEAR_A | GCM_CLEAR_S | GCM_CLEAR_Z);
    if (state == 1) {
        draw_meeting();
    } else if (state == 3) {
        draw_rect(0, 0, 1280, 720, 0xFF00AA00);
        draw_text(450, 300, "TRIPULANTES VENCERAM!");
        draw_text(430, 340, "APERTE X PARA JOGAR DE NOVO");
    } else if (state == 4) {
        draw_rect(0, 0, 1280, 720, 0xFFAA0000);
        draw_text(450, 300, "IMPOSTOR VENCEU!");
        draw_text(430, 340, "APERTE X PARA JOGAR DE NOVO");
    } else {
        draw_map();
        for (size_t i = 0; i < bots.size(); i++) {
            draw_bean(bots[i].x, bots[i].y, bots[i].cor, bots[i].vivo, false);
        }
        if (player.vivo) draw_bean(player.x, player.y, player.cor, true, false);
        draw_hud();
    }
    rsxFlip(ctx, 0);
}