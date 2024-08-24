#pragma once

#include "fastfetch.h"

typedef struct FFLocalIpResult
{
    FFstrbuf name;
    FFstrbuf ipv4;
    FFstrbuf ipv6;
    FFstrbuf mac;
    int32_t mtu;
    bool defaultRoute;
} FFLocalIpResult;

const char* ffDetectLocalIps(const FFLocalIpOptions* options, FFlist* results);
