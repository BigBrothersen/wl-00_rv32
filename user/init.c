#include "user.h"

// Run one program and wait for it. Returns its exit status, or -1.
static int run(char *argv[]) {
    int pid = fork();
    int status;
    if (pid < 0) {
        printf("init: fork failed\n");
        return -1;
    }
    // If child
    if (pid == 0) {
        if (exec(argv[0], argv) < 0) {
            printf("init: exec %s failed\n", argv[0]);
            exit(1); // or the child would carry on as a second init
        }
    }
    else {
        // Other pids are orphans kexit handed to init: reap them and go on
        while (1) {
            int w = wait(&status);
            if (w == pid) break;
            if (w < 0) return -1;
        }
    }
    printf("init: %s (pid %d) exited with status %d\n", argv[0], pid, status);
    return status;
}

int main(void) {
    if (getpid() != 1) {
        printf("ERROR init: multiple instance\n");
        exit(1);
    }
    // char *echo_argv[] = { "echo", "hello", "from", "init", 0 };
    char *terminal_argv[] = {"terminal", 0};

    printf("init: starting (pid %d)\n", getpid());
    
    for (;;) {
        run(terminal_argv);
        if (wait(0) < 0)
            sleep(100);
    }
}
