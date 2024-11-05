//
// Created by fluty on 24-9-28.
//

#ifndef DMLUTILS_H
#define DMLUTILS_H

#include "Modules/Inference/Models/GpuInfo.h"

#include <QList>

#ifdef _WIN32
#include <dxgi1_6.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;
#endif

class DmlUtils {
public:
    static QList<GpuInfo> getDirectXGPUs() {
#ifdef _WIN32
        QList<GpuInfo> gpuList;

        // Create DXGI factory
        ComPtr<IDXGIFactory6> dxgiFactory;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)))) {
            // "Failed to create DXGI factory."
            return {};
        }

        // Enumerate adapters
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT adapterIndex = 0;
             dxgiFactory->EnumAdapters1(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND;
             ++adapterIndex) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                // Skip software adapters
                continue;
            }

            gpuList.push_back({static_cast<int>(adapterIndex),
                               QString::fromWCharArray(desc.Description),
                               desc.DedicatedVideoMemory});
        }

        // Sort gpuList by DedicatedVideoMemory in descending order
        std::sort(gpuList.begin(), gpuList.end(),
                  [](const GpuInfo &a, const GpuInfo &b) { return a.memory > b.memory; });
        return gpuList;
#else
        return {};
#endif
    }
};



#endif // DMLUTILS_H
