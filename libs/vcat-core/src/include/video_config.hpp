
#pragma once

#include "utils.hpp"
#include <cstdint>

enum VideoSourceType : uint8_t
{
    NONE = 0,
    UDP_MPEGTS = 1,
    TEST = 2
};

struct NetworkSource
{
    Ip ip{};
    int port;
};

struct VideoConfig
{
    VideoSourceType sourceType = VideoSourceType::NONE;
    union {
        NetworkSource network{};
    };
};
