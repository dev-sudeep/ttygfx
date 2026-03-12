#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <zstd.h>

#define ESC "\x1b"
#define PIXELTEXT_DEF "  "
#define RED (Color){255, 0, 0}
#define ORANGE (Color){255, 128, 0}
#define YELLOW (Color){255, 255, 0}
#define GREEN (Color){0, 255, 0}
#define BLUE (Color){0, 0, 255}
#define PURPLE (Color){255, 0, 255}
#define WHITE (Color){255, 255, 255}
#define BLACK (Color){0, 0, 0}

#define BPIXEL(color) color.r, color.g, color.b
#define B_NL '\x20', '\x0a', '\x20' 

#define BPIC_HEADER_SIZE 16


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

struct TMouseEvent{
    struct TPoint position;
    int button;
};

typedef struct TPoint Point;
typedef struct TColor Color;
typedef struct TPixel Pixel;
typedef struct TMouseEvent MouseEvent;

static inline void ttygfx_enable_raw(void);
static inline void ttygfx_disable_raw(void);

static inline int MEAN(int a, int b){
    return (int)(a+b/2);
}

static inline Color MIX (Color c1, Color c2){
    return (Color){MEAN(c1.r, c2.r), MEAN(c1.g, c2.g), MEAN(c1.b, c2.b)};
}

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

static inline void move_cursor(Point p){
    printf(ESC "[%d;%dH", p.y + 1, p.x + 1);
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
    fflush(stdout);
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

Color get_terminal_bg() {
    Color color = {0, 0, 0};
    struct termios old_t, new_t;

    // 1. Save terminal settings and disable echoing/canonical mode
    tcgetattr(STDIN_FILENO, &old_t);
    new_t = old_t;
    new_t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &new_t);

    // 2. Send OSC 11 query: ESC ] 11 ; ? BEL
    printf("\033]11;?\007");
    fflush(stdout);

    // 3. Read response: Expecting something like \033]11;rgb:rrrr/gggg/bbbb\007
    // or \033]11;rgb:rr/gg/bb\033\\

    char buffer[64];
    int n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);

    if (n > 0) {
        buffer[n] = '\0';
        // Find the "rgb:" marker
        char *rgb_ptr = strstr(buffer, "rgb:");
        if (rgb_ptr) {
            unsigned int r, g, b;
            
            // We scan them and shift/scale 16 bit hex if necessary
            if (sscanf(rgb_ptr, "rgb:%x/%x/%x", &r, &g, &b) == 3) {
                // If it's 16-bit (0xFFFF), scale it down to 8-bit (0xFF)
                if (strstr(rgb_ptr, "rgb:0000") == NULL && r > 0xFF) {
                    color.r = r >> 8;
                    color.g = g >> 8;
                    color.b = b >> 8;
                } else {
                    color.r = r;
                    color.g = g;
                    color.b = b;
                }
            }
        }
    }

    // 4. Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &old_t);

    return color;
}

static struct termios orig_termios;

static inline void ttygfx_enable_raw(void) {
    struct termios raw;
    tcgetattr(STDIN_FILENO, &orig_termios);
    raw = orig_termios;

    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static inline void create_bpic(const char* filename, int compression, bool alpha, char* data, long datasize){
    /*Construct header*/
    char header[16];
    header[0] = '\x1b';
    memcpy(&header[1], "BPIC_", 5);
    char comp[2];
    comp[0] = (compression/10) + '0';
    comp[1] = (compression%10) + '0';
    memcpy(&header[6], comp, 2);
    header[8] = 0;
    header[9] = alpha + '0';
    header[10] = 0;
    header[11] = '\x1b';
    header[12] = 0;
    header[13] = 0;
    header[14] = '\x1b';
    header[15] = 0;
    
    /*Create file*/
    FILE* fp = fopen(filename, "wb");
    if(!fp){
        perror("Failed to create file");
        return;
    }
    fwrite(header, 1, 16, fp);
    
    /*Perform compression*/
    size_t compsize;
    char* compdata = NULL; // Declare outside so it's accessible later

    if (compression == 0) {
        compsize = datasize;
        compdata = data; // Note: Ensure you don't 'free' this if it's not from malloc
    } else {
        compsize = ZSTD_compressBound(datasize);
        compdata = (char*)malloc(compsize);
    
        if (compdata == NULL) { /* Handle allocation failure */ return; }
        size_t result = ZSTD_compress(compdata, compsize, data, datasize, compression);
    
        if (ZSTD_isError(result)) {
            fprintf(stderr, "Compression failed: %s\n", ZSTD_getErrorName(result));
            free(compdata); // Free memory before exiting
            return;
        }
        compsize = result; // Update to actual compressed size
    }




    /*Write compressed data*/
    fwrite(compdata, 1, compsize, fp);
    fclose(fp);

    return;
}

/*
 * Improved read_bpic() — drop-in replacement for the version in ttygfx.h
 *
 * Changes from the original:
 *   - Returns uint8_t* (raw decompressed pixel data) instead of int.
 *   - Writes byte count into *out_size  (pass NULL to ignore).
 *   - Writes alpha flag  into *out_alpha (pass NULL to ignore).
 *   - Returns NULL on any error with a descriptive message on stderr.
 *   - Caller is responsible for free()'ing the returned pointer.
 *   - Does NOT render to the terminal — all drawing is left to the caller.
 *   - No longer leaks the compressed buffer when decompression fails.
 *   - Validates that filesize > 16 before attempting to read pixel data.
 *
 * Typical caller pattern:
 *
 *   size_t  data_size;
 *   bool    has_alpha;
 *   uint8_t *data = read_bpic("art.bpic", &data_size, &has_alpha);
 *   if (!data) { ... handle error ... }
 *
 *   size_t pixel_size = has_alpha ? 4 : 3;
 *   for (uint8_t *p = data; p + pixel_size <= data + data_size; p += pixel_size) {
 *       if (p[0] == 0x20 && p[1] == 0x0A && p[2] == 0x20) {
 *           // row separator — advance to next row
 *       } else {
 *           Color c = { p[0], p[1], p[2] };
 *           if (has_alpha) { uint8_t a = p[3]; ... alpha blend ... }
 *       }
 *   }
 *   free(data);
 */
static inline uint8_t *read_bpic(const char *path,
                                  size_t     *out_size,
                                  bool       *out_alpha)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("read_bpic: fopen");
        return NULL;
    }

    /* ---- Read and validate 16-byte header ---- */
    uint8_t hdr[16];
    if (fread(hdr, 1, 16, fp) != 16) {
        fprintf(stderr, "read_bpic: file too short for header\n");
        fclose(fp);
        return NULL;
    }

    if (memcmp(hdr, "\x1B""BPIC_", 6) != 0) {
        fprintf(stderr, "read_bpic: unrecognised file format\n");
        fclose(fp);
        return NULL;
    }

    int  compression = (hdr[6] - '0') * 10 + (hdr[7] - '0');
    bool has_alpha   = (hdr[9] == '1');

    /* ---- Read payload ---- */
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 16, SEEK_SET);

    if (filesize <= 16) {
        fprintf(stderr, "read_bpic: file contains no pixel data\n");
        fclose(fp);
        return NULL;
    }

    size_t   payload_size = (size_t)(filesize - 16);
    uint8_t *payload      = malloc(payload_size);
    if (!payload) {
        perror("read_bpic: malloc payload");
        fclose(fp);
        return NULL;
    }

    if (fread(payload, 1, payload_size, fp) != payload_size) {
        fprintf(stderr, "read_bpic: short read on payload\n");
        free(payload);
        fclose(fp);
        return NULL;
    }
    fclose(fp);

    /* ---- Decompress if needed ---- */
    uint8_t *data      = NULL;
    size_t   data_size = 0;

    if (compression == 0) {
        /* Uncompressed: payload IS the data */
        data      = payload;
        data_size = payload_size;
    } else {
        unsigned long long expected =
            ZSTD_getFrameContentSize(payload, payload_size);

        if (expected == ZSTD_CONTENTSIZE_ERROR ||
            expected == ZSTD_CONTENTSIZE_UNKNOWN) {
            fprintf(stderr, "read_bpic: cannot determine decompressed size\n");
            free(payload);
            return NULL;
        }

        data_size = (size_t)expected;
        data      = malloc(data_size);
        if (!data) {
            perror("read_bpic: malloc decompressed buffer");
            free(payload);
            return NULL;
        }

        size_t result = ZSTD_decompress(data, data_size, payload, payload_size);
        free(payload); /* always freed here, success or failure */

        if (ZSTD_isError(result)) {
            fprintf(stderr, "read_bpic: decompression failed: %s\n",
                    ZSTD_getErrorName(result));
            free(data);
            return NULL;
        }

        data_size = result; /* actual bytes written by ZSTD */
    }

    if (out_size)  *out_size  = data_size;
    if (out_alpha) *out_alpha = has_alpha;

    return data; /* caller must free() */
}

static inline int render_bpic(const uint8_t *data,
                               size_t         data_size,
                               bool           has_alpha)
{
    if (!data) {
        fprintf(stderr, "render_bpic: NULL data pointer\n");
        return -1;
    }

    size_t pixel_size = has_alpha ? 4 : 3;

    /* Query the terminal background once up-front if we need it for blending */
    Color bg = {0, 0, 0};
    if (has_alpha)
        bg = get_terminal_bg();

    printf("\x1B[2J");   /* clear screen */
    printf("\x1B[H");    /* move cursor to top-left */

    const uint8_t *p   = data;
    const uint8_t *end = data + data_size;
    int x = 0, y = 0;

    while (p + pixel_size <= end) {

        /* Row-separator marker: 0x20 0x0A 0x20 */
        if (p[0] == 0x20 && p[1] == 0x0A && p[2] == 0x20) {
            x = 0;
            y++;
            p += pixel_size;
            continue;
        }

        Color c = { p[0], p[1], p[2] };

        if (has_alpha) {
            uint8_t a = p[3];
            c.r = (uint8_t)((c.r * a + bg.r * (255 - a)) / 255);
            c.g = (uint8_t)((c.g * a + bg.g * (255 - a)) / 255);
            c.b = (uint8_t)((c.b * a + bg.b * (255 - a)) / 255);
        }

        Pixel pix;
        pix.position = (Point){ x + 1, y + 1 };
        pix.color    = c;
        DrawPixel(pix, PIXELTEXT_DEF, T_BG);

        x += 2;  /* each cell is two terminal columns wide (PIXELTEXT_DEF = "  ") */
        p += pixel_size;
    }

    fflush(stdout);
    return 0;
}

static inline void ttygfx_disable_raw(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}


static inline void enable_Mouse(){
    ttygfx_enable_raw();
    printf("\033[?1000h"); // basic mouse clicks
    printf("\033[?1006h"); // SGR extended mode (recommended)
    fflush(stdout); 
}

static inline struct TMouseEvent* is_mouseevent(void) {
    static struct TMouseEvent ev;
    char buf[32];
    fd_set set;
    struct timeval tv = {0, 0};

    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);

    if (select(STDIN_FILENO + 1, &set, NULL, NULL, &tv) <= 0)
        return NULL;

    int n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (n <= 0)
        return NULL;

    buf[n] = '\0';

    // Expect: ESC [ < b ; x ; y M
    if (buf[0] != '\033' || buf[1] != '[' || buf[2] != '<')
        return NULL;

    int b, x, y;
    char type;

    if (sscanf(buf, "\033[<%d;%d;%d%c", &b, &x, &y, &type) != 4)
        return NULL;

    if (type != 'M')   // ignore release events ('m')
        return NULL;

    ev.button = b;
    ev.position.x = x;
    ev.position.y = y;
    return &ev;
}

static inline void disable_mouse(){
    printf("\033[?1000l");
    printf("\033[?1006l");
    fflush(stdout);
    ttygfx_disable_raw();
}
