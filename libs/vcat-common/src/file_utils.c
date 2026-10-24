
#include "file_utils.h"
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <linux/limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

const char *resolveExePath()
{
    static bool exeSet = false;
    static char exePath[PATH_MAX] = "\0";
    if (exeSet)
    {
        return exePath;
    }
    char tmp[PATH_MAX];
    const ssize_t exeLen = readlink("/proc/self/exe", tmp, sizeof(tmp) - 1);
    if (exeLen <= 0)
    {
        return "";
    }
    tmp[exeLen] = '\0';
    (void)snprintf(exePath, sizeof(exePath), "%s", dirname(tmp));
    exeSet = true;
    return exePath;
}

int mkdirp(const char *path, mode_t mode)
{
    char tmp[PATH_MAX];
    size_t len = strlen(path);

    if (len == 0 || len >= sizeof(tmp))
    {
        errno = ENAMETOOLONG;
        return -1;
    }

    strcpy(tmp, path);

    for (char *p = tmp + 1; *p; ++p)
    {
        if (*p == '/')
        {
            *p = '\0';

            if (mkdir(tmp, mode) != 0 && errno != EEXIST)
                return -1;

            *p = '/';
        }
    }

    if (mkdir(tmp, mode) != 0 && errno != EEXIST)
        return -1;

    return 0;
}

bool fileExists(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file != NULL)
    {
        fclose(file);
        return true;
    }
    return false;
}
