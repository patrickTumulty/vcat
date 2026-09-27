
#pragma once

/**
 * Owns every global GStreamer setup step: initializing the library and routing GStreamer's
 * debug output into the vcat logging facade. Call this before creating any GstElement.
 */
void initGStreamer();
