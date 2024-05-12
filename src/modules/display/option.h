#pragma once

// This file will be included in "fastfetch.h", do NOT put unnecessary things here

#include "common/option.h"

typedef enum FFDisplayCompactType
{
    FF_DISPLAY_COMPACT_TYPE_NONE = 0,
    FF_DISPLAY_COMPACT_TYPE_ORIGINAL_BIT = 1 << 0,
    FF_DISPLAY_COMPACT_TYPE_SCALED_BIT = 1 << 1,
    FF_DISPLAY_COMPACT_TYPE_REFRESH_RATE_BIT = 1 << 2,
} FFDisplayCompactType;

typedef enum FFDisplayOrder
{
    FF_DISPLAY_ORDER_NONE,
    FF_DISPLAY_ORDER_ASC,
    FF_DISPLAY_ORDER_DESC,
} FFDisplayOrder;

typedef struct FFDisplayOptions
{
    FFModuleBaseInfo moduleInfo;
    FFModuleArgs moduleArgs;

    FFDisplayCompactType compactType;
    bool preciseRefreshRate;
    FFDisplayOrder order;
} FFDisplayOptions;
