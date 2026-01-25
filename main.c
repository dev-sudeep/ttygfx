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
}
