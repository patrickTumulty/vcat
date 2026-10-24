
#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdbool.h>
#include <sys/types.h>

const char *resolveExePath();
int mkdirp(const char *path, mode_t mode);
bool fileExists(const char *filename);

#endif // FILE_UTILS_H
