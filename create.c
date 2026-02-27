#include "ttygfx.h"

int main(){
    char data[] = {BPIXEL(RED), 255, BPIXEL(RED), 192, BPIXEL(RED), 128, BPIXEL(RED), 64, BPIXEL(RED), 0, B_NL, 0, BPIXEL(GREEN), 0, BPIXEL(GREEN), 64, BPIXEL(GREEN), 128, BPIXEL(GREEN), 192, BPIXEL(GREEN), 255};
    create_bpic("test.bpic00", 0, 1, data, 44);
    create_bpic("test.bpic03", 3, 1, data, 44);
    create_bpic("test.bpic13", 13, 1, data, 44);

}
