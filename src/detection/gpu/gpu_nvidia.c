#include "gpu_driver_specific.h"

#include "3rdparty/nvml/nvml.h"
#include "common/library.h"

struct FFNvmlData {
    FF_LIBRARY_SYMBOL(nvmlDeviceGetCount_v2)
    FF_LIBRARY_SYMBOL(nvmlDeviceGetHandleByIndex_v2)
    FF_LIBRARY_SYMBOL(nvmlDeviceGetHandleByPciBusId_v2)
    FF_LIBRARY_SYMBOL(nvmlDeviceGetPciInfo_v3)
    FF_LIBRARY_SYMBOL(nvmlDeviceGetTemperature)
    FF_LIBRARY_SYMBOL(nvmlDeviceGetMemoryInfo_v2)
    FF_LIBRARY_SYMBOL(nvmlDeviceGetNumGpuCores)
    FF_LIBRARY_SYMBOL(nvmlDeviceGetMaxClockInfo)
    FF_LIBRARY_SYMBOL(nvmlDeviceGetBrand)

    bool inited;
} nvmlData;

const char* ffDetectNvidiaGpuInfo(const FFGpuDriverCondition* cond, FFGpuDriverResult result, const char* soName)
{
    if (!nvmlData.inited)
    {
        nvmlData.inited = true;
        FF_LIBRARY_LOAD(libnvml, NULL, "dlopen nvml failed", soName , 1);
        FF_LIBRARY_LOAD_SYMBOL_MESSAGE(libnvml, nvmlInit_v2)
        FF_LIBRARY_LOAD_SYMBOL_MESSAGE(libnvml, nvmlShutdown)
        FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libnvml, nvmlData, nvmlDeviceGetCount_v2)
        FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libnvml, nvmlData, nvmlDeviceGetHandleByIndex_v2)
        FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libnvml, nvmlData, nvmlDeviceGetHandleByPciBusId_v2)
        FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libnvml, nvmlData, nvmlDeviceGetPciInfo_v3)
        FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libnvml, nvmlData, nvmlDeviceGetTemperature)
        FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libnvml, nvmlData, nvmlDeviceGetMemoryInfo_v2)
        FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libnvml, nvmlData, nvmlDeviceGetNumGpuCores)
        FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libnvml, nvmlData, nvmlDeviceGetMaxClockInfo)
        FF_LIBRARY_LOAD_SYMBOL_VAR_MESSAGE(libnvml, nvmlData, nvmlDeviceGetBrand)

        if (ffnvmlInit_v2() != NVML_SUCCESS)
        {
            nvmlData.ffnvmlDeviceGetNumGpuCores = NULL;
            return "nvmlInit_v2() failed";
        }
        atexit((void*) ffnvmlShutdown);
        libnvml = NULL; // don't close nvml
    }

    if (nvmlData.ffnvmlDeviceGetNumGpuCores == NULL)
        return "loading nvml library failed";

    nvmlDevice_t device = NULL;
    if (cond->type & FF_GPU_DRIVER_CONDITION_TYPE_BUS_ID)
    {
        char pciBusIdStr[32];
        snprintf(pciBusIdStr, sizeof(pciBusIdStr) - 1, "%04x:%02x:%02x.%d", cond->pciBusId.domain, cond->pciBusId.bus, cond->pciBusId.device, cond->pciBusId.func);

        nvmlReturn_t ret = nvmlData.ffnvmlDeviceGetHandleByPciBusId_v2(pciBusIdStr, &device);
        if (ret != NVML_SUCCESS)
            return "nvmlDeviceGetHandleByPciBusId_v2() failed";
    }
    else if (cond->type & FF_GPU_DRIVER_CONDITION_TYPE_DEVICE_ID)
    {
        uint32_t count;
        if (nvmlData.ffnvmlDeviceGetCount_v2(&count) != NVML_SUCCESS)
            return "nvmlDeviceGetCount_v2() failed";

        for (uint32_t i = 0; i < count; i++, device = NULL)
        {
            if (nvmlData.ffnvmlDeviceGetHandleByIndex_v2(i, &device) != NVML_SUCCESS)
                continue;

            nvmlPciInfo_t pciInfo;
            if (nvmlData.ffnvmlDeviceGetPciInfo_v3(device, &pciInfo) != NVML_SUCCESS)
                continue;

            if (pciInfo.pciDeviceId != ((cond->pciDeviceId.deviceId << 16u) | cond->pciDeviceId.vendorId) ||
                pciInfo.pciSubSystemId != cond->pciDeviceId.subSystemId)
                continue;

            break;
        }
        if (!device) return "Device not found";
    }

    nvmlBrandType_t brand;
    if (nvmlData.ffnvmlDeviceGetBrand(device, &brand) == NVML_SUCCESS)
    {
        switch (brand)
        {
            case NVML_BRAND_NVIDIA_RTX:
            case NVML_BRAND_QUADRO_RTX:
            case NVML_BRAND_GEFORCE:
            case NVML_BRAND_TITAN:
            case NVML_BRAND_TESLA:
            case NVML_BRAND_QUADRO:
                *result.type = FF_GPU_TYPE_DISCRETE;
                break;
            default:
                break;
        }
    }

    if (result.temp)
    {
        uint32_t value;
        if (nvmlData.ffnvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &value) == NVML_SUCCESS)
            *result.temp = value;
    }

    if (result.memory)
    {
        nvmlMemory_v2_t memory = { .version = nvmlMemory_v2 };
        if (nvmlData.ffnvmlDeviceGetMemoryInfo_v2(device, &memory) == NVML_SUCCESS)
        {
            result.memory->total = memory.used + memory.free;
            result.memory->used = memory.used;
        }
    }

    if (result.coreCount)
        nvmlData.ffnvmlDeviceGetNumGpuCores(device, result.coreCount);

    if (result.frequency)
    {
        uint32_t clockMHz;
        if (nvmlData.ffnvmlDeviceGetMaxClockInfo(device, NVML_CLOCK_GRAPHICS, &clockMHz) == NVML_SUCCESS)
            *result.frequency = clockMHz / 1000.;
    }

    return NULL;
}
