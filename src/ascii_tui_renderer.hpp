
#pragma once

#include "tui_session.hpp"
#include "video_manager.hpp"
#include <memory>

class AsciiTUIRenderer : public ITUIRenderer
{
  public:
    AsciiTUIRenderer(std::shared_ptr<VideoManager> vm);

    void update() override;
    void onTerminalSizeChange(Rectangle newSize) override;
    void onKeyPressed(int key) override;

  private:
    void drawNoDataMessage();

    Rectangle _terminalSize{0, 0};

    std::shared_ptr<VideoManager> _vm;

    std::shared_ptr<imatrix<char>> _buffer = nullptr;
    int _staleFrameCounter = 0;
};
