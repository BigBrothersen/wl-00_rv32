#include "proc.h"
#include "kprint.h"
#include "exec.h"
#include "hellocode.h"

// initcode.h is compiled into proc.c; borrow its blob from there.
extern unsigned char user_init_bin[];
extern unsigned int user_init_bin_len;

// No filesystem yet, so exec looks programs up by name in this table of
// binaries embedded by the Makefile (xxd -i). Replace with a real lookup
// once the filesystem lands.
struct program {
    char *name;
    unsigned char *data;
    unsigned int *len;
};

static struct program programs[] = {
    { "init",  user_init_bin,  &user_init_bin_len },
    { "hello", user_hello_bin, &user_hello_bin_len },
};

static int streq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static struct program *findprog(const char *path)
{
    for (int i = 0; i < sizeof(programs) / sizeof(programs[0]); i++) {
        if (streq(programs[i].name, path))
            return &programs[i];
    }
    return NULL;
}

// Replace the current process image with the embedded program named path.
// argv is a NULL-terminated array of kernel strings. Returns argc on
// success; on failure returns -1 and leaves the caller's image untouched.
int exec(char *path, char **argv)
{
    struct proc *p = this_cpu()->proc;
    struct program *prog;
    pagetable_t pt = NULL;
    uint32_t sz = 0;
    char *stack;
    uint32_t sp, ustack[MAXARG + 1];
    int argc;

    // 2. Find the program image
    if ((prog = findprog(path)) == NULL)
        return -1;

    // 3. Fresh page table with only the kernel mappings
    if ((pt = init_userpt()) == NULL)
        return -1;

    // 4. Load the program at USER_TEXT. Until the load succeeds, sz covers
    // every page it may have mapped so the bad path frees them all.
    sz = USER_TEXT + PGROUNDUP(*prog->len);
    if (sz >= USER_STACK_TOP - PAGE_SIZE) {
        sz = 0;
        goto bad;
    }
    if (uvmload(pt, prog->data, *prog->len) != sz)
        goto bad;

    // 5. Fresh stack. Strings go at the top, then the argv[] pointer
    // array, and sp ends up pointing at argv[0]. The stack page is
    // identity mapped in the kernel, so it is written through its PA.
    if ((stack = uvmstack(pt)) == NULL)
        goto bad;
    sp = USER_STACK_TOP;
    for (argc = 0; argv[argc]; argc++) {
        if (argc >= MAXARG)
            goto bad;
        int len = strlen(argv[argc]) + 1;
        sp -= len;
        sp -= sp % 4;
        if (sp < USER_STACK_TOP - PAGE_SIZE + (MAXARG + 1) * 4 + 16)
            goto bad;
        memmove(stack + (sp - (USER_STACK_TOP - PAGE_SIZE)), argv[argc], len);
        ustack[argc] = sp;
    }
    ustack[argc] = 0;
    sp -= (argc + 1) * 4;
    sp -= sp % 16;
    memmove(stack + (sp - (USER_STACK_TOP - PAGE_SIZE)), ustack, (argc + 1) * 4);

    // 6. Commit: swap in the new image and free the old one
    pagetable_t old_pt = p->pt;
    uint32_t old_sz = p->sz;
    p->pt = pt;
    p->sz = sz;
    p->tf->epc = USER_TEXT;
    p->tf->regs[2] = sp;    // sp
    p->tf->regs[11] = sp;   // a1 = argv (a0 = argc via the syscall return)
    safestrcpy(p->name, prog->name, sizeof(p->name));
    uvmfree(old_pt, old_sz);

    // 7. Return argc; syscall() stores it in a0
    return argc;

bad:
    uvmfree(pt, sz);
    return -1;
}

