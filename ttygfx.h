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

Color get_terminal_bg() {
    Color c = {0, 0, 0};
    ttygfx_enable_raw();

   // 2. Send the OSC 11 query: "What is your background color?"
    // \033]11;?\007
    printf("\033]11;?\007");
    fflush(stdout);

    // 3. Read the response from stdin
    // Format is usually: ^]11;rgb:RRRR/GGGG/BBBB^G
    char response[64];
    int n = 0;
    
    // Read until the terminator (BEL \007 or ST \033\\)
    while (n < sizeof(response) - 1) {
        if (read(STDIN_FILENO, &response[n], 1) <= 0) break;
        if (response[n] == '\007') break; // End of response
        n++;
    }
    response[n] = '\0';

    // 4. Restore terminal settings immediately
    ttygfx_disable_raw();

    // 5. Parse the hex values (using sscanf here since the response IS text)
    // Note: Terminals often use 16-bit hex (RRRR), so we scale to 8-bit
    unsigned int r, g, b;
    if (sscanf(strstr(response, "rgb:"), "rgb:%x/%x/%x", &r, &g, &b) == 3) {
        // If the terminal sends 16-bit values (0xFFFF), scale them to 0-255
        c.r = (r > 0xFF) ? (r >> 8) : r;
        c.g = (g > 0xFF) ? (g >> 8) : g;
        c.b = (b > 0xFF) ? (b >> 8) : b;
    }

    return c;
}

/*static inline int read_bpic(const char *path){
    char dl_char[2];
    char alpha_char;
    size_t len = strlen(path);
    int dl;
    bool isalpha;
    FILE* fp = fopen(path, "rb");
    char magic[16];
    char* p;

    if(!fp){
        perror("Unable to open file");
        exit(1);
    }
    /* Infer compression level from magic if not provided 
    fread(magic, 1, 16, fp);
    p=&magic[0];
    char* a = malloc(6);
    memcpy(a, p, 6);
    if(strcmp(a, ESC "BPIC_")){
        printf("File format not recognised");
        exit(1);
    }
    p += 6;
    memcpy(dl_char, p, 2);
    p += 3;
    memcpy(&alpha_char, p, 1);
    p += 7;
    isalpha = alpha_char == '1' ? true:false;
    dl = atoi(dl_char);
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);
    char* data = malloc(size - 15);
    fseek(fp, 16, SEEK_SET);
    fread(data, 1, size - 16, fp);
    data[size - 16] = '\0';
    fclose(fp);
    free(a);
    int fdp1[2], fdp2[2];
    pipe(fdp1);
    pipe(fdp2);
    char* dcdata = malloc(size - 15);
    /* Decompress if needed 
    if(dl == 0){
        dcdata = data;
    }else{
        pid_t pid = fork();
        if(pid < 0){
            perror("fork error");
            return 1;
        }

        if(pid == 0){
            dup2(fdp2[1], STDOUT_FILENO);
            dup2(fdp1[0], STDIN_FILENO);

            close(fdp1[0]);
            close(fdp1[1]);
            close(fdp2[0]);
            

            execlp(
                "zstd",
                "zstd",
                "-d",
                "-"
                "-o",
                "ttygfxtmpfiledecompressed.bpic00",
                "-c",
                NULL
            );

            perror("Failed to decompress using zstd");
            _exit(1);
        }else{
            close(fdp1[0]);

        // 2. Write the string to the pipe
            write(fdp1[1], data, strlen(data));

        // 3. Close the write end to signal EOF
            close(fdp1[1]);

            read(fdp2[0], dcdata, size-16);
            
            close(fdp2[0]);
            close(fdp2[1]);

            if(waitpid(pid, NULL, 0) < 0){
                perror("waitpid error");
                return 1;
            }
        }

    }

    /* Read and render 
    p = dcdata;
    int b;
    int x = 0, y = 0;

    printf(ESC "[2J");

    for(; p < p + size-16; p += 3+isalpha){
        if(p[0] == '\x20' && p[1] == '\x0a' && p[2] == '\x20'){
            x=0;
            y++;
            continue;
        }
        Color c = (Color){p[0], p[1], p[2]};
        if(isalpha){
            Color bg = get_terminal_bg();
            c.r += ((bg.r-c.r)/255)*p[3];
            c.g += ((bg.g-c.g)/255)*p[3];
            c.b += ((bg.b-c.b)/255)*p[3];
        }
        Pixel pix;
        pix.position={x+1, y+1};
        pix.color = c;
        DrawPixel(pix, PIXELTEXT_DEF, T_BG);
    }

    return 0;
}
*/
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
