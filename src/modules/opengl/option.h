#pragma once

// This file will be included in "fastfetch.h", do NOT put unnecessary things here

#include "common/option.h"

#if defined(__linux__) || defined(__FreeBSD__)
typedef enum FFOpenGLLibrary
{
    FF_OPENGL_LIBRARY_AUTO,
    FF_OPENGL_LIBRARY_EGL,
    FF_OPENGL_LIBRARY_GLX,
    FF_OPENGL_LIBRARY_OSMESA
} FFOpenGLLibrary;
#endif

typedef struct FFOpenGLOptions
{
    FFModuleBaseInfo moduleInfo;
    FFModuleArgs moduleArgs;

    #if defined(__linux__) || defined(__FreeBSD__)
    FFOpenGLLibrary library;
    #endif
} FFOpenGLOptions;
