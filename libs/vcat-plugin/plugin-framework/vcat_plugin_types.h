
#ifndef VCAT_PLUGIN_TYPES_H
#define VCAT_PLUGIN_TYPES_H

typedef enum
{
    PLUGIN_TYPE_NONE = 0,
    PLUGIN_TYPE_VIDEO_SOURCE = 1
} PluginType;


typedef enum
{
    RC_OK = 0,
    RC_NOT_INITIALIZED,
    RC_FAIL,
    RC_BAD_INPUT_ARGS,
    RC_MEM_ALLOC_ERROR,
    RC_ALREADY_DONE,
} Rc;

#endif // VCAT_PLUGIN_TYPES_H
