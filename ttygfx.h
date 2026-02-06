#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>

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

struct TMouseEvent{
    struct TPoint position;
    int button;
};

typedef struct TPoint Point;
typedef struct TColor Color;
typedef struct TPixel Pixel;
typedef struct TMouseEvent MouseEvent;
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

static inline int read_bpic(const char *path, int compression_level){
    char filef[8192];
    char last2[3];
    size_t len = strlen(path);
    int dl = compression_level;

    /* Infer compression level from extension if not provided */
    if(dl < 0){
        if(len >= 7){
            last2[0] = path[len - 2];
            last2[1] = path[len - 1];
            last2[2] = '\0';
            dl = atoi(last2);
        }else{
            fprintf(stderr, "File not having extension and compression level not provided\n");
            return 1;
        }
    }

    /* Decompress if needed */
    if(dl == 0){
        strcpy(filef, path);
    }else{
        pid_t pid = fork();
        if(pid < 0){
            perror("fork error");
            return 1;
        }

        if(pid == 0){
            int fd = open("/dev/null", O_WRONLY);
            if(fd < 0){
                perror("Failed to open /dev/null");
                _exit(1);
            }
            dup2(fd, STDOUT_FILENO);

            execlp(
                "zstd",
                "zstd",
                "-d",
                path,
                "-o",
                "ttygfxtmpfiledecompressed.bpic0",
                NULL
            );

            perror("Failed to decompress using zstd");
            _exit(1);
        }else{
            if(waitpid(pid, NULL, 0) < 0){
                perror("waitpid error");
                return 1;
            }
        }

        strcpy(filef, "ttygfxtmpfiledecompressed.bpic0");
    }

    /* Read and render */
    int fd = open(filef, O_RDONLY);
    if(fd < 0){
        perror("unable to open file");
        return 1;
    }

    char buf[3];
    int b;
    int x = 0, y = 0;

    printf(ESC "[2J");

    while((b = read(fd, buf, 3)) > 0){
        if(buf[0] == '\x20' && buf[1] == '\x0a' && buf[2] == '\x20'){
            y++;
            x = 0;
            continue;
        }

        Pixel p = {
            (Point){ x, y },
            (Color){ buf[0], buf[1], buf[2] }
        };

        DrawPixel(p, PIXELTEXT_DEF, T_BG);
        x += 2;
    }

    if(b < 0){
        perror("read error");
        close(fd);
        return 1;
    }

    close(fd);

    if(dl != 0){
        remove("ttygfxtmpfiledecompressed.bpic0");
    }

    return 0;
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
