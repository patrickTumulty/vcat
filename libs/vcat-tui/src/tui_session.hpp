
#pragma once

#include "utils.hpp"
#include <memory>
#include <vector>

class ITUISessionListener
{
  public:
    virtual void onTerminalUpdate() = 0;
    virtual void onTerminalSizeChange(Rectangle newSize) = 0;
};

class TUISession
{
  public:
    TUISession();
    ~TUISession();

    void run();

    void addTUISessionListener(std::shared_ptr<ITUISessionListener> listener);
    void removeTUISessionListener(std::shared_ptr<ITUISessionListener> listener);

  private:
    void onTerminalSizeChange();
    void updatePresentationWindow();

    Rectangle _currentTermSize;
    int _updateDeltaMillis = 0;
    std::vector<std::shared_ptr<ITUISessionListener>> _listeners;
};
