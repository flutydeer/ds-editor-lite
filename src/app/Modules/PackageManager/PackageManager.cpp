//
// Created by FlutyDeer on 2025/7/27.
//

#include "PackageManager.h"

#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/InferEngine.h"
#include "stdcorelib/system.h"

#include <synthrt/Core/SynthUnit.h>
#include <synthrt/Core/PackageRef.h>
#include <synthrt/Support/Logging.h>
#include <synthrt/SVS/SingerContrib.h>

#include <dsinfer/Inference/InferenceDriver.h>

#include <QDebug>
#include <QLocale>

namespace fs = std::filesystem;

using srt::NO;

PackageManager::PackageManager() {
    connect(appStatus, &AppStatus::moduleStatusChanged, this,
            &PackageManager::onModuleStatusChanged);
}

Expected<QList<Package>, PackageManager::GetInstalledPackagesListError>
    PackageManager::getInstalledPackages() const {
    QList<Package> result = {
        {
            "packageA",
            "1.1.4",
            "Test Vendor A",
            "Copyright Test Vendor A",
            "Test DiffSinger package A.",
            "https://www.example.com",
            {
                {
                    "duration",
                    "ai.svs.DurationInference",
                    "inferences/duration/config.json"
                },
                {
                    "pitch",
                    "ai.svs.PitchInference",
                    "inferences/pitch/config.json"
                },
                {
                    "variance",
                    "ai.svs.VarianceInference",
                    "inferences/variance/config.json"
                },
            },
            {
                {
                    "singerA",
                    "diffsinger",
                    "characters/singerA/config.json"
                },
                {
                    "singerB",
                    "diffsinger",
                    "characters/singerB/config.json"
                }
            },
            {
                {
                    "dep1",
                    "1.0",
                    true
                },
                {
                    "dep2",
                    "2.0",
                    false
                }
            }
        },
        {
            "packageB",
            "5.1.4",
            "Test Vendor B",
            "Copyright Test Vendor B",
            "Test DiffSinger package B.",
            "https://www.example.com",
            {
                {
                    "duration",
                    "ai.svs.DurationInference",
                    "inferences/duration/config.json"
                },
                {
                    "pitch",
                    "ai.svs.PitchInference",
                    "inferences/pitch/config.json"
                },
                {
                    "variance",
                    "ai.svs.VarianceInference",
                    "inferences/variance/config.json"
                },
            },
            {
                {
                    "singerX",
                    "diffsinger",
                    "characters/singerA/config.json"
                },
                {
                    "singerY",
                    "diffsinger",
                    "characters/singerB/config.json"
                }
            },
            {
                {
                    "dep1",
                    "1.0",
                    true
                },
                {
                    "dep2",
                    "2.0",
                    false
                }
            }
        }
    };
    return result;
}

void PackageManager::onModuleStatusChanged(AppStatus::ModuleType module,
                                           AppStatus::ModuleStatus status) {
    if (module != AppStatus::ModuleType::Inference || status != AppStatus::ModuleStatus::Ready)
        return;

    auto homeDir = [] -> fs::path {
        return
#ifdef WIN32
            _wgetenv(L"USERPROFILE")
#else
            getenv("HOME")
#endif
            ;
    };

    const std::filesystem::path paths = {homeDir() / ".diffsinger/packages"};
    auto su = inferEngine->synthUnit();

    auto locale = QLocale::system().name().toStdString();

    // Add package directory to search path
    su->addPackagePath(paths);

    auto printPackageOne = [&](std::filesystem::path packagePath) {
        srt::ScopedPackageRef pkg;
        if (auto exp = su->open(packagePath, true); !exp) {
            throw std::runtime_error(stdc::formatN(R"(failed to open package "%1": %2)",
                                                   packagePath, exp.error().message()));
        } else {
            pkg = exp.take();
        }
        // if (!pkg.isLoaded()) {
        //     qCritical() << pkg.error().message();
        //     throw std::runtime_error(stdc::formatN(R"(failed to load package "%1": %2)", packagePath, pkg.error().message()));
        // }

        qInfo() << "------------ Package Info Begin ------------";
        qInfo() << "Package ID: " << pkg.id();
        qInfo() << "Package Vendor: " << QString::fromStdString(pkg.vendor().text(locale));
        qInfo() << "Package Version: " << pkg.version().toString();
        qInfo() << "Package Copyright: " << QString::fromStdString(pkg.copyright().text(locale));
        // qInfo() << "Package Description: " <<  QString::fromStdString(pkg.description().text(locale));

        qInfo() << "Package Singers:";
        auto &sc = *su->category("singer")->as<srt::SingerCategory>();
        for (const auto &singer : sc.singers()) {
            qInfo() << "- Singer:" << QString::fromStdString(singer->name().text(locale));
        }
        qInfo() << "------------- Package Info End -------------";
    };

    auto printAllPackages = [&](const std::filesystem::path &dir) {
        for (const auto &entry : fs::directory_iterator(dir)) {
            if (entry.is_directory()) {
                printPackageOne(entry.path());
            }
        }
    };

    for (const auto &path : su->packagePaths()) {
        if (!fs::exists(path) || !fs::is_directory(path)) {
            qWarning() << "Warning: " << QString::fromStdString(path.string()) <<
                " is not a valid directory\n";
            continue;
        }

        printAllPackages(path);
    }
}