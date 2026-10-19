
#ifndef VCAT_GSTREAMER_INIT_H
#define VCAT_GSTREAMER_INIT_H

/**
 * @brief Owns every global GStreamer setup step: initializing the library and routing
 * GStreamer's debug output into the vcat logging facade. Call this before creating any
 * GstElement.
 */
void vcatInitGStreamer(void);

#endif // VCAT_GSTREAMER_INIT_H
