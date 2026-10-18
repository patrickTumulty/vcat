
#ifndef TUI_UTILS_H
#define TUI_UTILS_H

#include "utils.h"

/**
 * @brief Draws a box of ncurses line characters at the given cell.
 *
 * Silently does nothing unless the whole box fits the terminal.
 */
void tuiUtilsDrawBox(int x, int y, int height, int width);

/**
 * @brief Fills a rectangle of cells with the given character.
 *
 * Not bounds-guarded: the caller must keep the rectangle inside the terminal.
 */
void tuiUtilsFillRegion(int x, int y, int height, int width, char v);

/**
 * @brief Offset that centers a height x width region inside a maxHeight x maxWidth one.
 *
 * @return The top-left cell of the centered region; zeroed on the axes where
 * the region does not fit.
 */
Point tuiUtilsGetCenteredOffset(const int height,
                                const int width,
                                const int maxHeight,
                                const int maxWidth);

#endif // TUI_UTILS_H
