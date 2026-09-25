#ifndef __EXEC__
#define __EXEC__

#define MAXARG  16   // max exec arguments
#define MAXPATH 128  // max exec path length, including the NUL

int exec(char *path, char **argv);

#endif
