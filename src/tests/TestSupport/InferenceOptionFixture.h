#pragma once

#include <QJsonObject>

namespace TestSupport {
    inline QJsonObject inferenceOptionConfig(const QString &cacheDirectory, const QString &provider,
                                             const QString &gpuId = QStringLiteral("saved-gpu")) {
        return {
            {QStringLiteral("cacheDirectory"),    cacheDirectory},
            {QStringLiteral("executionProvider"), provider      },
            {QStringLiteral("selectedGpuIndex"),  3             },
            {QStringLiteral("selectedGpuId"),     gpuId         }
        };
    }
}
