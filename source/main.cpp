// main.cpp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <sys/systime.h>
#include <sysutil/video.h>
#include <sysutil/sysutil.h>
#include <io/pad.h>
#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>

#include "render.h"
#include "game.h"

static u32 *g_fb;
static u32 g_fb_offset;
static gcmContextData *g_context = NULL;
static int g_curr_fb = 0;
static int g_running = 1;

static void sysutilCallback(u64 status, u64 param, void *userdata) {
    (void)param; (void)userdata;
    if (status == SYSUTIL_EXIT_GAME) {
        g_running = 0;
    }
}

static void waitFlip(void) {
    while (gcmGetFlipStatus() != 0) {
        sysUsleep(200);
    }
    gcmResetFlipStatus();
}

static void flipFrame(int bufferId) {
    gcmSetFlip(g_context, bufferId);
    rsxFlushBuffer(g_context);
    gcmSetWaitFlip(g_context);
}

static void initScreen(void) {
    void *host_addr = memalign(1024 * 1024, 2 * 1024 * 1024);
    rsxInit(&g_context, 1024, 1024, host_addr);

    videoState state;
    videoConfiguration vconfig;
    memset(&state, 0, sizeof(state));
    videoGetState(0, 0, &state);

    memset(&vconfig, 0, sizeof(vconfig));
    vconfig.resolution = VIDEO_RESOLUTION_720;
    vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch = FB_WIDTH * 4;
    vconfig.aspect = VIDEO_ASPECT_16_9;
    videoConfigure(0, &vconfig, NULL, 0);

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    for (int i = 0; i < 2; i++) {
        g_fb[i] = (u32 *)rsxMemalign(64, FB_WIDTH * FB_HEIGHT * 4);
        memset(g_fb[i], 0, FB_WIDTH * FB_HEIGHT * 4);
        rsxAddressToOffset(g_fb[i], &g_fb_offset[i]);
        gcmSetDisplayBuffer(i, g_fb_offset[i], FB_WIDTH * 4, FB_WIDTH, FB_HEIGHT);
    }

    gcmResetFlipStatus();
    flipFrame(0);
    g_curr_fb = 1;
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
    (void)argc; (void)argv;
    sysUtilRegisterCallback(0, sysutilCallback, NULL);

    initScreen();
    renderInit();
    gameInit(0);
    ioPadInit(7);

    u64 lastTime = sysGetSystemTime();
    float deltaTime = 1.0f / 60.0f;

    while (g_running) {
        sysUtilCheckCallback();

        u64 now = sysGetSystemTime();
        deltaTime = (float)(now - lastTime) / 1000000.0f;
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        if (deltaTime < 0.001f) deltaTime = 0.001f;
        lastTime = now;

        padData pad;
        memset(&pad, 0, sizeof(pad));
        ioPadGetData(0, &pad);

        int btnCross = 0, btnSquare = 0, btnTriangle = 0, btnCircle = 0, btnStart = 0;
        float ax = 0.0f, az = 0.0f;
        if (pad.len > 0) {
            handlePad(&pad, &btnCross, &btnSquare, &btnTriangle, &btnCircle, &btnStart, &ax, &az);
        }

        gameUpdate(deltaTime, btnCross, btnSquare, btnTriangle, btnCircle, btnStart, ax, az);
        gameRender();

        waitFlip();
        renderPresent(g_fb[g_curr_fb]);
        flipFrame(g_curr_fb);
        g_curr_fb ^= 1;
    }

    return 0;
}
