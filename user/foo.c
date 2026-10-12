#include "user.h"

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (i < argc - 1) printf("%s ", argv[i]);
        else printf("%s", argv[i]);
    }
    printf("\n");
    return 0;
}