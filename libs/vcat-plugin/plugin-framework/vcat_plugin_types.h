
#ifndef VCAT_PLUGIN_TYPES_H
#define VCAT_PLUGIN_TYPES_H

typedef enum
{
    PLUGIN_TYPE_NONE = 0,
    PLUGIN_TYPE_VIDEO_SOURCE = 1
} PluginType;

#define VCAT_DEFINE_PLUGIN_NAME(NAME) \
    char *vcatPluginGetName()         \
    {                                 \
        return (NAME);                \
    }

#define VCAT_DEFINE_PLUGIN_TYPE(TYPE) \
    PluginType vcatPluginGetType()    \
    {                                 \
        return (TYPE);                \
    }

#endif // VCAT_PLUGIN_TYPES_H
