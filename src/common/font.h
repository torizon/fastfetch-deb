#pragma once

#include "util/FFstrbuf.h"
#include "util/FFlist.h"

typedef struct FFfont
{
    FFstrbuf pretty;
    FFstrbuf name;
    FFstrbuf size;
    FFlist styles;
} FFfont;

void ffFontInit(FFfont* font);
void ffFontInitQt(FFfont* font, const char* data);
void ffFontInitPango(FFfont* font, const char* data);
void ffFontInitValues(FFfont* font, const char* name, const char* size);
void ffFontInitWithSpace(FFfont* font, const char* rawName);
void ffFontDestroy(FFfont* font);

static inline void ffFontInitCopy(FFfont* font, const char* name)
{
    ffFontInitValues(font, name, NULL);
}
