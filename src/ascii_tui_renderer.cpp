
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

void writeRegion(int x, int y, int height, int width, char v)
{
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            mvaddch(i + y, j + x, v);
        }
    }
}

// Cells of margin between the picture and the border box drawn around it.
static constexpr int BORDER_MARGIN = 1;
// Cells reserved off each axis of the terminal for that box. The picture is fitted to the
// space left over, so it can never be wider than usable - 1 once the margin is added back,
// and the box always lands on screen.
static constexpr int BORDER_RESERVED = 2 * BORDER_MARGIN;

AsciiTUIRenderer::AsciiTUIRenderer(std::shared_ptr<VideoManager> vm) : _vm(vm)
{
}

void AsciiTUIRenderer::drawNoDataMessage()
{
    int height = 5;
    int width = 11;
    int offsetX = std::max(1, (_terminalSize.width - width) / 2);
    int offsetY = std::max(0, (_terminalSize.height - height) / 2);
    drawBox(offsetX, offsetY, height, width);
    writeRegion(offsetX + 1, offsetY + 1, height - 2, width - 2, ' ');
    mvaddstr(offsetY + 2, offsetX + 2, "NO DATA");
}
void AsciiTUIRenderer::update()
{
    auto queueReader = _vm->getAsciiDataQueue();
    if (queueReader == nullptr)
    {
        return;
    }

    static std::shared_ptr<imatrix<char>> buffer = nullptr;
    static int staleFrameCounter = 0;

    auto bufferOpt = queueReader->acquireLatest();

    if (!bufferOpt.has_value() && buffer == nullptr)
    {
        drawNoDataMessage();
        return;
    }
    else if (bufferOpt.has_value())
    {
        buffer = bufferOpt.value();
        staleFrameCounter = 0;
    }

    int offsetX = std::max(1, (_terminalSize.width - buffer->width()) / 2);
    int offsetY = std::max(0, (_terminalSize.height - buffer->height()) / 2);
    int height = std::min(buffer->height(), _terminalSize.height);
    int width = std::min(buffer->width(), _terminalSize.width);
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
        {
            mvaddch(i + offsetY, j + offsetX, buffer->get(j, i));
        }
    }

    if (!bufferOpt.has_value())
    {
        staleFrameCounter++;
        if (staleFrameCounter >= 80)
        {
            drawNoDataMessage();
        }
    }

    queueReader->release(buffer);
}

void AsciiTUIRenderer::onTerminalSizeChange(Rectangle newSize)
{
    _terminalSize = newSize;
    _vm->updateVideoBounds(newSize);
}

void AsciiTUIRenderer::onKeyPressed(char key)
{
}
