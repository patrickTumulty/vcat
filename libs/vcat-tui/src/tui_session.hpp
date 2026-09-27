
#pragma once

#include "utils.hpp"
#include <atomic>
#include <memory>
#include <vector>

class ITUIRenderer
{
  public:
    virtual void update() = 0;
    virtual void onTerminalSizeChange(Rectangle newSize) = 0;
    virtual void onKeyPressed(char key) = 0;
};

class TUISession
{
  public:
    TUISession();
    ~TUISession();

    void run();
    void stop();

    void registerRenderer(std::shared_ptr<ITUIRenderer> listener);
    void unregisterRenderer(std::shared_ptr<ITUIRenderer> listener);

  private:
    void onTerminalSizeChange();
    void updatePresentationWindow();

    Rectangle _currentTermSize;
    int _updateDeltaMillis = 0;
    std::atomic<bool> _running{false};
    std::vector<std::shared_ptr<ITUIRenderer>> _renderer;
};
