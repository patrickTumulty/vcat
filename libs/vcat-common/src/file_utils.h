
#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <sys/types.h>

const char* resolveExePath();
int mkdirp(const char *path, mode_t mode);

#endif // FILE_UTILS_H
