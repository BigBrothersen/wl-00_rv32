// terminal: test for the read syscall. Reads stdin line by line and prints
// what read() returned. Ctrl-D on an empty line (read returns 0) ends it.
#include "user.h"

int main(int argc, char *argv[]) {
    int pid = fork();
    if (pid == 0) {
        while(1) {
            sleep(1);
            printf("test\n");
        }; // loop forever
    }

    sleep(3);
    
    printf("killed pid %d with num: %d\n", pid, kill(pid));
    char buf[256];
    printf("Welcome to terminal: type a line, Ctrl-D to quit\n");
    while (1) {
        printf("$ ");
        int n = read(0, buf, sizeof(buf)-1);
        if (n < 0) {
            printf("read error %d\n", n);
            exit(1);
        }
        if (n == 0) {
            printf("\nterminal: EOF\n");
            break;
        }
        buf[n] = 0;
        printf("read %d bytes: [%s]\n", n, buf); // the line includes its '\n'
    }

    exit(0);
}
