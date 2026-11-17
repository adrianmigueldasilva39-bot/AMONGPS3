// main.cpp
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <malloc.h>
#include <sys/systime.h>
#include <sysutil/video.h>
#include <sysutil/sysutil.h>
#include <rsx/rsx.h>
#include <io/pad.h>

#include "render.h"
#include "game.h"

#define CB_SIZE   0x100000
#define HOST_SIZE (32 * 1024 * 1024)

/* ---------- Video / RSX (baseado no codigo que abriu video no PS3) ---------- */
static gcmContextData *ctx;
static void *bufs[2];
static u32   offs[2];
static int   cur = 0;

static void video_init(void) {
    videoState state;
    videoConfiguration vc;
    void *host;
    int i;

    videoGetState(0, 0, &state);

    memset(&vc, 0, sizeof(vc));
    vc.resolution = VIDEO_RESOLUTION_720;
    vc.format     = VIDEO_BUFFER_FORMAT_XRGB;
    vc.pitch      = FB_WIDTH * 4;
    vc.aspect     = VIDEO_ASPECT_AUTO;
    videoConfigure(0, &vc, NULL, 0);
    videoGetState(0, 0, &state);

    host = memalign(1024 * 1024, HOST_SIZE);
    rsxInit(&ctx, CB_SIZE, HOST_SIZE, host);

    for (i = 0; i < 2; i++) {
        bufs[i] = rsxMemalign(64, FB_WIDTH * FB_HEIGHT * 4);
        memset(bufs[i], 0, FB_WIDTH * FB_HEIGHT * 4);
        rsxAddressToOffset(bufs[i], &offs[i]);
        gcmSetDisplayBuffer(i, offs[i], FB_WIDTH * 4, FB_WIDTH, FB_HEIGHT);
    }
}

static void flip(void) {
    gcmSetFlip(ctx, cur);
    rsxFlushBuffer(ctx);
    gcmSetWaitFlip(ctx);
    cur ^= 1;
}

static void handlePad(padData *pad, int *btnCross, int *btnSquare, int *btnTriangle, int *btnCircle, int *btnStart, float *ax, float *az) {
    *btnCross    = (pad->BTN_CROSS != 0);
    *btnSquare   = (pad->BTN_SQUARE != 0);
    *btnTriangle = (pad->BTN_TRIANGLE != 0);
    *btnCircle   = (pad->BTN_CIRCLE != 0);
    *btnStart    = (pad->BTN_START != 0);

    float rawX = (float)pad->ANA_L_H;
    float rawZ = (float)pad->ANA_L_V;
    *ax = (rawX - 127.5f) / 127.5f;
    *az = (rawZ - 127.5f) / 127.5f;

    if (pad->BTN_LEFT)  *ax = -1.0f;
    if (pad->BTN_RIGHT) *ax =  1.0f;
    if (pad->BTN_UP)    *az = -1.0f;
    if (pad->BTN_DOWN)  *az =  1.0f;
}

int main(int argc, char *argv[]) {
    (void)argc; (void)argv;

    video_init();
    ioPadInit(7);
    renderInit();
    gameInit(0);

    u64 lastTime = sysGetSystemTime();
    float deltaTime = 1.0f / 60.0f;

    while (1) {
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

        renderPresent((uint32_t *)bufs[cur]);
        flip();
    }

    return 0;
}
