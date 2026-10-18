
#include "tui_utils.h"
#include <ncurses.h>

void tuiUtilsDrawBox(int x, int y, int height, int width)
{
    if (!(width >= 2 && height >= 2 && x >= 0 && y >= 0 && x + width <= COLS &&
          y + height <= LINES))
    {
        return;
    }

    mvhline(y, x + 1, ACS_HLINE, width - 2);              // Top Line
    mvhline(y + height - 1, x + 1, ACS_HLINE, width - 2); // Bottom Line

    mvvline(y + 1, x, ACS_VLINE, height - 2);             // Left Line
    mvvline(y + 1, x + width - 1, ACS_VLINE, height - 2); // Right Line

    mvaddch(y, x, ACS_ULCORNER);                          // Upper Left
    mvaddch(y, x + width - 1, ACS_URCORNER);              // Upper Right
    mvaddch(y + height - 1, x, ACS_LLCORNER);             // Lower Left
    mvaddch(y + height - 1, x + width - 1, ACS_LRCORNER); // Lower Right
}

void tuiUtilsFillRegion(int x, int y, int height, int width, char v)
{
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            mvaddch(i + y, j + x, v);
        }
    }
}

Point tuiUtilsGetCenteredOffset(const int height,
                                const int width,
                                const int maxheight,
                                const int maxWidth)
{
    Point point = {0};
    int diffWidth = maxWidth - width;
    point.x = diffWidth > 0 ? (diffWidth) / 2 : 0;
    int diffHeight = maxheight - height;
    point.y = diffHeight > 0 ? (diffHeight) / 2 : 0;
    return point;
}
