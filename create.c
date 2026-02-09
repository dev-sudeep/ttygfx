#include "ttygfx.h"

int main(){
    char bstr[] = {BPIXEL(RED), BPIXEL(ORANGE), BPIXEL(YELLOW), 
                   BPIXEL(GREEN), B_NL, BPIXEL(BLUE), BPIXEL(PURPLE), 
                   BPIXEL(WHITE), BPIXEL(BLACK), B_NL, 
                   BPIXEL(MIX(RED, GREEN)), BPIXEL(MIX(GREEN, BLUE)), BPIXEL(MIX(RED, BLUE))};
    FILE* fp = fopen("test.bpic00", "wb");
    if(!fp){
        exit(1);
    }
    fwrite(bstr, 1, sizeof(bstr), fp);
    fclose(fp);
    system("zstd test.bpic00 -o test.bpic03");

}
