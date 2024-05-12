#include "registry.h"
#include "unicode.h"
#include "util/mallocHelper.h"

static const char* hKey2Str(HKEY hKey)
{
    #define HKEY_CASE(compareKey) if(hKey == compareKey) return #compareKey;
    HKEY_CASE(HKEY_CLASSES_ROOT)
    HKEY_CASE(HKEY_CURRENT_USER)
    HKEY_CASE(HKEY_LOCAL_MACHINE)
    HKEY_CASE(HKEY_USERS)
    HKEY_CASE(HKEY_PERFORMANCE_DATA)
    HKEY_CASE(HKEY_PERFORMANCE_TEXT)
    HKEY_CASE(HKEY_PERFORMANCE_NLSTEXT)
    HKEY_CASE(HKEY_CURRENT_CONFIG)
    HKEY_CASE(HKEY_DYN_DATA)
    HKEY_CASE(HKEY_CURRENT_USER_LOCAL_SETTINGS)
    #undef HKEY_CASE

    return "UNKNOWN";
}

bool ffRegOpenKeyForRead(HKEY hKey, const wchar_t* subKeyW, HKEY* result, FFstrbuf* error)
{
    if(RegOpenKeyExW(hKey, subKeyW, 0, KEY_READ, result) != ERROR_SUCCESS)
    {
        if(error)
        {
            FF_STRBUF_AUTO_DESTROY subKeyA = ffStrbufCreateWS(subKeyW);
            ffStrbufAppendF(error, "RegOpenKeyExW(%s\\%s) failed", hKey2Str(hKey), subKeyA.chars);
        }
        return false;
    }
    return true;
}

bool ffRegReadStrbuf(HKEY hKey, const wchar_t* valueNameW, FFstrbuf* result, FFstrbuf* error)
{
    DWORD bufSize; //with tailing '\0'
    if(RegGetValueW(hKey, NULL, valueNameW, RRF_RT_REG_SZ, NULL, NULL, &bufSize) != ERROR_SUCCESS)
    {
        if(error)
        {
            if(!valueNameW)
                valueNameW = L"(default)";
            FF_STRBUF_AUTO_DESTROY valueNameA = ffStrbufCreateWS(valueNameW);
            ffStrbufAppendF(error, "RegGetValueW(%s, NULL, RRF_RT_REG_SZ) failed", valueNameA.chars);
        }
        return false;
    }
    wchar_t* FF_AUTO_FREE resultW = (wchar_t*)malloc(bufSize);
    if(RegGetValueW(hKey, NULL, valueNameW, RRF_RT_REG_SZ, NULL, resultW, &bufSize) != ERROR_SUCCESS)
    {
        if(error)
        {
            if(!valueNameW)
                valueNameW = L"(default)";
            FF_STRBUF_AUTO_DESTROY valueNameA = ffStrbufCreateWS(valueNameW);
            ffStrbufAppendF(error, "RegGetValueW(%s, result, RRF_RT_REG_SZ) failed", valueNameA.chars);
        }
        return false;
    }
    ffStrbufSetWS(result, resultW);
    return true;
}

bool ffRegReadData(HKEY hKey, const wchar_t* valueNameW, uint8_t** result, uint32_t* length, FFstrbuf* error)
{
    assert(result && length);
    DWORD bufSize = 0;
    LONG err = RegGetValueW(hKey, NULL, valueNameW, RRF_RT_REG_BINARY, NULL, NULL, &bufSize);
    if(err != ERROR_SUCCESS || bufSize == 0)
    {
        if(error)
        {
            if(!valueNameW)
                valueNameW = L"(default)";
            FF_STRBUF_AUTO_DESTROY valueNameA = ffStrbufCreateWS(valueNameW);
            ffStrbufAppendF(error, "RegGetValueW(%s, NULL, RRF_RT_REG_BINARY, NULL, NULL, &bufSize) failed", valueNameA.chars);
        }
        return false;
    }

    uint8_t* buf = (uint8_t*) malloc(bufSize);
    err = RegGetValueW(hKey, NULL, valueNameW, RRF_RT_REG_BINARY, NULL, buf, &bufSize);
    if(err != ERROR_SUCCESS)
    {
        if(error)
        {
            if(!valueNameW)
                valueNameW = L"(default)";
            FF_STRBUF_AUTO_DESTROY valueNameA = ffStrbufCreateWS(valueNameW);
            ffStrbufAppendF(error, "RegGetValueW(%s, NULL, RRF_RT_REG_BINARY, NULL, length) failed", valueNameA.chars);
        }
        free(buf);
        return false;
    }
    *result = buf;
    *length = bufSize;
    return true;
}

bool ffRegReadUint(HKEY hKey, const wchar_t* valueNameW, uint32_t* result, FFstrbuf* error)
{
    DWORD bufSize = sizeof(*result);
    if(RegGetValueW(hKey, NULL, valueNameW, RRF_RT_DWORD, NULL, result, &bufSize) != ERROR_SUCCESS)
    {
        if(error)
        {
            if(!valueNameW)
                valueNameW = L"(default)";
            FF_STRBUF_AUTO_DESTROY valueNameA = ffStrbufCreateWS(valueNameW);
            ffStrbufAppendF(error, "RegGetValueW(%s, result, RRF_RT_DWORD) failed", valueNameA.chars);
        }
        return false;
    }
    return true;
}

bool ffRegReadUint64(HKEY hKey, const wchar_t* valueNameW, uint64_t* result, FFstrbuf* error)
{
    DWORD bufSize = sizeof(*result);
    if(RegGetValueW(hKey, NULL, valueNameW, RRF_RT_QWORD, NULL, result, &bufSize) != ERROR_SUCCESS)
    {
        if(error)
        {
            if(!valueNameW)
                valueNameW = L"(default)";
            FF_STRBUF_AUTO_DESTROY valueNameA = ffStrbufCreateWS(valueNameW);
            ffStrbufAppendF(error, "RegGetValueW(%s, result, RRF_RT_QWORD) failed", valueNameA.chars);
        }
        return false;
    }
    return true;
}

bool ffRegGetSubKey(HKEY hKey, uint32_t index, FFstrbuf* result, FFstrbuf* error)
{
    DWORD bufSize = 0;
    if(RegQueryInfoKeyW(hKey, NULL, NULL, NULL, NULL, &bufSize, NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
    {
        if (error)
            ffStrbufAppendS(error, "RegQueryInfoKeyW(hKey) failed");
        return false;
    }
    ++bufSize;
    wchar_t* FF_AUTO_FREE resultW = (wchar_t*) malloc(bufSize * sizeof(*resultW));
    if(RegEnumKeyExW(hKey, index, resultW, &bufSize, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
    {
        if (error)
            ffStrbufAppendF(error, "RegEnumKeyExW(hKey, %u) failed", (unsigned) index);
        return false;
    }
    ffStrbufSetWS(result, resultW);
    return true;
}

bool ffRegGetNSubKeys(HKEY hKey, uint32_t* result, FFstrbuf* error)
{
    DWORD buffer;
    if(RegQueryInfoKeyW(hKey, NULL, NULL, NULL, &buffer, NULL, NULL, NULL, NULL, NULL, NULL, NULL) != ERROR_SUCCESS)
    {
        if (error)
            ffStrbufAppendS(error, "RegQueryInfoKeyW(hKey) failed");
        return false;
    }

    *result = buffer;
    return true;
}
