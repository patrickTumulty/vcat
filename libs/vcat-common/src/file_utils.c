

#include "file_utils.h"
#include <libgen.h>
#include <limits.h>
#include <linux/limits.h>
#include <stdio.h>
#include <unistd.h>

const char *resolveExePath()
{
    static char exePath[PATH_MAX];
    if (exePath[0] != '\0')
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
    return exePath;
}
