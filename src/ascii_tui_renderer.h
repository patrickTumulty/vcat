
#ifndef VCAT_ASCII_TUI_RENDERER_H
#define VCAT_ASCII_TUI_RENDERER_H

#include "tui_session.h"
#include "video_manager.h"

typedef struct AsciiTUIRenderer AsciiTUIRenderer;

AsciiTUIRenderer *vcatAsciiTuiRendererCreate(VideoManager *videoManager);

void vcatAsciiTuiRendererFree(AsciiTUIRenderer *renderer);

ITUIRenderer *vcatAsciiTuiRendererAsITUIRenderer(AsciiTUIRenderer *renderer);

void vcatAsciiTuiRendererToggleFreezeFrame(AsciiTUIRenderer *renderer);

void vcatAsciiTuiRendererToggleColor(AsciiTUIRenderer *renderer);

/// Sets whether cells draw with the frame's averaged colors; only takes effect
/// on color-capable terminals. Used for the start-up `-c` flag.
void vcatAsciiTuiRendererSetColor(AsciiTUIRenderer *renderer, bool enabled);

#endif // VCAT_ASCII_TUI_RENDERER_H
