
#include "tui_session.hpp"
#include <algorithm>
#include <memory>
#include <ncurses.h>
#include <thread>

constexpr float TUI_REFRESH_RATE_HZ = 60.0f;

TUISession::TUISession() : _updateDeltaMillis(1 / TUI_REFRESH_RATE_HZ)
{
    initscr();
    noecho();
    cbreak();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);

    getmaxyx(stdscr, _currentTermSize.height, _currentTermSize.width);
}

TUISession::~TUISession()
{
    endwin();
}

void TUISession::onTerminalSizeChange()
{
    getmaxyx(stdscr, _currentTermSize.height, _currentTermSize.width);
    for (auto listener : _listeners)
        listener->onTerminalSizeChange(_currentTermSize);
}

void TUISession::addTUISessionListener(std::shared_ptr<ITUISessionListener> listener)
{
    _listeners.push_back(listener);
}

void TUISession::removeTUISessionListener(std::shared_ptr<ITUISessionListener> listener)
{
    _listeners.erase(std::remove(_listeners.begin(), _listeners.end(), listener), _listeners.end());
}

void TUISession::run()
{
    onTerminalSizeChange();

    while (true)
    {
        werase(stdscr);

        for (auto listener : _listeners)
            listener->onTerminalUpdate();

        wnoutrefresh(stdscr);
        doupdate();

        int ch = getch();
        if (ch == KEY_RESIZE)
        {
            onTerminalSizeChange();
        }
        else if (ch == 27) // ESC
        {
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(_updateDeltaMillis));
    }
}
