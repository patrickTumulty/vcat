
#ifndef VCAT_TUI_SESSION_H
#define VCAT_TUI_SESSION_H

#include "utils.h"
#include <stdbool.h>

/**
 * @brief Renderer interface for a TUISession.
 *
 * Implementations embed an ITUIRenderer as their FIRST member and register
 * themselves with the session; the session calls back with the registered
 * pointer, so implementations recover themselves with a plain cast.
 *
 * Renderers are pure views: they draw during update() and follow terminal
 * resizes. Keys are fed to registered input handlers instead.
 */
typedef struct ITUIRenderer
{
    void (*update)(struct ITUIRenderer *self);
    void (*onTerminalSizeChange)(struct ITUIRenderer *self, Rectangle newSize);
} ITUIRenderer;

/**
 * @brief Input handler interface for a TUISession.
 *
 * Implementations embed an ITUIInputHandler as their FIRST member and
 * register themselves with the session, the same way renderers do. The
 * session hands every key it does not own itself (ESC) to each registered
 * handler.
 */
typedef struct ITUIInputHandler
{
    void (*onKeyPressed)(struct ITUIInputHandler *self, int key);
} ITUIInputHandler;

/**
 * @brief Owns the ncurses session and the update loop.
 */
typedef struct TUISession TUISession;

/**
 * @brief Initializes ncurses (raw terminal, no echo, keys non-blocking).
 */
TUISession *vcatTuiSessionCreate(void);

/**
 * @brief Tears the terminal state down.
 */
void vcatTuiSessionFree(TUISession *session);

/**
 * @brief Runs the refresh loop until stop() or the ESC key; blocks the caller.
 */
void vcatTuiSessionRun(TUISession *session);

/**
 * @brief Asks the loop to exit at its next iteration. Safe to call from other threads.
 */
void vcatTuiSessionStop(TUISession *session);

/**
 * @brief Adds a renderer to the ones the loop drives. A renderer may register once.
 */
bool vcatTuiSessionRegisterRenderer(TUISession *session, ITUIRenderer *renderer);

/**
 * @brief Removes a renderer from the loop.
 */
void vcatTuiSessionUnregisterRenderer(TUISession *session, ITUIRenderer *renderer);

/**
 * @brief Adds an input handler to the ones the loop feeds keys. A handler may register once.
 */
bool vcatTuiSessionRegisterInputHandler(TUISession *session, ITUIInputHandler *handler);

/**
 * @brief Removes an input handler from the loop.
 */
void vcatTuiSessionUnregisterInputHandler(TUISession *session, ITUIInputHandler *handler);

#endif // VCAT_TUI_SESSION_H
