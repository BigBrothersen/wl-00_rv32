// init: pid 1, the first user process. The kernel starts it (uvmfirst) with
// no arguments, and every other process descends from it. It must never exit:
// kexit hands orphans to pid 1, and only init can reap them.
// For now it runs a few programs one after another; in M3 it will start the
// shell and restart it whenever it exits.
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
    char *echo_argv[] = { "echo", "hello", "from", "init", 0 };
    char *hello_argv[] = { "hello", "a", "b", 0 };
    char *nope_argv[] = { "nope", 0 }; // no such program: shows the failure path

    printf("init: starting (pid %d)\n", getpid());
    run(echo_argv);
    run(hello_argv);
    run(nope_argv);

    // Never return. Keep reaping orphans; with no children at all wait fails
    // at once, so sleep instead of spinning the CPU.
    for (;;) {
        if (wait(0) < 0)
            sleep(100);
    }
}
