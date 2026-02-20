#include "ttygfx.h"

int main(){
    char bstr[] = "\x1b" "BPIC_00" "\0" "0" "\0\x1b\0\0\x1b\0" "\xff\x00\x00\x00\xff\x00\x00\x00\xff";
    FILE* fp = fopen("test.bpic00", "wb");
    if(!fp){
        exit(1);
    }
    fwrite(bstr, 1, 25, fp);
    fclose(fp);

}
