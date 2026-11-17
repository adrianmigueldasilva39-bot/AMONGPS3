// IMPOSTOR NA NAVE - jogo inspirado em Among Us para PS3 (PSL1GHT)
// Offline, 1 jogador + 5 bots (1 deles e o impostor).
// Render por software 640x360 -> escala 2x para 1280x720. Mapa e minimapa em cache.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <malloc.h>
#include <unistd.h>
#include <ppu-lv2.h>
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>
#include <io/pad.h>

SYS_PROCESS_PARAM(1001, 0x100000)

#define W 640
#define H 360
#define TILE 16
#define MW 60
#define MH 38
#define WW (MW * TILE)
#define WH (MH * TILE)
#define NP 6
#define NTASK 5
#define FOGR 120

// ------------------------------------------------------------ video
typedef struct { int width, height; uint32_t *ptr; uint32_t offset; } rsxBuffer;
static gcmContextData *context;
static videoResolution vres;
static rsxBuffer buffers[2];
static int curbuf = 0;
static int running = 1;

static void waitFlip(void) {
    while (gcmGetFlipStatus() != 0) usleep(200);
    gcmResetFlipStatus();
}
static int flipBuf(int id) {
    if (gcmSetFlip(context, id) == 0) {
        rsxFlushBuffer(context);
        gcmSetWaitFlip(context);
        return 1;
    }
    return 0;
}
static int makeBuffer(rsxBuffer *b, int w, int h, int id) {
    int pitch = 4 * w;
    b->ptr = (uint32_t *)rsxMemalign(64, h * pitch);
    if (!b->ptr) return 0;
    if (rsxAddressToOffset(b->ptr, &b->offset) != 0) return 0;
    if (gcmSetDisplayBuffer(id, b->offset, pitch, w, h) != 0) return 0;
    b->width = w; b->height = h;
    memset(b->ptr, 0, h * pitch);
    return 1;
}
static void initScreen(void) {
    videoState st;
    videoConfiguration vc;
    void *host = memalign(1024 * 1024, 32 * 1024 * 1024);
    rsxInit(&context, 0x100000, 32 * 1024 * 1024, host);
    videoGetState(0, 0, &st);
    int rid = VIDEO_RESOLUTION_720;
    if (videoGetResolutionAvailability(VIDEO_PRIMARY, VIDEO_RESOLUTION_720, VIDEO_ASPECT_AUTO, 0) != 1)
        rid = st.displayMode.resolution;
    videoGetResolution(rid, &vres);
    memset(&vc, 0, sizeof(vc));
    vc.resolution = rid;
    vc.format = VIDEO_BUFFER_FORMAT_XRGB;
    vc.pitch = vres.width * 4;
    videoConfigure(0, &vc, NULL, 0);
    videoGetState(0, 0, &st);
    gcmSetFlipMode(GCM_FLIP_VSYNC);
    makeBuffer(&buffers[0], vres.width, vres.height, 0);
    makeBuffer(&buffers[1], vres.width, vres.height, 1);
    gcmResetFlipStatus();
    flipBuf(1);
}

static uint32_t fb[W * H];
static uint32_t worldbuf[WW * WH];
static uint32_t mini[MW * 2 * MH * 2];
static int xmap[2048], ymap[2048];

static void present(rsxBuffer *b) {
    uint32_t *dst = b->ptr;
    int dw = b->width, dh = b->height;
    if (dw == W * 2 && dh == H * 2) {
        for (int y = 0; y < H; y++) {
            const uint32_t *s = fb + y * W;
            uint64_t *d0 = (uint64_t *)(dst + (y * 2) * dw);
            uint64_t *d1 = (uint64_t *)(dst + (y * 2 + 1) * dw);
            for (int x = 0; x < W; x++) {
                uint64_t v = ((uint64_t)s[x] << 32) | s[x];
                d0[x] = v; d1[x] = v;
            }
        }
    } else {
        for (int y = 0; y < dh; y++) {
            const uint32_t *s = fb + ymap[y] * W;
            uint32_t *d = dst + y * dw;
            for (int x = 0; x < dw; x++) d[x] = s[xmap[x]];
        }
    }
}

// ------------------------------------------------------------ input
#define B_X 1
#define B_O 2
#define B_SQ 4
#define B_TRI 8
#define B_START 16
static padInfo pinfo;
static padData pdata;
static uint32_t held = 0, prevheld = 0, pressed = 0;
static float ax = 0, ay = 0;

static void readPad(void) {
    ioPadGetInfo(&pinfo);
    held = 0; ax = ay = 0;
    if (pinfo.status[0]) {
        ioPadGetData(0, &pdata);
        if (pdata.BTN_CROSS) held |= B_X;
        if (pdata.BTN_CIRCLE) held |= B_O;
        if (pdata.BTN_SQUARE) held |= B_SQ;
        if (pdata.BTN_TRIANGLE) held |= B_TRI;
        if (pdata.BTN_START) held |= B_START;
        ax = ((int)pdata.ANA_L_H - 128) / 128.0f;
        ay = ((int)pdata.ANA_L_V - 128) / 128.0f;
        if (pdata.BTN_LEFT) ax = -1;
        if (pdata.BTN_RIGHT) ax = 1;
        if (pdata.BTN_UP) ay = -1;
        if (pdata.BTN_DOWN) ay = 1;
        float l = sqrtf(ax * ax + ay * ay);
        if (l < 0.25f) { ax = 0; ay = 0; }
        else if (l > 1.0f) { ax /= l; ay /= l; }
    }
    pressed = held & ~prevheld;
    prevheld = held;
}

static void sysevent(u64 status, u64 param, void *ud) {
    if (status == SYSUTIL_EXIT_GAME) running = 0;
}

// ------------------------------------------------------------ random
static uint32_t rngs = 2463534242u;
static uint32_t rnd(void) { rngs ^= rngs << 13; rngs ^= rngs >> 17; rngs ^= rngs << 5; return rngs; }
#define RI(n) ((int)(rnd() % (unsigned)(n)))

// ------------------------------------------------------------ desenho
static void rect(int x, int y, int w, int h, uint32_t c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    for (int j = y0; j < y1; j++) {
        uint32_t *p = fb + j * W;
        for (int i = x0; i < x1; i++) p[i] = c;
    }
}
static void circle(int cx, int cy, int r, uint32_t c) {
    for (int y = -r; y <= r; y++) {
        int hw = (int)sqrtf((float)(r * r - y * y));
        rect(cx - hw, cy + y, hw * 2 + 1, 1, c);
    }
}

// fonte 5x7 (colunas): 0-9, A-Z, : / - . ! %
static const uint8_t font[42][5] = {
    {0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},
    {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
    {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
    {0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
    {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
    {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
    {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},
    {0x00,0x36,0x36,0x00,0x00},{0x20,0x10,0x08,0x04,0x02},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x00,0x00,0x5F,0x00,0x00},{0x23,0x13,0x08,0x64,0x62}
};
static int glyph(char c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'Z') return 10 + c - 'A';
    switch (c) { case ':': return 36; case '/': return 37; case '-': return 38;
                 case '.': return 39; case '!': return 40; case '%': return 41; }
    return -1;
}
static void text(int x, int y, const char *s, int sc, uint32_t col) {
    if (y >= H || y + 7 * sc <= 0) return;
    for (; *s; s++, x += 6 * sc) {
        if (x >= W) break;
        if (x + 5 * sc <= 0) continue;
        int g = glyph(*s);
        if (g < 0) continue;
        for (int cx = 0; cx < 5; cx++) {
            uint8_t b = font[g][cx];
            if (!b) continue;
            if (sc == 1) {
                int xx = x + cx;
                if (xx < 0 || xx >= W) continue;
                for (int cy = 0; cy < 7; cy++) {
                    int yy = y + cy;
                    if (((b >> cy) & 1) && yy >= 0 && yy < H) fb[yy * W + xx] = col;
                }
            } else {
                for (int cy = 0; cy < 7; cy++)
                    if ((b >> cy) & 1) rect(x + cx * sc, y + cy * sc, sc, sc, col);
            }
        }
    }
}
static int tw(const char *s, int sc) { return (int)strlen(s) * 6 * sc; }
static void textc(int y, const char *s, int sc, uint32_t col) {
    int x = (W - tw(s, sc)) / 2;
    text(x + sc, y + sc, s, sc, 0x000000);
    text(x, y, s, sc, col);
}

// boneco (tripulante). bs = escala
static int bs = 1;
static void br(int ox, int oy, int dx, int dy, int w, int h, uint32_t c) {
    rect(ox + dx * bs, oy + dy * bs, w * bs, h * bs, c);
}
static void bean(int x, int y, uint32_t col, int face, int ph, int dead) {
    uint32_t sh = (col >> 1) & 0x7F7F7F;
    if (dead) {
        br(x, y, -6, 0, 12, 5, col);
        br(x, y, -6, 3, 12, 2, sh);
        br(x, y, -1, -5, 3, 5, 0xE8E8E8);
        br(x, y, -3, -3, 7, 2, 0xE8E8E8);
        return;
    }
    int a = (ph / 6) & 1;
    br(x, y, face > 0 ? -9 : 6, -4, 3, 7, sh);
    br(x, y, -4, -10, 8, 1, col);
    br(x, y, -5, -9, 10, 1, col);
    br(x, y, -6, -8, 12, 10, col);
    br(x, y, -6, 0, 12, 2, sh);
    br(x, y, -6, 2, 5, 3 + a, col);
    br(x, y, 1, 2, 5, 4 - a, col);
    br(x, y, face > 0 ? 0 : -6, -8, 6, 4, 0xA0E4F4);
    br(x, y, face > 0 ? 1 : -5, -8, 3, 1, 0xFFFFFF);
}

// ------------------------------------------------------------ mapa
typedef struct { int x0, y0, x1, y1; const char *name; uint32_t col; } Room;
static const Room rooms[9] = {
    {22, 3, 37, 11, "CAFETERIA", 0x9AA3B5}, {3, 3, 13, 10, "REATOR", 0x9DB19B},
    {3, 16, 13, 24, "ELETRICA", 0xB5A98A},  {3, 29, 13, 35, "MOTORES", 0x9BA9B8},
    {46, 3, 56, 10, "NAVEGACAO", 0x9BB0B3}, {46, 16, 56, 24, "ESCUDOS", 0xA79BB5},
    {46, 29, 56, 35, "OXIGENIO", 0x8FB5A0}, {22, 17, 37, 25, "ADMIN", 0xB59B9B},
    {22, 30, 37, 35, "ARMAS", 0xB0B08F}};
static const int corr[12][4] = {
    {14,7,21,8},{38,6,45,7},{29,12,30,16},{29,26,30,29},{7,11,8,15},{7,25,8,28},
    {14,20,21,21},{38,20,45,21},{50,11,51,15},{50,25,51,28},{38,32,45,33},{14,32,21,33}};
static uint8_t map[MH][MW];
static int8_t rmap[MH][MW];

static void buildMap(void) {
    memset(map, 0, sizeof(map));
    memset(rmap, -1, sizeof(rmap));
    for (int k = 0; k < 12; k++)
        for (int y = corr[k][1]; y <= corr[k][3]; y++)
            for (int x = corr[k][0]; x <= corr[k][2]; x++) map[y][x] = 1;
    for (int r = 0; r < 9; r++)
        for (int y = rooms[r].y0; y <= rooms[r].y1; y++)
            for (int x = rooms[r].x0; x <= rooms[r].x1; x++) { map[y][x] = 1; rmap[y][x] = r; }
}
static float roomCx(int r) { return (rooms[r].x0 + rooms[r].x1 + 1) * TILE * 0.5f; }
static float roomCy(int r) { return (rooms[r].y0 + rooms[r].y1 + 1) * TILE * 0.5f; }
static float taskX(int r) { return roomCx(r) + 32; }
static float taskY(int r) { return roomCy(r) + 14; }

static int solid(float x, float y) {
    int tx = (int)floorf(x / TILE), ty = (int)floorf(y / TILE);
    if (tx < 0 || ty < 0 || tx >= MW || ty >= MH) return 1;
    return !map[ty][tx];
}
static int boxfree(float x, float y, float r) {
    return !(solid(x - r, y - r) || solid(x + r, y - r) || solid(x - r, y + r) || solid(x + r, y + r));
}
static void moveE(float *x, float *y, float dx, float dy) {
    if (boxfree(*x + dx, *y, 5)) *x += dx;
    if (boxfree(*x, *y + dy, 5)) *y += dy;
}

// desenha a parte ESTATICA do mapa (so usado para montar o cache)
static void drawWorldStatic(int camx, int camy) {
    int tx0 = camx / TILE, ty0 = camy / TILE;
    for (int ty = ty0; ty <= ty0 + H / TILE + 1 && ty < MH; ty++)
        for (int tx = tx0; tx <= tx0 + W / TILE + 1 && tx < MW; tx++) {
            int sx = tx * TILE - camx, sy = ty * TILE - camy;
            if (map[ty][tx]) {
                int r = rmap[ty][tx];
                uint32_t c = r >= 0 ? rooms[r].col : 0x7C8496;
                if ((tx + ty) & 1) c -= 0x060606;
                rect(sx, sy, TILE, TILE, c);
            } else {
                uint32_t c = (ty > 0 && map[ty - 1][tx]) ? 0x59627A : 0x151922;
                rect(sx, sy, TILE, TILE, c);
            }
        }
    for (int r = 0; r < 9; r++)
        text(rooms[r].x0 * TILE + 6 - camx, rooms[r].y0 * TILE + 5 - camy, rooms[r].name, 1, 0x2A2F3C);
    circle(480 - camx, 120 - camy, 17, 0x6B7385);
    circle(480 - camx, 120 - camy, 15, 0xC9CED8);
}
static void buildCaches(void) {
    const int cxs[2] = {0, WW - W}, cys[2] = {0, WH - H};
    for (int a = 0; a < 2; a++)
        for (int b = 0; b < 2; b++) {
            drawWorldStatic(cxs[a], cys[b]);
            for (int y = 0; y < H; y++)
                memcpy(worldbuf + (cys[b] + y) * WW + cxs[a], fb + y * W, W * 4);
        }
    memset(mini, 0, sizeof(mini));
    for (int ty = 0; ty < MH; ty++)
        for (int tx = 0; tx < MW; tx++)
            if (map[ty][tx])
                for (int dy = 0; dy < 2; dy++)
                    for (int dx = 0; dx < 2; dx++)
                        mini[(ty * 2 + dy) * (MW * 2) + tx * 2 + dx] = 0x6A7388;
}

// ------------------------------------------------------------ jogo
typedef struct { float x, y, gx, gy; int alive, imp, face, wait, kcd, rt, anim, moving, rep; uint32_t col; } Ent;
typedef struct { float x, y; uint32_t col; int id; } Body;
enum { S_MENU, S_PLAY, S_MEET, S_MRES, S_END };

static const uint32_t cols[NP] = {0xC51111, 0x132ED1, 0x117F2D, 0xED54BA, 0xEF7D0E, 0xF5F557};
static const char *cname[NP] = {"VOCE", "AZUL", "VERDE", "ROSA", "LARANJA", "AMARELO"};

static int state = S_MENU;
static int frame = 0;
static Ent E[NP];
static Body bodies[NP];
static int nb = 0, impId = 1;
static int taskRoom[NTASK], taskDone[NTASK], tdone = 0;
static float tprog = 0;
static int emergUsed = 0, grace = 0;
static int endWin = 0; static char endMsg[80];
static int malive[NP], nalive = 0, msel = 0, mvotes[NP], mhint = -1, mej = -1, navprev = 0;
static char mmsg1[64], mmsg2[64], mtitle[40];

static int16_t dist[NP][MH][MW];
static int16_t qx[MW * MH], qy[MW * MH];

static void bfs(int id, int gx, int gy) {
    int16_t (*d)[MW] = dist[id];
    memset(d, 0xFF, sizeof(int16_t) * MW * MH);
    if (gx < 0 || gy < 0 || gx >= MW || gy >= MH || !map[gy][gx]) return;
    static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
    int h = 0, t = 0;
    d[gy][gx] = 0; qx[t] = gx; qy[t++] = gy;
    while (h < t) {
        int x = qx[h], y = qy[h++], c = d[y][x];
        for (int k = 0; k < 4; k++) {
            int nx = x + dx[k], ny = y + dy[k];
            if (nx >= 0 && ny >= 0 && nx < MW && ny < MH && map[ny][nx] && d[ny][nx] < 0) {
                d[ny][nx] = c + 1; qx[t] = nx; qy[t++] = ny;
            }
        }
    }
}
static int stepTo(int id, float spd) {
    Ent *e = &E[id];
    int tx = (int)(e->x / TILE), ty = (int)(e->y / TILE);
    if (tx < 0 || ty < 0 || tx >= MW || ty >= MH) return 1;
    int16_t (*d)[MW] = dist[id];
    int cur = d[ty][tx];
    if (cur < 0) { e->moving = 0; return 1; }
    float gx = e->gx, gy = e->gy;
    if (cur > 0) {
        static const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
            int nx = tx + dx[k], ny = ty + dy[k];
            if (nx >= 0 && ny >= 0 && nx < MW && ny < MH && d[ny][nx] == cur - 1) {
                gx = (nx + 0.5f) * TILE; gy = (ny + 0.5f) * TILE; break;
            }
        }
    }
    float vx = gx - e->x, vy = gy - e->y, l = sqrtf(vx * vx + vy * vy);
    if (l <= spd) { e->x = gx; e->y = gy; e->moving = 0; return cur == 0; }
    e->x += vx / l * spd; e->y += vy / l * spd;
    e->moving = 1; e->anim++;
    if (vx > 0.3f) e->face = 1; else if (vx < -0.3f) e->face = -1;
    return 0;
}

static int crewAlive(void) {
    int n = 0;
    for (int i = 0; i < NP; i++) if (E[i].alive && !E[i].imp) n++;
    return n;
}
static void endGame(int win, const char *msg) {
    endWin = win; strncpy(endMsg, msg, 79); endMsg[79] = 0; state = S_END;
}
static void killEnt(int v) {
    Ent *e = &E[v];
    e->alive = 0;
    bodies[nb].x = e->x; bodies[nb].y = e->y; bodies[nb].col = e->col; bodies[nb].id = v; nb++;
    if (v == 0) { endGame(0, "VOCE FOI ELIMINADO PELO IMPOSTOR!"); return; }
    if (crewAlive() <= 1) endGame(0, "OS IMPOSTORES VENCERAM!");
}

static void spawnAll(void) {
    for (int i = 0; i < NP; i++) {
        float a = i * 6.2831853f / NP;
        E[i].x = 480 + cosf(a) * 54; E[i].y = 120 + sinf(a) * 34;
        E[i].rt = -1; E[i].wait = (i == 0) ? 0 : RI(60); E[i].rep = 0; E[i].moving = 0;
    }
}
static void newGame(void) {
    memset(E, 0, sizeof(E));
    nb = 0; impId = 1 + RI(NP - 1);
    for (int i = 0; i < NP; i++) { E[i].alive = 1; E[i].imp = (i == impId); E[i].col = cols[i]; E[i].face = 1; }
    spawnAll();
    int order[9];
    for (int i = 0; i < 9; i++) order[i] = i;
    for (int i = 8; i > 0; i--) { int j = RI(i + 1); int t = order[i]; order[i] = order[j]; order[j] = t; }
    for (int i = 0; i < NTASK; i++) { taskRoom[i] = order[i]; taskDone[i] = 0; }
    tdone = 0; tprog = 0; emergUsed = 0; grace = 60 * 8;
    E[impId].kcd = 60 * 20;
}

static void wander(int id, float spd) {
    Ent *e = &E[id];
    if (e->wait > 0) { e->wait--; e->moving = 0; return; }
    if (e->rt < 0) {
        int r = RI(9);
        e->rt = r;
        e->gx = roomCx(r) + RI(48) - 24; e->gy = roomCy(r) + RI(48) - 24;
        bfs(id, (int)(e->gx / TILE), (int)(e->gy / TILE));
    }
    if (stepTo(id, spd)) { e->rt = -1; e->wait = 90 + RI(200); }
}
static void updateImp(int id) {
    Ent *e = &E[id];
    if (e->kcd > 0) e->kcd--;
    if (e->kcd > 0 || grace > 0) { wander(id, 1.0f); return; }
    int t = -1; float bd = 1e9f;
    for (int i = 0; i < NP; i++)
        if (i != id && E[i].alive) {
            float dx = E[i].x - e->x, dy = E[i].y - e->y, d2 = dx * dx + dy * dy;
            if (d2 < bd) { bd = d2; t = i; }
        }
    if (t < 0) { wander(id, 1.0f); return; }
    e->rt = -1; e->wait = 0;
    e->gx = E[t].x; e->gy = E[t].y;
    if (e->rep-- <= 0) { bfs(id, (int)(E[t].x / TILE), (int)(E[t].y / TILE)); e->rep = 10; }
    stepTo(id, 1.35f);
    if (bd < 14.0f * 14.0f) {
        killEnt(t);
        e->kcd = 60 * 14; e->wait = 45; e->rep = 0;
    }
}

static float dist2f(float x1, float y1, float x2, float y2) {
    float dx = x1 - x2, dy = y1 - y2; return sqrtf(dx * dx + dy * dy);
}
static int nearBody(void) {
    for (int i = 0; i < nb; i++) if (dist2f(E[0].x, E[0].y, bodies[i].x, bodies[i].y) < 34) return i;
    return -1;
}
static int nearTable(void) { return dist2f(E[0].x, E[0].y, 480, 120) < 34; }
static int nearTask(void) {
    for (int t = 0; t < NTASK; t++)
        if (!taskDone[t] && dist2f(E[0].x, E[0].y, taskX(taskRoom[t]), taskY(taskRoom[t])) < 24) return t;
    return -1;
}

static void startMeeting(int bi) {
    nalive = 0;
    for (int i = 0; i < NP; i++) if (E[i].alive) malive[nalive++] = i;
    mhint = -1;
    if (bi >= 0) {
        float bd = 1e9f;
        for (int i = 1; i < NP; i++)
            if (E[i].alive) {
                float d = dist2f(E[i].x, E[i].y, bodies[bi].x, bodies[bi].y);
                if (d < bd) { bd = d; mhint = i; }
            }
        strcpy(mtitle, "CORPO ENCONTRADO!");
    } else strcpy(mtitle, "REUNIAO DE EMERGENCIA!");
    for (int i = 1; i < NP; i++) {
        if (!E[i].alive) continue;
        int v;
        if (E[i].imp) { do v = malive[RI(nalive)]; while (v == i); }
        else {
            int r = RI(100);
            if (r < 35 && E[impId].alive) v = impId;
            else if (r < 50) v = 6;
            else { do v = malive[RI(nalive)]; while (v == i); }
        }
        mvotes[i] = v;
    }
    msel = 0; navprev = 0; state = S_MEET;
}

static void updatePlay(void) {
    Ent *p = &E[0];
    if (grace > 0) grace--;
    if (ax != 0 || ay != 0) {
        moveE(&p->x, &p->y, ax * 1.7f, ay * 1.7f);
        p->moving = 1; p->anim++;
        if (ax > 0.2f) p->face = 1; else if (ax < -0.2f) p->face = -1;
    } else p->moving = 0;
    for (int i = 1; i < NP; i++) {
        if (!E[i].alive) continue;
        if (E[i].imp) updateImp(i); else wander(i, 0.9f);
        if (state != S_PLAY) return;
    }
    int t = nearTask();
    if (t >= 0 && (held & B_X)) {
        tprog += 1;
        if (tprog >= 100) {
            taskDone[t] = 1; tdone++; tprog = 0;
            if (tdone >= NTASK) { endGame(1, "TAREFAS CONCLUIDAS! A TRIPULACAO VENCEU!"); return; 