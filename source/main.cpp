// main.cpp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <sys/systime.h>
#include <sysutil/video.h>
#include <io/pad.h>
#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>

#include "render.h"
#include "game.h"

static u32 *g_fb[2];
static u32 g_depth_offset;
static gcmContextData *g_context;
static int g_curr_fb = 0;
static videoResolution g_res;

static void waitFlip(void) {
    while (gcmGetFlipStatus() != 0) {
        sysUsleep(200);
    }
    gcmResetFlipStatus();
}

static void flipFrame(void) {
    waitFlip();
    gcmSetFlip(g_context, g_curr_fb);
    rsxFlushBuffer(g_context);
    gcmSetWaitFlip(g_context);
    g_curr_fb ^= 1;
}

static void initScreen(void) {
    videoState state;
    videoConfiguration vconfig;
    memset(&state, 0, sizeof(state));
    if (videoGetState(0, 0, &state) != 0) return;
    if (videoGetResolution(state.displayMode.resolution, &g_res) != 0) return;

    memset(&vconfig, 0, sizeof(vconfig));
    vconfig.resolution = VIDEO_RESOLUTION_720;
    vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch = g_res.width * 4;
    vconfig.aspect = state.displayMode.aspect;
    videoConfigure(0, &vconfig, NULL, 0);
    videoGetState(0, 0, &state);

    void *host_addr = memalign(1024 * 1024, 2 * 1024 * 1024);
    rsxInit(&g_context, 1024, 1024, host_addr);
    gcmSetFlipMode(GCM_FLIP_VSYNC);

    int i;
    for (i = 0; i < 2; i++) {
        g_fb[i] = (u32 *)rsxMemalign(64, g_res.width * g_res.height * 4);
        rsxAddressToOffset(g_fb[i], &g_depth_offset);
        gcmSetDisplayBuffer(i, g_depth_offset, g_res.width * 4, g_res.width, g_res.height);
    }

    for (i = 0; i < 2; i++) {
        gcmSetWaitFlip(g_context);
        flipFrame();
    }
    gcmResetFlipStatus();
}

static void handlePad(padData *pad, int *btnCross, int *btnSquare, int *btnTriangle, int *btnCircle, int *btnStart, float *ax, float *az) {
    *btnCross = (pad->BTN_CROSS != 0);
    *btnSquare = (pad->BTN_SQUARE != 0);
    *btnTriangle = (pad->BTN_TRIANGLE != 0);
    *btnCircle = (pad->BTN_CIRCLE != 0);
    *btnStart = (pad->BTN_START != 0);

    float rawX = (float)pad->ANA_L_H;
    float rawZ = (float)pad->ANA_L_V;
    *ax = (rawX - 127.5f) / 127.5f;
    *az = (rawZ - 127.5f) / 127.5f;

    if (pad->BTN_LEFT) *ax = -1.0f;
    if (pad->BTN_RIGHT) *ax = 1.0f;
    if (pad->BTN_UP) *az = -1.0f;
    if (pad->BTN_DOWN) *az = 1.0f;
}

int main(int argc, char *argv[]) {
    int playerIsImpostor = (argc > 1 && strcmp(argv[1], "impostor") == 0);
    gameInit(playerIsImpostor);
    renderInit();
    initScreen();
    ioPadInit(7);

    u64 lastTime = sysGetSystemTime();
    float deltaTime = 1.0f / 60.0f;

    while (1) {
        u64 now = sysGetSystemTime();
        deltaTime = (float)(now - lastTime) / 1000000.0f;
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        if (deltaTime < 0.001f) deltaTime = 0.001f;
        lastTime = now;

        padData pad;
        memset(&pad, 0, sizeof(pad));
        ioPadGetData(0, &pad);

        int btnCross, btnSquare, btnTriangle, btnCircle, btnStart;
        float ax, az;
        handlePad(&pad, &btnCross, &btnSquare, &btnTriangle, &btnCircle, &btnStart, &ax, &az);

        gameUpdate(deltaTime, btnCross, btnSquare, btnTriangle, btnCircle, btnStart, ax, az);
        gameRender();
        renderPresent(g_fb[g_curr_fb]);
        flipFrame();
    }

    return 0;
}