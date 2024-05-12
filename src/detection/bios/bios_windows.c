#include "bios.h"
#include "util/smbiosHelper.h"

#include <ntstatus.h>
#include <winternl.h>

typedef struct _SYSTEM_BOOT_ENVIRONMENT_INFORMATION
{
    GUID BootIdentifier;
    FIRMWARE_TYPE FirmwareType;
    union
    {
        ULONGLONG BootFlags;
        struct
        {
            ULONGLONG DbgMenuOsSelection : 1; // REDSTONE4
            ULONGLONG DbgHiberBoot : 1;
            ULONGLONG DbgSoftBoot : 1;
            ULONGLONG DbgMeasuredLaunch : 1;
            ULONGLONG DbgMeasuredLaunchCapable : 1; // 19H1
            ULONGLONG DbgSystemHiveReplace : 1;
            ULONGLONG DbgMeasuredLaunchSmmProtections : 1;
            ULONGLONG DbgMeasuredLaunchSmmLevel : 7; // 20H1
        };
    };
} SYSTEM_BOOT_ENVIRONMENT_INFORMATION;


typedef struct FFSmbiosBios
{
    FFSmbiosHeader Header;

    uint8_t Vendor; // string
    uint8_t BiosVersion; // string
    uint16_t BiosStartingAddressSegment; // varies
    uint8_t BiosReleaseDate; // string
    uint8_t BiosRomSize; // string
    uint32_t BiosCharacteristics; // bit field

    // 2.4+
    uint8_t BiosCharacteristicsExtensionBytes[2]; // bit field
    uint8_t SystemBiosMajorRelease; // varies
    uint8_t SystemBiosMinorRelease; // varies
    uint8_t EmbeddedControllerFirmwareMajorRelease; // varies
    uint8_t EmbeddedControllerFirmwareMinorRelease; // varies

    // 3.1+
    uint16_t ExtendedBiosRomSize; // bit field
} FFSmbiosBios;

const char* ffDetectBios(FFBiosResult* bios)
{
    const FFSmbiosBios* data = (const FFSmbiosBios*) (*ffGetSmbiosHeaderTable())[FF_SMBIOS_TYPE_BIOS];
    if (!data)
        return "BIOS section is not found in SMBIOS data";

    const char* strings = (const char*) data + data->Header.Length;

    ffStrbufSetStatic(&bios->version, ffSmbiosLocateString(strings, data->BiosVersion));
    ffCleanUpSmbiosValue(&bios->version);
    ffStrbufSetStatic(&bios->vendor, ffSmbiosLocateString(strings, data->Vendor));
    ffCleanUpSmbiosValue(&bios->vendor);
    ffStrbufSetStatic(&bios->date, ffSmbiosLocateString(strings, data->BiosReleaseDate));
    ffCleanUpSmbiosValue(&bios->date);

    if (data->Header.Length > offsetof(FFSmbiosBios, SystemBiosMajorRelease))
        ffStrbufSetF(&bios->release, "%u.%u", data->SystemBiosMajorRelease, data->SystemBiosMinorRelease);

    // Same as GetFirmwareType, but support (?) Windows 7
    // https://ntdoc.m417z.com/system_information_class
    SYSTEM_BOOT_ENVIRONMENT_INFORMATION sbei;
    if (NT_SUCCESS(NtQuerySystemInformation(90 /*SystemBootEnvironmentInformation*/, &sbei, sizeof(sbei), NULL)))
    {
        switch (sbei.FirmwareType)
        {
            case FirmwareTypeBios: ffStrbufSetStatic(&bios->type, "BIOS"); break;
            case FirmwareTypeUefi: ffStrbufSetStatic(&bios->type, "UEFI"); break;
            default: break;
        }
    }

    return NULL;
}
