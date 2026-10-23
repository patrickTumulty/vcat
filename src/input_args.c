
#include "input_args.h"
#include "logging.h"
#include "vcat_plugin_api.h"
#include "vcat_plugin_framework.h"
#include <getopt.h>
#include <stdio.h>

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
           "  -v, --version   Print version information\n"
           "      --last-log  Print the previous run's log to stdout\n"
           "  -h, --help      Show this help\n"
           "\n"
           "Sources:\n",
           VCAT_VERSION);

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
        printf("  %-*s   %s\n", uriWidth, vcatVideoSourceGetUriHelp(p),
               vcatVideoSourceGetDescription(p));
    }

    printf("\n"
           "Run 'vcat <source> --help' for source-specific options.\n");
}

InputArgsResult vcatProcessInputArgs(int argc,                  //
                                     char *argv[],              //
                                     VCatPluginHandle *plugins, //
                                     const int pluginsLen,      //
                                     VCatPluginHandle *targetPlugin)
{
    // Long-only options get values past the character range: they can never
    // collide with a short option added later.
    enum
    {
        OPT_LAST_LOG = 256
    };

    static const struct option cliOptions[] = {
        {"version",   no_argument, NULL, 'v'         },
        {"help",      no_argument, NULL, 'h'         },
        {"last-log",  no_argument, NULL, OPT_LAST_LOG},
        {NULL,        0,           NULL, 0           }  //
    };

    // The "+" disables GNU argv permutation: option parsing stops at the first
    // non-option, so "<source> [ARGS...]" options belong to the source, not here.
    int opt;
    while ((opt = getopt_long(argc, argv, "+vh", cliOptions, NULL)) != -1)
    {
        switch (opt)
        {
        case 'v':
            printf("vcat - v%s\n", VCAT_VERSION);
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
        case OPT_LAST_LOG:
            if (vcatLogPrintLastRun() != RC_OK)
            {
                printf("No log file from a previous run\n");
                return ARG_PARSE_RESULT_ERROR;
            }
            return ARG_PARSE_RESULT_SUCCESS_EXIT_EARLY;
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

    // The source owns its whole "<SOURCE> [ARGS...]" handling: it parses the
    // source URI (argv[0]), prints its own help on "-h"/"--help", and consumes
    // remaining options. The result decides what vcat does next.
    return vcatVideoSourceProcessInputArgs(*targetPlugin, argc - optind, argv + optind);
}
