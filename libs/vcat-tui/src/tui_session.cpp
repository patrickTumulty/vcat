
#include "tui_session.hpp"
#include "logging.hpp"
#include <algorithm>
#include <cmath>
#include <exception>
#include <memory>
#include <ncurses.h>
#include <thread>
#include <utility>

constexpr float TUI_REFRESH_RATE_HZ = 20.0f;

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
    logging::info("on terminal size change");
    getmaxyx(stdscr, _currentTermSize.height, _currentTermSize.width);
    for (auto listener : _renderer)
        listener->onTerminalSizeChange(_currentTermSize);
}

void TUISession::registerRenderer(std::shared_ptr<ITUIRenderer> listener)
{
    _renderer.push_back(listener);
}

void TUISession::unregisterRenderer(std::shared_ptr<ITUIRenderer> listener)
{
    _renderer.erase(std::remove(_renderer.begin(), _renderer.end(), listener), _renderer.end());
}

void TUISession::run()
{
    _running = true;

    logging::info("** Starting TUI session");

    onTerminalSizeChange();

    while (_running)
    {
        werase(stdscr);

        for (auto r : _renderer)
            r->update();

        wnoutrefresh(stdscr);
        doupdate();

        int ch = getch();
        if (ch == ERR)
        {
            // Do Nothing - no key pressed
        }
        if (ch == KEY_RESIZE)
        {
            onTerminalSizeChange();
        }
        else if (ch == 27) // ESC
        {
            logging::info("Escape key hit: exiting UI loop");
            _running = false;
            break;
        }
        else
        {
            for (auto r : _renderer)
                r->onKeyPressed(ch);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(_updateDeltaMillis));
    }
}

void TUISession::stop()
{
    logging::info("** Stopping TUI session");
    _running = false;
}
