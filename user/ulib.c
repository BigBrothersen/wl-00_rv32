// Program startup and the string/memory helpers. There is no C library, and
// GCC may emit calls to memcpy/memset/memmove by itself (struct copies, big
// initialisers), so they must exist even if no program calls them directly.
#include "user.h"

int main(int argc, char *argv[]);

// The entry point (user.ld: ENTRY(start)). exec starts it with a0 = argc and
// a1 = argv, which is where a C function gets its first two arguments. Never
// returns: nothing called it, so ra is garbage; main's result goes to exit.
void start(int argc, char *argv[]) {
    exit(main(argc, argv));
}

unsigned int strlen(const char *str)
{
    unsigned int count = 0;
    while (*str++ != '\0') {
        count++;
    }
    return count;
}


// 1 if c is one of the characters in delim
static int is_delim(char c, const char *delim) {
    for (; *delim != '\0'; delim++) {
        if (c == *delim) return 1;
    }
    return 0;
}

// Splits str into tokens separated by any character in delim (e.g. " \t\n").
// First call: pass the string. Later calls: pass NULL to continue where the
// last one stopped. Returns the next token, or NULL when there are no more.
// Modifies str (writes '\0' over each delimiter that ends a token) and keeps
// its position in a static, so only one string can be tokenized at a time.
char *strtok(char *str, const char *delim) {
    static char *next; // where the next search starts

    if (str != NULL) next = str;
    if (next == NULL) return NULL;

    // skip leading delimiters
    while (*next != '\0' && is_delim(*next, delim)) next++;
    if (*next == '\0') {
        next = NULL; // no tokens left
        return NULL;
    }

    // the token runs until the next delimiter or the end of the string
    char *token = next;
    while (*next != '\0' && !is_delim(*next, delim)) next++;

    if (*next != '\0') {
        *next = '\0'; // terminate this token
        next++;       // next search starts after it
    }
    else {
        next = NULL;  // hit the end: the next call returns NULL
    }
    return token;
}

// Returns 0 if a and b are equal, <0 if a sorts before b, >0 if after
int strcmp(const char *a, const char *b) {
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

void *memset(void *dst, int val, unsigned int len) {
    char *ptr = (char *)dst;
    while (len-- > 0) *ptr++ = val;
    return dst;
}

// Safe when the areas overlap: copy forwards if dst is below src, otherwise
// backwards from the end, so no byte is overwritten before it is copied.
void *memmove(void *dst, const void *src, unsigned int len) {
    char *d = dst;
    const char *s = src;
    if (d < s) {
        while (len--) *d++ = *s++;
    }
    else {
        d += len; // one past the last byte
        s += len;
        while (len--) *--d = *--s;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, unsigned int len) {
    return memmove(dst, src, len);
}