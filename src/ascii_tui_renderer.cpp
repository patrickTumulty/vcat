
#include "ascii_tui_renderer.hpp"
#include <memory>

AsciiTUIRenderer::AsciiTUIRenderer(std::shared_ptr<VideoManager> vm)
    : _buffer(std::make_shared<greedy_matrix<char>>(10, 10)), _vm(vm)
{
}

void AsciiTUIRenderer::update()
{

    // _buffer->copy_from(imatrix<char> &destination)

        // TODO(ncurses): the drawing below needs the terminal, so it moves to vcat-tui together
        // with the TUISessionListener base. Kept here until the converter is split up.
        // std::scoped_lock lock(_asciiDataLock);

    //     int offsetX = std::max(1, (_terminalSize.width - _asciiData->width()) / 2);
    // int offsetY = std::max(0, (_terminalSize.height - _asciiData->height()) / 2);
    //
    // for (int i = 0; i < _asciiData->height(); i++)
    // {
    //     for (int j = 0; j < _asciiData->width(); j++)
    //     {
    //         mvaddch(i + offsetY + 1, j + offsetX + 1, _asciiData->get(j, i));
    //     }
    // }
    //
    // drawBox(offsetX, offsetY, _asciiData->height() + (2 * BORDER_MARGIN), _asciiData->width() + (2 * BORDER_MARGIN));
}

void AsciiTUIRenderer::onTerminalSizeChange(Rectangle newSize)
{
    _terminalSize = newSize;
    _vm->updateVideoBounds(newSize);
}

void AsciiTUIRenderer::onKeyPressed(char key)
{
}
