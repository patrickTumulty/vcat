
#include "tui_session.hpp"
#include "logging.hpp"
#include <algorithm>
#include <cmath>
#include <memory>
#include <ncurses.h>
#include <thread>

constexpr float TUI_REFRESH_RATE_HZ = 35.0f;

TUISession::TUISession() : _updateDeltaMillis(static_cast<int>(1000.0f / TUI_REFRESH_RATE_HZ))
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
    for (const auto &listener : _renderer)
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

    auto nextFrame = std::chrono::steady_clock::now();

    while (_running)
    {
        nextFrame += _updateDeltaMillis;

        werase(stdscr);

        for (const auto &r : _renderer)
            r->update();

        wnoutrefresh(stdscr);
        doupdate();

        int ch = getch();
        if (ch == ERR)
        {
            // Do nothing - no key pressed
        }
        else if (ch == KEY_RESIZE)
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
            for (const auto &r : _renderer)
                r->onKeyPressed(ch);
        }

        const auto now = std::chrono::steady_clock::now();
        if (nextFrame < now)
        {
            nextFrame = now;
        }
        std::this_thread::sleep_until(nextFrame);
    }
}

void TUISession::stop()
{
    logging::info("** Stopping TUI session");
    _running = false;
}
