#ifdef ONNXRUNTIME_ENABLE_CUDA
#include "CudaGpuUtils.h"

#include <QDebug>
#include <QProcess>

static QString g_nvidiaSmiPath = QStringLiteral("nvidia-smi");

static inline GpuInfo parseGpuFromLine(const QStringView lineView, QString *outPciDeviceVendorId = nullptr) {
    GpuInfo gpuInfo;
    bool insideQuotes = false;
    QString currentField;
    int fieldIndex = 0;

    auto processField = [&](int fieldIndex_, const QStringView field) {
        switch (fieldIndex_) {
            case 0:
                gpuInfo.index = field.toInt();
            break;
            case 1:
                gpuInfo.description = field.toString();
            break;
            case 2:
                gpuInfo.memory = field.toULongLong() * 1024 * 1024;
            break;
            case 3:
                gpuInfo.deviceId = field.toString();
            break;
            case 4:
                if (outPciDeviceVendorId) {
                    *outPciDeviceVendorId = field.toString();
                }
            break;
            default:
                break;
        }
    };

    for (qsizetype i = 0; i < lineView.size(); ++i) {
        QChar ch = lineView.at(i);

        if (ch == '"') {
            insideQuotes = !insideQuotes; // Toggle inside-quotes state
        } else if (!insideQuotes && lineView.mid(i, 2) == ", ") {
            // Delimiter found, end of field
            processField(fieldIndex, currentField.trimmed());
            currentField.clear();
            fieldIndex++;
            i++; // Skip over the delimiter
        } else {
            currentField.append(ch);
        }

        // At the end of the line, finalize the current field
        if (i == lineView.size() - 1 && !currentField.isEmpty()) {
            processField(fieldIndex, currentField.trimmed());
        }
    }

    return gpuInfo;
}

static inline QList<GpuInfo> parseGpuListFromOutput(const QStringView output, int maxCount = 0) {
    QList<GpuInfo> gpuList;
    if (output.isEmpty()) {
        return gpuList;
    }
    const auto lines = output.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        return gpuList;
    }
    gpuList.reserve((maxCount > 0) ? qMin(maxCount, lines.size()) : lines.size());
    for (const auto line : lines) {
        if (line.isEmpty()) {
            continue;
        }
        gpuList.append(parseGpuFromLine(line));
        if (maxCount > 0 && gpuList.size() >= maxCount) {
            break;
        }
    }
    return gpuList;
}

static inline QString getCommandOutput() {
    QProcess process;
    process.start(g_nvidiaSmiPath, {"--query-gpu=index,name,memory.total,uuid,pci.device_id",
                                    "--format=csv,noheader,nounits"});
    if (!process.waitForFinished()) {
        qWarning() << "Failed to run nvidia-smi! Ensure it is in the PATH or setNvidiaSmiPath() is used.";
        return {};
    }

    return process.readAllStandardOutput();
}

GpuInfo CudaGpuUtils::getGpuByIndex(int index) {
    const QStringView output = getCommandOutput();
    if (output.isEmpty()) {
        return {};
    }
    const auto lines = output.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        return {};
    }
    for (const auto line : lines) {
        if (line.isEmpty()) {
            continue;
        }
        if (auto gpuInfo = parseGpuFromLine(line); gpuInfo.index == index) {
            return gpuInfo;
        }
    }
    return {};
}

QList<GpuInfo> CudaGpuUtils::getGpuList() {
    return parseGpuListFromOutput(getCommandOutput());
}

GpuInfo CudaGpuUtils::getGpuByPciDeviceVendorId(unsigned int pciDeviceId, unsigned int pciVendorId, int indexHint) {
    const QStringView output = getCommandOutput();
    if (output.isEmpty()) {
        return {};
    }
    const auto lines = output.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        return {};
    }
    GpuInfo info;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const auto line = std::as_const(lines)[i];
        if (line.isEmpty()) {
            continue;
        }
        QString deviceVendorId;
        unsigned int currentDeviceId = 0;
        unsigned int currentVendorId = 0;
        auto currentGpuInfo = parseGpuFromLine(line, &deviceVendorId);
        if (!GpuInfo::parseIdString(deviceVendorId, currentDeviceId, currentVendorId)) {
            continue;
        }
        if (pciDeviceId == currentDeviceId && pciVendorId == currentVendorId) {
            info = std::move(currentGpuInfo);
            if (info.index >= indexHint) {
                break;
            }
        }
    }
    return info;
}

GpuInfo CudaGpuUtils::getGpuByPciDeviceVendorIdString(const QString &idString, int indexHint) {
    unsigned int pciDeviceId = 0;
    unsigned int pciVendorId = 0;
    if (!GpuInfo::parseIdString(idString, pciDeviceId, pciVendorId)) {
        return {};
    }
    return getGpuByPciDeviceVendorId(pciDeviceId, pciVendorId, indexHint);
}

GpuInfo CudaGpuUtils::getGpuByUuid(const QString &uuid) {
    const QStringView output = getCommandOutput();
    if (output.isEmpty()) {
        return {};
    }
    const auto lines = output.split('\n', Qt::SkipEmptyParts);
    if (lines.isEmpty()) {
        return {};
    }
    for (const auto line : lines) {
        if (line.isEmpty()) {
            continue;
        }
        if (auto gpuInfo = parseGpuFromLine(line); gpuInfo.deviceId == uuid) {
            return gpuInfo;
        }
    }
    return {};
}

GpuInfo CudaGpuUtils::getRecommendedGpu() {
    // Find the GPU that has largest vRAM
    const auto gpuList = getGpuList();
    if (gpuList.isEmpty()) {
        return {};
    }
    unsigned long long memory = 0;
    qsizetype index = 0;
    for (qsizetype i = 0; i < gpuList.size(); ++i) {
        const auto &gpuInfo = std::as_const(gpuList)[i];
        if (gpuInfo.memory > memory) {
            index = i;
        }
    }
    return std::as_const(gpuList)[index];
}

void CudaGpuUtils::setNvidiaSmiPath(const QString &path) {
    g_nvidiaSmiPath = path;
}
#endif // defined(ONNXRUNTIME_ENABLE_CUDA)