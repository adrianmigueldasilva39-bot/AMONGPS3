// render.h
#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>
#include <string.h>

#define FB_WIDTH 1280
#define FB_HEIGHT 720

typedef struct {
    float x, y, z;
} Vec3;

typedef struct {
    uint32_t color;
    float z;
} Pixel;

extern Pixel g_framebuffer[FB_WIDTH * FB_HEIGHT];
extern uint32_t g_fb32[FB_WIDTH * FB_HEIGHT];

void renderInit(void);
void renderClear(uint32_t color);
void renderPresent(uint32_t *dst);
void renderSetCamera(float isoAngle, float tiltAngle, float zoom, float camX, float camY, float camZ);
Vec3 renderProject(Vec3 world);
void renderFillTriangle(Vec3 v0, Vec3 v1, Vec3 v2, uint32_t baseColor, float lightDirX, float lightDirY, float lightDirZ);
void renderDrawLine(float x0, float y0, float x1, float y1, uint32_t color);
void renderFillRect(int x, int y, int w, int h, uint32_t color);
void renderDrawText(int x, int y, const char *text, uint32_t color, int scale);
void renderDrawTextCentered(int cx, int y, const char *text, uint32_t color, int scale);

#endif