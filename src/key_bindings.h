
#ifndef VCAT_KEY_BINDINGS_H
#define VCAT_KEY_BINDINGS_H

typedef struct VcatController VcatController;

/**
 * @brief One row of the binding table: what a key does, and how the help menu
 * shows it.
 *
 * The controller defines the table and dispatches keys through it; the OSD
 * help menu renders the same rows, so keys and help text cannot drift apart.
 */
typedef struct VcatKeyBinding
{
    int key;                 // The key as getch() reports it, e.g. '?'.
    const char *input;       // Help column: how the key is shown, e.g. "ESC".
    const char *description; // Help column: what the key does.
    void (*action)(VcatController *controller);
} VcatKeyBinding;

#endif // VCAT_KEY_BINDINGS_H
