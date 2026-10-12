
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

#define VCAT_DEFINE_PLUGIN_VERSION(VERSION) \
    const char *vcatPluginGetVersion()      \
    {                                       \
        return (VERSION);                   \
    }

#define VERIFY_ELEMENT(E, MSG)                               \
    if (!(E))                                                \
    {                                                        \
        lfLogError("%s: GST Element %s is NULL", (MSG), #E); \
        return RC_ERROR;                                     \
    }

#endif // VCAT_PLUGIN_TYPES_H
