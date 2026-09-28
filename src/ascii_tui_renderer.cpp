
#include "ascii_tui_renderer.hpp"
#include "logging.hpp"
#include <cmath>
#include <memory>
#include <ncurses.h>

void drawBox(int x, int y, int height, int width)
{
    if (!(width >= 2 && height >= 2 && x >= 0 && y >= 0 && x + width <= COLS && y + height <= LINES))
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

// Cells of margin between the picture and the border box drawn around it.
static constexpr int BORDER_MARGIN = 1;
// Cells reserved off each axis of the terminal for that box. The picture is fitted to the
// space left over, so it can never be wider than usable - 1 once the margin is added back,
// and the box always lands on screen.
static constexpr int BORDER_RESERVED = 2 * BORDER_MARGIN;

AsciiTUIRenderer::AsciiTUIRenderer(std::shared_ptr<VideoManager> vm)
    : _buffer(std::make_shared<greedy_matrix<char>>(10, 10)), _vm(vm)
{
}

void AsciiTUIRenderer::update()
{
    _buffer->copy_from(*_vm->getAsciiData().get());

    int offsetX = std::max(1, (_terminalSize.width - _buffer->width()) / 2);
    int offsetY = std::max(0, (_terminalSize.height - _buffer->height()) / 2);

    for (int i = 0; i < _buffer->height(); i++)
    {
        for (int j = 0; j < _buffer->width(); j++)
        {
            mvaddch(i + offsetY, j + offsetX, _buffer->get(j, i));
        }
    }
}

void AsciiTUIRenderer::onTerminalSizeChange(Rectangle newSize)
{
    _terminalSize = newSize;
    _vm->updateVideoBounds(newSize);
}

void AsciiTUIRenderer::onKeyPressed(char key)
{
}
