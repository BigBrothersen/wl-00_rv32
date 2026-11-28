#include "user.h"

int main(int argc, char *argv[]) {
    if (argv[1] == NULL) {
        printf("error\n");
        exit(-1);
    }
    int pid = stoi(argv[1]);
    if (pid <= 1) {
        printf("Attempting to kill invalid process\n");
        exit(-1);
    }
    if (kill(pid) < 0) {
        printf("Failed to kill process %d\n", pid);
        exit(-1);
    }
    printf("Successfully killed process %d\n", pid);
    exit(0);
}