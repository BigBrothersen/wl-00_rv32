// terminal: test for the read syscall. Reads stdin line by line and prints
// what read() returned. Ctrl-D on an empty line (read returns 0) ends it.
#include "user.h"

int main(void) {
    printf("Welcome to terminal: type a line, Ctrl-D to quit\n");
    char buf[MAXBUF];
    while (1) {
        char *argv[MAXARGS];
        int argc = 0;
        printf("$ ");

        // Read and parse command
        int n = read(0, buf, sizeof(buf)-1);
        if (n < 0) {
            printf("read error %d\n", n);
            exit(1);
        }
        if (n == 0) {
            printf("\nterminal: EOF\n");
            break;
        }
        // Get rid of newline
        buf[n] = '\0';
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r'))
            buf[--n] = '\0';

        // Parse commands
        for (char *tok = strtok(buf, " \t"); tok != NULL && argc < MAXARGS - 1;
            tok = strtok(NULL, " \t")) {
            argv[argc++] = tok;
        }
        argv[argc] = 0;            // exec needs a null-terminated argv
        if (argc == 0) continue;

        // Debug
        // for (int i = 0; i < argc; i++) {
        //     printf("[%s] ", argv[i]);
        // }
        // printf("\n");

        int pid = fork();
        int status;
        if (pid < 0) {
            printf("ERROR: fork failed");
        }
        if (pid == 0) {
            if (exec(argv[0], argv) < 0) {
                printf("init: exec %s failed\n", argv[0]);
                exit(1); // or the child would carry on as a second init
            }
        }
        else {
            while (1) {
                int w = wait(&status);
                if (w == pid) break;
                if (w < 0) return -1;
            }
        } 
    }
    printf("Terminal exited\n");

    exit(0);
}
