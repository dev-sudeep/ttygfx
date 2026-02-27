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

static inline int read_bpic(const char* path){
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("Unable to open file");
        return 1;
    }

    /* ---- Read header ---- */
    uint8_t magic[16];
    if (fread(magic, 1, 16, fp) != 16) {
        fprintf(stderr, "Invalid BPIC header\n");
        fclose(fp);
        return 1;
    }

    /* Validate magic */
    if (memcmp(magic, "\x1B""BPIC_", 6) != 0) {
        fprintf(stderr, "File format not recognised\n");
        fclose(fp);
        return 1;
    }

    /* Compression level (ASCII digits) */
    int dl = (magic[6] - '0') * 10 + (magic[7] - '0');

    /* Alpha flag */
    bool isalpha = (magic[9] == '1');

    /* ---- Read entire payload ---- */
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 16, SEEK_SET);

    size_t compressed_size = filesize - 16;
    uint8_t *compressed = malloc(compressed_size);
    if (!compressed) {
        perror("malloc failed");
        fclose(fp);
        return 1;
    }

    if (fread(compressed, 1, compressed_size, fp) != compressed_size) {
        fprintf(stderr, "Failed to read BPIC data\n");
        free(compressed);
        fclose(fp);
        return 1;
    }

    fclose(fp);
    
    uint8_t *data = NULL;
    size_t decompressed_size = compressed_size;

    if (dl == 0) {
        /* No compression */
        data = compressed;
    } else {
        unsigned long long expected_size =
            ZSTD_getFrameContentSize(compressed, compressed_size);

        if (expected_size == ZSTD_CONTENTSIZE_ERROR ||
            expected_size == ZSTD_CONTENTSIZE_UNKNOWN) {
            fprintf(stderr, "Invalid or unknown ZSTD frame size\n");
            free(compressed);
            return 1;
        }

        decompressed_size = (size_t)expected_size;
        data = malloc(decompressed_size);
        if (!data) {
            perror("malloc failed");
            free(compressed);
            return 1;
        }

        size_t result = ZSTD_decompress(
            data,
            decompressed_size,
            compressed,
            compressed_size
        );
        

        free(compressed);
    }

    printf("\x1B[2J");  // clear screen

    size_t pixel_size = 3 + (isalpha ? 1 : 0);
    uint8_t *p = data;
    uint8_t *end = data + decompressed_size;

    int x = 0, y = 0;

    while (p + pixel_size <= end) {

        /* newline marker */
        if (p[0] == 0x20 && p[1] == 0x0A && p[2] == 0x20) {
            x = 0;
            y++;
            p += pixel_size;
            continue;
        }

        Color c = { p[0], p[1], p[2] };

        if (isalpha) {
            uint8_t a = p[3];
            Color bg = get_terminal_bg();

            /* Correct alpha blending */
            c.r = (c.r * a + bg.r * (255 - a)) / 255;
            c.g = (c.g * a + bg.g * (255 - a)) / 255;
            c.b = (c.b * a + bg.b * (255 - a)) / 255;
        }
        Pixel pix;
        pix.position = (Point){ x + 1, y + 1 };
        pix.color = c;

        DrawPixel(pix, PIXELTEXT_DEF, T_BG);

        x += 2;
        p += pixel_size;
    }

    free(data);
    
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
