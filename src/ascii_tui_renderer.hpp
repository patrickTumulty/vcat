
#pragma once

#include "tui_session.hpp"
#include "video_manager.hpp"
#include <memory>

class AsciiTUIRenderer : public ITUIRenderer
{
  public:
    AsciiTUIRenderer(std::shared_ptr<VideoManager> vm);

    void drawNoDataMessage();
    void update() override;
    void onTerminalSizeChange(Rectangle newSize) override;
    void onKeyPressed(char key) override;

  private:
    Rectangle _terminalSize{0, 0};

    std::shared_ptr<VideoManager> _vm;
};
