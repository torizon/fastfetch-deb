#include "displayserver_linux.h"

#ifdef FF_HAVE_X11
#include "common/library.h"
#include "common/parsing.h"
#include "util/stringUtils.h"
#include <X11/Xlib.h>

typedef struct X11PropertyData
{
    FF_LIBRARY_SYMBOL(XInternAtom)
    FF_LIBRARY_SYMBOL(XGetWindowProperty)
    FF_LIBRARY_SYMBOL(XFree)
} X11PropertyData;

static bool x11InitPropertyData(void* libraryHandle, X11PropertyData* propertyData)
{
    FF_LIBRARY_LOAD_SYMBOL_PTR(libraryHandle, propertyData, XInternAtom, false)
    FF_LIBRARY_LOAD_SYMBOL_PTR(libraryHandle, propertyData, XGetWindowProperty, false)
    FF_LIBRARY_LOAD_SYMBOL_PTR(libraryHandle, propertyData, XFree, false)

    return true;
}

static unsigned char* x11GetProperty(X11PropertyData* data, Display* display, Window window, const char* request)
{
    Atom requestAtom = data->ffXInternAtom(display, request, False);
    if(requestAtom == None)
        return NULL;

    Atom actualType;
    unsigned long unused;
    unsigned char* result = NULL;

    if(data->ffXGetWindowProperty(display, window, requestAtom, 0, 64, False, AnyPropertyType, &actualType, (int*) &unused, &unused, &unused, &result) != Success)
        return NULL;

    return result;
}

static void x11DetectWMFromEWMH(X11PropertyData* data, Display* display, FFDisplayServerResult* result)
{
    if(result->wmProcessName.length > 0 || ffStrbufCompS(&result->wmProtocolName, FF_WM_PROTOCOL_WAYLAND) == 0)
        return;

    Window* wmWindow = (Window*) x11GetProperty(data, display, DefaultRootWindow(display), "_NET_SUPPORTING_WM_CHECK");
    if(wmWindow == NULL)
        return;

    char* wmName = (char*) x11GetProperty(data, display, *wmWindow, "_NET_WM_NAME");
    if(wmName == NULL)
        wmName = (char*) x11GetProperty(data, display, *wmWindow, "WM_NAME");

    if(ffStrSet(wmName))
        ffStrbufSetS(&result->wmProcessName, wmName);

    data->ffXFree(wmName);
    data->ffXFree(wmWindow);
}

void ffdsConnectXlib(FFDisplayServerResult* result)
{
    FF_LIBRARY_LOAD(x11, &instance.config.library.libX11, , "libX11" FF_LIBRARY_EXTENSION, 7, "libX11-xcb" FF_LIBRARY_EXTENSION, 2)
    FF_LIBRARY_LOAD_SYMBOL(x11, XOpenDisplay,)
    FF_LIBRARY_LOAD_SYMBOL(x11, XCloseDisplay,)

    X11PropertyData propertyData;
    bool propertyDataInitialized = x11InitPropertyData(x11, &propertyData);

    Display* display = ffXOpenDisplay(x11);
    if(display == NULL)
        return;

    if(propertyDataInitialized && ScreenCount(display) > 0)
        x11DetectWMFromEWMH(&propertyData, display, result);

    for(int i = 0; i < ScreenCount(display); i++)
    {
        Screen* screen = ScreenOfDisplay(display, i);
        ffdsAppendDisplay(result,
            (uint32_t) WidthOfScreen(screen),
            (uint32_t) HeightOfScreen(screen),
            0,
            (uint32_t) WidthOfScreen(screen),
            (uint32_t) HeightOfScreen(screen),
            0,
            NULL,
            FF_DISPLAY_TYPE_UNKNOWN,
            false,
            0,
            (uint32_t) WidthMMOfScreen(screen),
            (uint32_t) HeightMMOfScreen(screen)
        );
    }

    ffXCloseDisplay(display);

    //If wayland hasn't set this, connection failed for it. So we are running only a X Server, not XWayland.
    if(result->wmProtocolName.length == 0)
        ffStrbufSetS(&result->wmProtocolName, FF_WM_PROTOCOL_X11);
}

#else

void ffdsConnectXlib(FFDisplayServerResult* result)
{
    //Do nothing. WM / DE detection will use environment vars to detect as much as possible.
    FF_UNUSED(result);
}

#endif //FF_HAVE_X11

#ifdef FF_HAVE_XRANDR
#include "util/edidHelper.h"
#include <X11/extensions/Xrandr.h>

typedef struct XrandrData
{
    FF_LIBRARY_SYMBOL(XInternAtom)
    FF_LIBRARY_SYMBOL(XGetAtomName);
    FF_LIBRARY_SYMBOL(XFree);
    FF_LIBRARY_SYMBOL(XRRGetMonitors)
    FF_LIBRARY_SYMBOL(XRRGetScreenResourcesCurrent)
    FF_LIBRARY_SYMBOL(XRRGetOutputInfo)
    FF_LIBRARY_SYMBOL(XRRGetOutputProperty)
    FF_LIBRARY_SYMBOL(XRRGetCrtcInfo)
    FF_LIBRARY_SYMBOL(XRRFreeCrtcInfo)
    FF_LIBRARY_SYMBOL(XRRFreeOutputInfo)
    FF_LIBRARY_SYMBOL(XRRFreeScreenResources)
    FF_LIBRARY_SYMBOL(XRRFreeMonitors)

    //Init once
    Display* display;
    FFDisplayServerResult* result;

    //Init per screen
    XRRScreenResources* screenResources;
} XrandrData;

static double xrandrHandleMode(XrandrData* data, RRMode mode)
{
    for(int i = 0; i < data->screenResources->nmode; i++)
    {
        if(data->screenResources->modes[i].id == mode)
        {
            XRRModeInfo* modeInfo = &data->screenResources->modes[i];
            return (double) modeInfo->dotClock / (double) (modeInfo->hTotal * modeInfo->vTotal);
        }
    }
    return 0;
}

static bool xrandrHandleCrtc(XrandrData* data, RRCrtc crtc, FFstrbuf* name, bool primary, XRROutputInfo* output, FFDisplayType displayType)
{
    //We do the check here, because we want the best fallback display if this call failed
    if(data->screenResources == NULL)
        return false;

    XRRCrtcInfo* crtcInfo = data->ffXRRGetCrtcInfo(data->display, data->screenResources, crtc);
    if(crtcInfo == NULL)
        return false;

    uint32_t rotation;
    switch (crtcInfo->rotation)
    {
        case RR_Rotate_90:
            rotation = 90;
            break;
        case RR_Rotate_180:
            rotation = 180;
            break;
        case RR_Rotate_270:
            rotation = 270;
            break;
        default:
            rotation = 0;
            break;
    }

    bool res = ffdsAppendDisplay(
        data->result,
        (uint32_t) crtcInfo->width,
        (uint32_t) crtcInfo->height,
        xrandrHandleMode(data, crtcInfo->mode),
        (uint32_t) crtcInfo->width,
        (uint32_t) crtcInfo->height,
        rotation,
        name,
        displayType,
        primary,
        0,
        (uint32_t) output->mm_width,
        (uint32_t) output->mm_height
    );

    data->ffXRRFreeCrtcInfo(crtcInfo);
    return res;
}

static bool xrandrHandleOutput(XrandrData* data, RROutput output, FFstrbuf* name, bool primary, FFDisplayType displayType)
{
    XRROutputInfo* outputInfo = data->ffXRRGetOutputInfo(data->display, data->screenResources, output);
    if(outputInfo == NULL)
        return false;

    Atom atomEdid = data->ffXInternAtom(data->display, "EDID", true);
    if (atomEdid != None)
    {
        int actual_format = 0;
        unsigned long nitems = 0, bytes_after = 0;
        Atom actual_type = None;
        uint8_t* edidData = NULL;
        if (data->ffXRRGetOutputProperty(data->display, output, atomEdid, 0, 100, false, false, AnyPropertyType, &actual_type, &actual_format, &nitems, &bytes_after, &edidData) == Success)
        {
            if (nitems >= 128)
            {
                ffStrbufClear(name);
                ffEdidGetName(edidData, name);
            }
        }
        if (edidData)
            data->ffXFree(edidData);
    }
    bool res = xrandrHandleCrtc(data, outputInfo->crtc, name, primary, outputInfo, displayType);

    data->ffXRRFreeOutputInfo(outputInfo);

    return res;
}

static bool xrandrHandleMonitor(XrandrData* data, XRRMonitorInfo* monitorInfo)
{
    bool foundOutput = false;
    char* xname = data->ffXGetAtomName(data->display, monitorInfo->name);
    FF_STRBUF_AUTO_DESTROY name = ffStrbufCreateS(xname);
    data->ffXFree(xname);
    FFDisplayType displayType = ffdsGetDisplayType(name.chars);
    for(int i = 0; i < monitorInfo->noutput; i++)
    {
        if(xrandrHandleOutput(data, monitorInfo->outputs[i], &name, monitorInfo->primary, displayType))
            foundOutput = true;
    }

    return foundOutput ? true : !!ffdsAppendDisplay(
        data->result,
        (uint32_t) monitorInfo->width,
        (uint32_t) monitorInfo->height,
        0,
        (uint32_t) monitorInfo->width,
        (uint32_t) monitorInfo->height,
        0,
        &name,
        displayType,
        !!monitorInfo->primary,
        0,
        (uint32_t) monitorInfo->mwidth,
        (uint32_t) monitorInfo->mheight
    );
}

static bool xrandrHandleMonitors(XrandrData* data, Screen* screen)
{
    int numberOfMonitors;
    XRRMonitorInfo* monitorInfos = data->ffXRRGetMonitors(data->display, RootWindowOfScreen(screen), True, &numberOfMonitors);
    if(monitorInfos == NULL)
        return false;

    bool foundAMonitor;

    for(int i = 0; i < numberOfMonitors; i++)
    {
        if(xrandrHandleMonitor(data, &monitorInfos[i]))
            foundAMonitor = true;
    }

    data->ffXRRFreeMonitors(monitorInfos);

    return foundAMonitor;
}

static void xrandrHandleScreen(XrandrData* data, Screen* screen)
{
    //Init screen resources
    data->screenResources = data->ffXRRGetScreenResourcesCurrent(data->display, RootWindowOfScreen(screen));

    bool ret = xrandrHandleMonitors(data, screen);

    data->ffXRRFreeScreenResources(data->screenResources);

    if(ret)
        return;

    //Fallback to screen
    ffdsAppendDisplay(
        data->result,
        (uint32_t) WidthOfScreen(screen),
        (uint32_t) HeightOfScreen(screen),
        0,
        (uint32_t) WidthOfScreen(screen),
        (uint32_t) HeightOfScreen(screen),
        0,
        NULL,
        FF_DISPLAY_TYPE_UNKNOWN,
        false,
        0,
        (uint32_t) WidthMMOfScreen(screen),
        (uint32_t) HeightMMOfScreen(screen)
    );
}

void ffdsConnectXrandr(FFDisplayServerResult* result)
{
    FF_LIBRARY_LOAD(xrandr, &instance.config.library.libXrandr, , "libXrandr" FF_LIBRARY_EXTENSION, 3)

    FF_LIBRARY_LOAD_SYMBOL(xrandr, XOpenDisplay,)
    FF_LIBRARY_LOAD_SYMBOL(xrandr, XCloseDisplay,)

    XrandrData data;

    FF_LIBRARY_LOAD_SYMBOL_VAR(xrandr, data, XInternAtom,);
    FF_LIBRARY_LOAD_SYMBOL_VAR(xrandr, data, XGetAtomName,);
    FF_LIBRARY_LOAD_SYMBOL_VAR(xrandr, data, XFree,);
    FF_LIBRARY_LOAD_SYMBOL_VAR(xrandr, data, XRRGetMonitors,);
    FF_LIBRARY_LOAD_SYMBOL_VAR(xrandr, data, XRRGetScreenResourcesCurrent,);
    FF_LIBRARY_LOAD_SYMBOL_VAR(xrandr, data, XRRGetOutputInfo,);
    FF_LIBRARY_LOAD_SYMBOL_VAR(xrandr, data, XRRGetOutputProperty,);
    FF_LIBRARY_LOAD_SYMBOL_VAR(xrandr, data, XRRGetCrtcInfo,);
    FF_LIBRARY_LOAD_SYMBOL_VAR(xrandr, data, XRRFreeCrtcInfo,);
    FF_LIBRARY_LOAD_SYMBOL_VAR(xrandr, data, XRRFreeOutputInfo,);
    FF_LIBRARY_LOAD_SYMBOL_VAR(xrandr, data, XRRFreeScreenResources,);
    FF_LIBRARY_LOAD_SYMBOL_VAR(xrandr, data, XRRFreeMonitors,);

    X11PropertyData propertyData;
    bool propertyDataInitialized = x11InitPropertyData(xrandr, &propertyData);

    data.display = ffXOpenDisplay(NULL);
    if(data.display == NULL)
        return;

    if(propertyDataInitialized && ScreenCount(data.display) > 0)
        x11DetectWMFromEWMH(&propertyData, data.display, result);

    data.result = result;

    for(int i = 0; i < ScreenCount(data.display); i++)
        xrandrHandleScreen(&data, ScreenOfDisplay(data.display, i));

    ffXCloseDisplay(data.display);

    //If wayland hasn't set this, connection failed for it. So we are running only a X Server, not XWayland.
    if(result->wmProtocolName.length == 0)
        ffStrbufSetS(&result->wmProtocolName, FF_WM_PROTOCOL_X11);
}

#else

void ffdsConnectXrandr(FFDisplayServerResult* result)
{
    //Do nothing here. There are more x11 implementations to come.
    FF_UNUSED(result);
}

#endif // FF_HAVE_XRANDR
