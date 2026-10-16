
#pragma once

#include "tui_session.hpp"
#include "video_manager.hpp"
#include <memory>

class AsciiTUIRenderer : public ITUIRenderer
{
  public:
    AsciiTUIRenderer(std::shared_ptr<VideoManager> vm);
    ~AsciiTUIRenderer();

    void update() override;
    void onTerminalSizeChange(Rectangle newSize) override;
    void onKeyPressed(int key) override;

  private:
    void drawNoDataMessage();

    Rectangle _terminalSize{0, 0};

    std::shared_ptr<VideoManager> _vm;

    /// Borrowed from the mailbox: released back to it before this renderer goes away.
    matrix_i8_t _buffer = nullptr;
    int _staleFrameCounter = 0;
};
