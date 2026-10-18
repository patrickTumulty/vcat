
#ifndef TUI_UTILS_H
#define TUI_UTILS_H

#include "utils.h"

void tuiUtilsDrawBox(int x, int y, int height, int width);
void tuiUtilsFillRegion(int x, int y, int height, int width, char v);
Point tuiUtilsGetCenteredOffset(const int height, const int width, const int maxheight, const int maxWidth);

#endif // TUI_UTILS_H
