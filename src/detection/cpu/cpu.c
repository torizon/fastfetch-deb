#include "cpu.h"

const char* ffDetectCPUImpl(const FFCPUOptions* options, FFCPUResult* cpu);

const char* ffDetectCPU(const FFCPUOptions* options, FFCPUResult* cpu)
{
    const char* error = ffDetectCPUImpl(options, cpu);
    if (error) return error;

    const char* removeStrings[] = {
        " CPU", " FPU", " APU", " Processor",
        " Dual-Core", " Quad-Core", " Six-Core", " Eight-Core", " Ten-Core",
        " 2-Core", " 4-Core", " 6-Core", " 8-Core", " 10-Core", " 12-Core", " 14-Core", " 16-Core",
        " with Radeon Graphics"
    };
    ffStrbufRemoveStrings(&cpu->name, sizeof(removeStrings) / sizeof(removeStrings[0]), removeStrings);
    ffStrbufSubstrBeforeFirstC(&cpu->name, '@'); //Cut the speed output in the name as we append our own
    ffStrbufTrimRight(&cpu->name, ' '); //If we removed the @ in previous step there was most likely a space before it
    return NULL;
}

const char* ffCPUAppleCodeToName(uint32_t code)
{
    // https://github.com/AsahiLinux/docs/wiki/Codenames
    switch (code)
    {
        case 8103: return "Apple M1";
        case 6000: return "Apple M1 Pro";
        case 6001: return "Apple M1 Max";
        case 6002: return "Apple M1 Ultra";
        case 8112: return "Apple M2";
        case 6020: return "Apple M2 Pro";
        case 6021: return "Apple M2 Max";
        case 6022: return "Apple M2 Ultra";
        case 8122: return "Apple M3";
        case 6030: return "Apple M3 Pro";
        case 6031:
        case 6034: return "Apple M3 Max";
        default: return "Apple Silicon";
    }
}
