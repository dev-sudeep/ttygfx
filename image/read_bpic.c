#include <stdio.h>
#include "../ttygfx.h"
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>

int main(int argc, char* argv[]){
    if(argc < 2){
    	printf("Usage: %s [FILE] [COMPRESSION_LEVEL](optional if file has extension .bpicXX)", argv[0]);
    	return 1;
    }
	char* file = strdup(argv[1]);
	char filef[8192];
	int dl;
	char last2[3];	
	size_t len = strlen(argv[1]);
	if(argc > 2){
		dl = atoi(argv[2]);
	}else if (len >= 7) {
	    last2[0] = argv[1][len - 2];
	    last2[1] = argv[1][len - 1];
	    last2[2] = '\0';
	    dl = atoi(last2);
	}else{
		printf("File not having extension and compression level argument not provided");
		return 1;
	}
	if(dl == 0){
		strcpy(filef, file);
	}else{
		pid_t pid = fork();
		if(pid < 0){
			perror("fork error");
			return 1;
		}if(pid == 0){
		    int fd = open("/dev/null", O_WRONLY);
		    if(fd < 0){
		    	perror("Failed to open /dev/null");
		    	return -1;
		    }
		    dup2(fd, STDOUT_FILENO);
			execlp("zstd", "zstd", "-d", file, "-o", "ttygfxtmpfiledecompressed.bpic0", NULL);
			perror("Failed to decompress using zstd");
			return 1;
		}else{
			if (waitpid(pid, NULL, 0) < 0){
			    perror("waitpid error");
			    return 1;
			}
		}
	    strcpy(filef, "ttygfxtmpfiledecompressed.bpic0");
	} 
	int fd = open(filef, O_RDONLY);
	if(fd < 0){
		perror("unable to open file");
		return 1;
	}
	char buf[3];
	int b;
	int x=0, y=0;
	printf(ESC "[2J");
	while((b = read(fd, buf, 3)) > 0){
		if(buf[0] == '\x20' && buf[1] == '\x0a' && buf[2] == '\x20'){
			y++;
			x=0;
			continue;
		}
		Pixel p ={(Point){x, y}, (Color){buf[0], buf[1], buf[2]}};
		DrawPixel(p, PIXELTEXT_DEF, T_BG);
		x += 2;
	}
	if(b < 0){
		perror("read error");
		return 1;
	}
	if(dl != 0){
	    close(fd);
		remove("ttygfxtmpfiledecompressed.bpic0");
	}
	close(fd);
	return 0;
}
