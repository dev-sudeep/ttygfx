#include "ttygfx.h"

int main(){
	TMode = picture;
	RefreshScreen(0);
	DrawLine((Point){0, 0}, (Point){20, 10}, (Color){255, 0, 0}, PIXELTEXT_DEF, T_BG);
	DrawLine((Point){0, 0}, (Point){0, 10}, (Color){255, 128, 0}, PIXELTEXT_DEF, T_BG);
	DrawLine((Point){0, 0}, (Point){20, 0}, (Color){255, 255, 0}, PIXELTEXT_DEF, T_BG);
	DrawLine((Point){20, 0}, (Point){20, 10}, (Color){0, 255, 0}, PIXELTEXT_DEF, T_BG);
	DrawLine((Point){0, 10}, (Point){20, 10}, (Color){0, 0, 255}, PIXELTEXT_DEF, T_BG);
    getchar();
    read_bpic("demo.bpic13", -1);
    getchar();
    enable_Mouse();
    int c;
    for(c = 0; c<5; ){
        MouseEvent *ev;
        if((ev = is_mouseevent())){
            RefreshScreen(0);
            printf("Mouse event button %d at (%d, %d)", ev->button, ev->position.x, ev->position.y);
            c++;
        }
    }
    disable_mouse();
    Reset_tty();
}
