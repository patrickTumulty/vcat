
#include "ascii_tui_renderer.hpp"
#include <cmath>
#include <memory>
#include <ncurses.h>

namespace
{
// Frames without new data before the stream is considered dead and the "NO DATA"
// message replaces the stale picture. At 35 Hz refresh this is about 2 seconds.
static constexpr int STALE_FRAME_LIMIT = 70;

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

} // namespace

AsciiTUIRenderer::AsciiTUIRenderer(std::shared_ptr<VideoManager> vm) : _vm(vm)
{
}

void AsciiTUIRenderer::drawNoDataMessage()
{
    constexpr int height = 5;
    constexpr int width = 11;
    if (_terminalSize.width < width || _terminalSize.height < height)
    {
        return; // Terminal too small for the box; writeRegion/mvaddstr are not bounds-guarded.
    }
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

    auto bufferOpt = queueReader->acquireLatest();

    if (bufferOpt.has_value())
    {
        if (_buffer != nullptr)
        {
            queueReader->release(_buffer);
        }
        _buffer = bufferOpt.value();
        _staleFrameCounter = 0;
    }
    else if (_buffer != nullptr)
    {
        _staleFrameCounter++;
        if (_staleFrameCounter >= STALE_FRAME_LIMIT)
        {
            queueReader->release(_buffer);
            _buffer = nullptr;
        }
    }

    if (_buffer == nullptr)
    {
        drawNoDataMessage();
        return;
    }

    int offsetX = std::max(1, (_terminalSize.width - _buffer->width()) / 2);
    int offsetY = std::max(0, (_terminalSize.height - _buffer->height()) / 2);
    int height = std::min(_buffer->height(), _terminalSize.height);
    int width = std::min(_buffer->width(), _terminalSize.width);
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
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

void AsciiTUIRenderer::onKeyPressed(int key)
{
}
