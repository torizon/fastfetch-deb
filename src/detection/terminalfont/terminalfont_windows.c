#include "common/properties.h"
#include "common/io/io.h"
#include "detection/terminalshell/terminalshell.h"
#include "terminalfont.h"
#include "util/windows/unicode.h"

#include <windows.h>

static void detectMintty(FFTerminalFontResult* terminalFont)
{
    FF_STRBUF_AUTO_DESTROY fontName = ffStrbufCreate();
    FF_STRBUF_AUTO_DESTROY fontSize = ffStrbufCreate();

    if(!ffParsePropFileConfigValues("mintty/config", 2, (FFpropquery[]) {
        {"Font=", &fontName},
        {"FontHeight=", &fontSize}
    }))
        ffParsePropFileConfigValues(".minttyrc", 2, (FFpropquery[]) {
            {"Font=", &fontName},
            {"FontHeight=", &fontSize}
        });
    if(fontName.length == 0)
        ffStrbufAppendS(&fontName, "Lucida Console");
    if(fontSize.length == 0)
        ffStrbufAppendC(&fontSize, '9');

    ffFontInitValues(&terminalFont->font, fontName.chars, fontSize.chars);
}

static void detectConhost(FFTerminalFontResult* terminalFont)
{
    CONSOLE_FONT_INFOEX cfi = { .cbSize = sizeof(cfi) };
    if(!GetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &cfi))
    {
        ffStrbufAppendS(&terminalFont->error, "GetCurrentConsoleFontEx() failed");
        return;
    }

    FF_STRBUF_AUTO_DESTROY fontName = ffStrbufCreateWS(cfi.FaceName);

    char fontSize[16];
    _ultoa((unsigned long)(cfi.dwFontSize.Y), fontSize, 10);

    ffFontInitValues(&terminalFont->font, fontName.chars, fontSize);
}

static void detectConEmu(FFTerminalFontResult* terminalFont)
{
    //https://conemu.github.io/en/ConEmuXml.html#search-sequence
    FF_STRBUF_AUTO_DESTROY path = ffStrbufCreate();
    FF_STRBUF_AUTO_DESTROY fontName = ffStrbufCreate();
    FF_STRBUF_AUTO_DESTROY fontSize = ffStrbufCreate();

    const char* paths[] = { "ConEmuDir", "ConEmuBaseDir", "APPDATA" };
    for (uint32_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i)
    {
        ffStrbufSetS(&path, getenv(paths[i]));
        if(path.length > 0)
        {
            ffStrbufAppendS(&path, "/ConEmu.xml");
            if(ffParsePropFileValues(path.chars, 2, (FFpropquery[]){
                {"<value name=\"FontName\" type=\"string\" data=\"", &fontName},
                {"<value name=\"FontSize\" type=\"ulong\" data=\"", &fontSize}
            }))
                break;
        }
    }

    if(fontName.length == 0 && fontSize.length == 0)
    {
        ffStrbufAppendS(&terminalFont->error, "Failed to parse ConEmu.xml");
        return;
    }

    if(fontName.length > 0)
        ffStrbufSubstrBeforeLastC(&fontName, '"');
    else
        ffStrbufAppendS(&fontName, "Consola");

    if(fontSize.length > 0)
        ffStrbufSubstrBeforeLastC(&fontSize, '"');
    else
        ffStrbufAppendS(&fontSize, "14");

    ffFontInitValues(&terminalFont->font, fontName.chars, fontSize.chars);
}

void ffDetectTerminalFontPlatform(const FFTerminalResult* terminal, FFTerminalFontResult* terminalFont)
{
    if(ffStrbufIgnCaseEqualS(&terminal->processName, "mintty"))
        detectMintty(terminalFont);
    else if(ffStrbufIgnCaseEqualS(&terminal->processName, "conhost.exe"))
        detectConhost(terminalFont);
    else if(ffStrbufStartsWithIgnCaseS(&terminal->processName, "ConEmu"))
        detectConEmu(terminalFont);
}
