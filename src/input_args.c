
#include "input_args.h"
#include "ascii_gradients.h"
#include "color_utils.h"
#include "crox_utils.h"
#include "logging.h"
#include "vcat_plugin_api.h"
#include "vcat_plugin_framework.h"
#include <bits/getopt_core.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Matches a <source> command line token against a plugin's URI help
 * string.
 *
 * The match key is the URI scheme: everything up to "://" (e.g. "udp" in
 * "udp://<host>:<port>"), or the whole string for sources without one. A token
 * matches it either bare ("udp") or as a full URI on that scheme
 * ("udp://127.0.0.1:1234").
 */
static bool sourceTokenMatchesPlugin(const char *token, VCatPluginHandle plugin)
{
    const char *uriHelp = vcatVideoSourceGetUriHelp(plugin);

    const char *schemeEnd = strstr(uriHelp, "://");
    size_t keyLen = schemeEnd == NULL ? strlen(uriHelp) : (size_t)(schemeEnd - uriHelp);

    if (strncmp(token, uriHelp, keyLen) != 0)
    {
        return false;
    }

    if (token[keyLen] == '\0')
    {
        return true; // Bare scheme token, e.g. "vcat udp --help"
    }

    // Full URI token, e.g. "udp://127.0.0.1:1234"
    return strncmp(&token[keyLen], "://", 3) == 0;
}

static void printMainHelp(const VCatPluginHandle *plugins, int pluginsLen)
{
    printf("vcat - v%s\n"
           "\n"
           "Usage: vcat [OPTIONS] <SOURCE> [ARGS...]\n"
           "\n"
           "Options:\n"
           "  -h, --help            Show this help\n"
           "  -v, --version         Show version information\n"
           "  -c, --color-mode N    Start with color mode number N (default: 0)\n"
           "  -g, --gradient N      Start with gradient number N (default: 0)\n"
           "      --show-gradients  Print gradient numbers, names and ramps, then exit\n"
           "      --last-log        Print the previous run's log to stdout\n"
           "\n"
           "Gradients:\n",
           VCAT_VERSION);

    for (int i = 0; i < gAsciiGradientsLen; i++)
    {
        printf("  %d  %s\n", i, gAsciiGradients[i].name);
    }

    printf("\nColor modes:\n");
    for (VcatColorMode mode = VCAT_COLOR_MODE_NO_COLOR; mode < VCAT_COLOR_MODE_COUNT; mode++)
    {
        printf("  %d  %s\n", mode, vcatColorModeName(mode));
    }

    printf("\nSources:\n");

    // The descriptions column starts past the widest URI help string; pad that
    // minimum so the two sources align even with short names.
    int uriWidth = 4;
    for (int i = 0; i < pluginsLen; i++)
    {
        if (vcatPluginGetType(plugins[i]) != PLUGIN_TYPE_VIDEO_SOURCE)
        {
            continue;
        }
        int len = (int)strlen(vcatVideoSourceGetUriHelp(plugins[i]));
        if (len > uriWidth)
        {
            uriWidth = len;
        }
    }

    for (int i = 0; i < pluginsLen; i++)
    {
        VCatPluginHandle p = plugins[i];
        if (vcatPluginGetType(p) != PLUGIN_TYPE_VIDEO_SOURCE)
        {
            continue;
        }
        printf("  %-*s   %s\n", uriWidth, vcatVideoSourceGetUriHelp(p), vcatVideoSourceGetDescription(p));
    }

    printf("\n"
           "Network sources take a URI ('<scheme>://<uri>'); other sources take\n"
           "their arguments as plain tokens ('<source> <args>').\n"
           "\n"
           "Run 'vcat <source> --help' for source-specific options.\n");
}

InputArgsResult vcatProcessInputArgs(int argc,                       //
                                     char *argv[],                   //
                                     VCatPluginHandle *plugins,      //
                                     const int pluginsLen,           //
                                     VCatPluginHandle *targetPlugin, //
                                     VideoSourceInfo *sourceInfo,
                                     VcatUserArgs *userArgs)
{
    // Long-only options get values past the character range: they can never
    // collide with a short option added later.
    enum
    {
        OPT_LAST_LOG = 256,
        OPT_SHOW_GRADIENTS
    };

    static const struct option cliOptions[] = {
        {"version",        no_argument,       NULL, 'v'               },
        {"help",           no_argument,       NULL, 'h'               },
        {"color-mode",     required_argument, NULL, 'c'               },
        {"last-log",       no_argument,       NULL, OPT_LAST_LOG      },
        {"show-gradients", no_argument,       NULL, OPT_SHOW_GRADIENTS},
        {NULL,             0,                 NULL, 0                 }  //
    };

    // The "+" disables GNU argv permutation: option parsing stops at the first
    // non-option, so "<source> [ARGS...]" options belong to the source, not here.
    int opt;
    while ((opt = getopt_long(argc, argv, "+vhg:c:", cliOptions, NULL)) != -1)
    {
        switch (opt)
        {
        case 'v':
            printf("vcat - v%s\n", VCAT_VERSION);
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        case 'c': {
            char *end = NULL;
            const long index = strtol(optarg, &end, 10);
            if (end == optarg || (end != NULL && *end != '\0') || index < 0 || index >= VCAT_COLOR_MODE_COUNT)
            {
                printf("Invalid color mode: '%s'\n", optarg);
                printMainHelp(plugins, pluginsLen);
                return ARG_PARSE_RESULT_ERROR;
            }
            if (userArgs != NULL)
            {
                userArgs->colorModeIndex = (int)index;
            }
            break;
        }
        case OPT_LAST_LOG:
            if (vcatLogPrintLastRun() != RC_OK)
            {
                printf("No log file from a previous run\n");
                return ARG_PARSE_RESULT_ERROR;
            }
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        case OPT_SHOW_GRADIENTS:
            for (int i = 0; i < gAsciiGradientsLen; i++)
            {
                printf("%d  %-16s %s\n", i, gAsciiGradients[i].name, gAsciiGradients[i].gradient);
            }
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        case 'g': {
            char *end = NULL;
            const long index = strtol(optarg, &end, 10);
            if (end == optarg || (end != NULL && *end != '\0') || index < 0 || index >= gAsciiGradientsLen)
            {
                printf("Invalid gradient: '%s'\n", optarg);
                printMainHelp(plugins, pluginsLen);
                return ARG_PARSE_RESULT_ERROR;
            }
            if (userArgs != NULL)
            {
                userArgs->gradientIndex = (int)index;
            }
            break;
        }
        case 'h':
            printMainHelp(plugins, pluginsLen);
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        default:
            printMainHelp(plugins, pluginsLen);
            return ARG_PARSE_RESULT_ERROR;
        }
    }

    // The first non-option token selects the video source: a bare scheme token
    // ("udp") or a full URI ("udp://127.0.0.1:1234"). Everything after it
    // belongs to the selected source.
    if (optind < argc)
    {
        for (int j = 0; j < pluginsLen; j++)
        {
            if (sourceTokenMatchesPlugin(argv[optind], plugins[j]))
            {
                *targetPlugin = plugins[j];
                break;
            }
        }
    }

    if (*targetPlugin == NULL)
    {
        if (optind < argc)
        {
            printf("Unknown video source: '%s'\n", argv[optind]);
        }
        else
        {
            printf("No video source specified\n");
        }
        printMainHelp(plugins, pluginsLen);
        return ARG_PARSE_RESULT_ERROR;
    }

    // Before the source parses its slice: its getopt_long advances the shared optind.
    crox_str_join(sourceInfo->args, sizeof(sourceInfo->args), &argv[optind], argc - optind, " ");
    sourceInfo->name = vcatPluginGetName(*targetPlugin);

    // The source owns its whole "<SOURCE> [ARGS...]" handling: it parses the
    // source URI (argv[0]), prints its own help on "-h"/"--help", and consumes
    // remaining options. The result decides what vcat does next.
    return vcatVideoSourceProcessInputArgs(*targetPlugin, argc - optind, argv + optind);
}
