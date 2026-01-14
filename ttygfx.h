#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ESC "\x1b"
#define PIXELTEXT_DEF "  "

enum T_LAYER { T_FG = 38, T_BG = 48 };
enum T_DRAWMODE { picture, animated };

static enum T_DRAWMODE TMode = picture;

struct TPoint {
    int x;
    int y;
};

struct TColor {
    unsigned char r, g, b;
};

struct TPixel {
    struct TPoint position;
    struct TColor color;
};

typedef struct TPoint Point;
typedef struct TColor Color;
typedef struct TPixel Pixel;

static inline void RefreshScreen(int framerate)
{
    static int first = 1;

    printf(ESC "[?25l");  // hide cursor

    if (first) {
        printf(ESC "[2J");
        first = 0;
    }

    if (TMode == animated) {
        if (framerate > 0)
            usleep(1000000 / framerate);
        printf(ESC "[H");
    }

    fflush(stdout);
}

static inline void DrawPixel(struct TPixel p,
                             const char *pt,
                             enum T_LAYER layer)
{
    printf(ESC "[%d;%dH"
           ESC "[%d;2;%d;%d;%dm%s"
           ESC "[0m",
           p.position.y + 1,
           p.position.x + 1,
           layer,
           p.color.r,
           p.color.g,
           p.color.b,
           pt);
}

static inline void DrawLine(
    struct TPoint p0,
    struct TPoint p1,
    struct TColor color,
    const char pt[],
    short layer
){
    int x0 = p0.x;
    int y0 = p0.y;
    int x1 = p1.x;
    int y1 = p1.y;

    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);

    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;

    int err = dx - dy;

    while (1) {
        struct TPixel pixel = {
            .position = { x0, y0 },
            .color = color
        };

        DrawPixel(pixel, pt, layer);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;

        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static inline void Reset_tty(){
	printf(ESC "[0m");
	printf(ESC "[?25h");
}
