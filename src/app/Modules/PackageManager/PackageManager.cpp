//
// Created by FlutyDeer on 2025/7/27.
//

#include "PackageManager.h"

#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/InferEngine.h"
#include "Utils/StringUtils.h"
#include "stdcorelib/system.h"

#include <synthrt/Core/SynthUnit.h>
#include <synthrt/Core/PackageRef.h>
#include <synthrt/Support/Logging.h>

#include <dsinfer/Inference/InferenceDriver.h>

#include <QDebug>
#include <QLocale>

namespace fs = std::filesystem;

PackageManager::PackageManager() {
    connect(appStatus, &AppStatus::moduleStatusChanged, this,
            &PackageManager::onModuleStatusChanged);
}

Expected<GetInstalledPackagesResult, GetInstalledPackagesError>
    PackageManager::getInstalledPackages() {
    if (!inferEngine->initialized()) {
        return GetInstalledPackagesError{
            InferEngineNotInitialized,
            tr("Inference engine is not initialized"),
        };
    }

    QElapsedTimer timer;
    timer.start();
    GetInstalledPackagesResult result;
    srt::SynthUnit &su = inferEngine->synthUnit();

    auto processPackage = [&](const std::filesystem::path &packagePath) {
        if (auto exp = su.open(packagePath, true); !exp) {
            result.failedPackages.append({
                StringUtils::path_to_qstr(packagePath),
                srtErrorToString(exp.error())
            });
        } else {
            result.successfulPackages.append(exp.take());
        }
    };

    auto processAllPackages = [&](const std::filesystem::path &dir) {
        for (const auto &entry : fs::directory_iterator(dir)) {
            if (entry.is_directory()) {
                processPackage(entry.path());
            }
        }
    };

    for (const auto &path : su.packagePaths()) {
        if (!fs::exists(path) || !fs::is_directory(path)) {
            result.failedPackages.append({
                StringUtils::path_to_qstr(path),
                tr("Path is not a valid directory")
            });
            continue;
        }
        processAllPackages(path);
    }

    qDebug() << "Package scan completed in" << timer.elapsed() << "ms";
    return result;
}

void PackageManager::onModuleStatusChanged(AppStatus::ModuleType module,
                                           AppStatus::ModuleStatus status) {
    if (module != AppStatus::ModuleType::Inference || status != AppStatus::ModuleStatus::Ready)
        return;
}

QString PackageManager::srtErrorToString(const srt::Error &error) {
    const QString message = QString::fromStdString(error.message());
    switch (error.type()) {
        case srt::Error::NoError:
            return tr("No error: ") + message;
        case srt::Error::InvalidFormat:
            return tr("Invalid format: ") + message;
        case srt::Error::FileNotFound:
            return tr("File not found: ") + message;
        case srt::Error::FileNotOpen:
            return tr("File not open: ") + message;
        case srt::Error::FileDuplicated:
            return tr("File duplicated: ") + message;
        case srt::Error::RecursiveDependency:
            return tr("Recursive dependency: ") + message;
        case srt::Error::FeatureNotSupported:
            return tr("Feature not supported: ") + message;
        case srt::Error::InvalidArgument:
            return tr("Invalid argument: ") + message;
        case srt::Error::NotImplemented:
            return tr("Not implemented: ") + message;
        case srt::Error::SessionError:
            return tr("Session error: ") + message;
        default:
            return tr("Unknown error: ") + message;
    }
}