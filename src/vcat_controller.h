
#ifndef VCAT_VCAT_CONTROLLER_H
#define VCAT_VCAT_CONTROLLER_H

#include "key_bindings.h"
#include "osd_menu_renderer.h"
#include "tui_session.h"
#include "video_manager.h"

/**
 * @brief Owns the key bindings: the session feeds it every key, and each
 * binding mutates app state or drives a renderer command.
 *
 * Renderers stay pure views; the binding table is the one place that maps keys
 * to actions, and the OSD help menu renders the very same rows, so new
 * features land as new binding table rows.
 */
typedef struct VcatController VcatController;

/**
 * @brief Creates the controller.
 *
 * @param[in] tuiSession Borrowed, must outlive the controller.
 * @param[in] videoManager Borrowed, must outlive the controller.
 * @param[in] osd Borrowed, must outlive the controller.
 */
VcatController *vcatVcatControllerCreate(TUISession *tuiSession,
                                         VideoManager *videoManager,
                                         OsdTUIRenderer *osd);

/**
 * @brief Releases the controller. Everything it was given is borrowed and stays alive.
 */
void vcatVcatControllerFree(VcatController *controller);

/**
 * @brief The interface view to register with a TUISession; the base is the first member.
 */
ITUIInputHandler *vcatVcatControllerAsITUIInputHandler(VcatController *controller);

/**
 * @brief The binding table the controller dispatches keys through.
 *
 * The same rows render as the OSD help menu, so keys and help text cannot
 * drift apart. Outputs are cleared when the controller is NULL.
 */
void vcatVcatControllerGetKeyBindings(const VcatController *controller,
                                      const VcatKeyBinding **bindings,
                                      int *bindingsLen);

#endif // VCAT_VCAT_CONTROLLER_H
