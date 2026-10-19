
#ifndef VCAT_VCAT_CONTROLLER_H
#define VCAT_VCAT_CONTROLLER_H

#include "osd_menu_renderer.h"
#include "tui_session.h"
#include "video_manager.h"

/**
 * @brief Owns the key bindings: the session feeds it every key, and each
 * binding mutates app state or drives a renderer command.
 *
 * Renderers stay pure views; this is the one place that maps keys to actions,
 * so new features land as new binding table rows.
 */
typedef struct VcatController VcatController;

/**
 * @brief Creates the controller.
 *
 * @param[in] videoManager Borrowed, must outlive the controller.
 * @param[in] osd Borrowed, must outlive the controller.
 */
VcatController *vcatVcatControllerCreate(VideoManager *videoManager, OsdTUIRenderer *osd);

/**
 * @brief Releases the controller. Everything it was given is borrowed and stays alive.
 */
void vcatVcatControllerFree(VcatController *controller);

/**
 * @brief The interface view to register with a TUISession; the base is the first member.
 */
ITUIInputHandler *vcatVcatControllerAsITUIInputHandler(VcatController *controller);

#endif // VCAT_VCAT_CONTROLLER_H
