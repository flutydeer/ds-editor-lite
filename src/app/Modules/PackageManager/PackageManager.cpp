//
// Created by FlutyDeer on 2025/7/27.
//

#include "PackageManager.h"

#include "Model/AppStatus/AppStatus.h"
#include "Modules/Inference/InferEngine.h"
#include "Utils/StringUtils.h"
#include "Utils/VersionUtils.h"
#include "Models/PackageInfo.h"
#include "Models/SingerInfo.h"

#include <stdcorelib/path.h>

#include <synthrt/Core/SynthUnit.h>
#include <synthrt/Core/PackageRef.h>
#include <synthrt/SVS/SingerContrib.h>
#include <synthrt/Support/JSON.h>

#include <QDebug>
#include <QLocale>
#include <mutex>

namespace fs = std::filesystem;

namespace {
    QString parseDisplayText(const srt::JsonValue &textValue, const std::string &currentLocale) {
        if (textValue.isString()) {
            return QString::fromUtf8(textValue.toString());
        }
        if (!textValue.isObject()) {
            return {};
        }
        const auto &textObject = textValue.toObject();

        // First, look for the exact current locale
        if (const auto it = textObject.find(currentLocale); it != textObject.end()) {
            return QString::fromUtf8(it->second.toString());
        }

        // If not found, fall back to the default key "_"
        if (const auto it = textObject.find("_"); it != textObject.end()) {
            return QString::fromUtf8(it->second.toString());
        }

        return {};
    }

    QString joinPath(const std::string &originalPath, const std::filesystem::path &parentPath) {
        if (originalPath.empty()) {
            return {};
        }
        auto path = stdc::path::from_utf8(originalPath);
        if (path.is_relative() && !parentPath.empty()) {
            path = stdc::clean_path(parentPath / path);
        }
        return StringUtils::path_to_qstr(path);
    }

    QList<SpeakerInfo> parseSpeakerInfo(const srt::JsonObject &manifestConfiguration,
                                        const std::string &currentLocale) {
        QList<SpeakerInfo> result;
        const auto it_speakers = manifestConfiguration.find("speakers");
        if (it_speakers == manifestConfiguration.end()) {
            return result;
        }
        const auto &speakersValue = it_speakers->second;
        if (!speakersValue.isArray()) {
            return result;
        }
        const auto &speakers = speakersValue.toArray();
        result.reserve(static_cast<QList<SpeakerInfo>::size_type>(speakers.size()));
        for (const auto &speakerValue : speakers) {
            if (!speakerValue.isObject()) {
                continue;
            }
            const auto &speaker = speakerValue.toObject();
            QString speakerId;
            if (const auto it = speaker.find("id"); it != speaker.end()) {
                speakerId.assign(it->second.toString());
            }

            QString speakerName;
            if (const auto it = speaker.find("name"); it != speaker.end()) {
                if (auto val = parseDisplayText(it->second, currentLocale); !val.isEmpty()) {
                    speakerName = std::move(val);
                }
            }

            QString speakerToneMin, speakerToneMax;
            if (const auto it = speaker.find("toneRanges"); it != speaker.end()) {
                if (it->second.isObject()) {
                    const auto &toneRanges = it->second.toObject();
                    if (const auto it_min = toneRanges.find("min"); it_min != toneRanges.end()) {
                        speakerToneMin = QString::fromUtf8(it->second.toString());
                    }
                    if (const auto it_max = toneRanges.find("max"); it_max != toneRanges.end()) {
                        speakerToneMax = QString::fromUtf8(it->second.toString());
                    }
                }
            }
            result.emplace_back(std::move(speakerId), std::move(speakerName),
                                std::move(speakerToneMin), std::move(speakerToneMax));
        }
        return result;
    }

    QList<LanguageInfo> parseLanguageInfo(const srt::JsonObject &manifestConfiguration,
                                          const std::string &currentLocale,
                                          const std::filesystem::path &singerPath) {
        QList<LanguageInfo> result;
        const auto it_languages = manifestConfiguration.find("languages");
        if (it_languages == manifestConfiguration.end()) {
            return result;
        }
        const auto &languagesValue = it_languages->second;
        if (!languagesValue.isArray()) {
            return result;
        }
        const auto &languages = languagesValue.toArray();
        result.reserve(static_cast<QList<LanguageInfo>::size_type>(languages.size()));
        for (const auto &languageValue : languages) {
            if (!languageValue.isObject()) {
                continue;
            }
            const auto &language = languageValue.toObject();
            QString languageId;
            if (const auto it = language.find("id"); it != language.end()) {
                languageId.assign(it->second.toString());
            }
            QString languageName;
            if (const auto it = language.find("name"); it != language.end()) {
                if (auto val = parseDisplayText(it->second, currentLocale); !val.isEmpty()) {
                    languageName = std::move(val);
                }
            }
            QString languageG2p;
            if (const auto it = language.find("g2p"); it != language.end()) {
                languageG2p.assign(it->second.toString());
            }
            QString languageDict;
            if (const auto it = language.find("dict"); it != language.end()) {
                languageDict = joinPath(it->second.toString(), singerPath);
            }
            result.emplace_back(std::move(languageId), std::move(languageName),
                                std::move(languageG2p), std::move(languageDict));
        }
        return result;
    }

    QString parseDefaultLanguage(const srt::JsonObject &manifestConfiguration) {
        const auto it = manifestConfiguration.find("defaultLanguage");
        if (it == manifestConfiguration.end()) {
            return {};
        }
        return QString::fromUtf8(it->second.toString());
    }

    QString parseDefaultDict(const srt::JsonObject &manifestConfiguration,
                             const std::filesystem::path &singerPath) {
        const auto it = manifestConfiguration.find("defaultDict");
        if (it == manifestConfiguration.end()) {
            return {};
        }
        return joinPath(it->second.toString(), singerPath);
    }
}

PackageManager::PackageManager() {
    connect(appStatus, &AppStatus::moduleStatusChanged, this,
            &PackageManager::onModuleStatusChanged);
}

Expected<GetInstalledPackagesResult, GetInstalledPackagesError>
    PackageManager::refreshInstalledPackages() {

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
    const auto locale = QLocale::system().name().toStdString();

    auto processPackage = [&](const std::filesystem::path &packagePath) {
        if (auto exp = su.open(packagePath, true); !exp) {
            result.failedPackages.emplace_back(
                StringUtils::path_to_qstr(packagePath),
                srtErrorToString(exp.error())
            );
        } else {
            const srt::ScopedPackageRef pkg(exp.take());

            auto packageId = QString::fromUtf8(pkg.id());
            auto packageVersion = VersionUtils::stdc_to_qt(pkg.version());
            auto vendor = QString::fromUtf8(pkg.vendor().text(locale));
            auto description = QString::fromUtf8(pkg.description().text(locale));
            auto copyright = QString::fromUtf8(pkg.copyright().text(locale));
            auto path = StringUtils::native_to_qstr(pkg.path());

            const auto contribSpecs = pkg.contributes("singer");
            QList<SingerInfo> singers;
            singers.reserve(static_cast<QList<SingerInfo>::size_type>(contribSpecs.size()));

            for (const auto contribSpec : contribSpecs) {
                // NOLINTNEXTLINE(*-pro-type-static-cast-downcast)
                const auto singerSpec = static_cast<const srt::SingerSpec *>(contribSpec);
                if (!singerSpec) {
                    continue;
                }
                auto singerId = QString::fromUtf8(singerSpec->id());
                auto singerName = QString::fromUtf8(singerSpec->name().text(locale));
                const auto &singerPath = singerSpec->path();
                const auto &manifestConfiguration = singerSpec->manifestConfiguration();
                auto languageInfos = parseLanguageInfo(manifestConfiguration, locale, singerPath);
                auto defaultLanguage = parseDefaultLanguage(manifestConfiguration);
                auto defaultDict = parseDefaultDict(manifestConfiguration, singerPath);
                // If defaultDict is not specified, use the dict of defaultLanguage
                if (defaultDict.isEmpty()) {
                    if (!defaultLanguage.isEmpty()) {
                        for (const auto &languageInfo : std::as_const(languageInfos)) {
                            if (languageInfo.id() != defaultLanguage) {
                                continue;
                            }
                            if (const auto &dict = languageInfo.dict(); !dict.isEmpty()) {
                                defaultDict = dict;
                            }
                            break;
                        }
                    }
                }
                singers.emplace_back(SingerIdentifier{singerId, packageId, packageVersion},
                                     singerName, parseSpeakerInfo(manifestConfiguration, locale),
                                     std::move(languageInfos), std::move(defaultLanguage),
                                     std::move(defaultDict));
            }
            result.successfulPackages.emplace_back(packageId, packageVersion, vendor, description,
                                                   copyright, path, singers);
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
            result.failedPackages.emplace_back(
                StringUtils::path_to_qstr(path),
                tr("Path is not a valid directory")
            );
            continue;
        }
        processAllPackages(path);
    }

    qDebug() << "Package scan completed in" << timer.elapsed() << "ms";
    {
        QWriteLocker writeLocker(&m_resultRwLock);
        m_result = result;
        m_packageLocator.clear();
        m_singerLocator.clear();
        for (const auto &packageInfo : std::as_const(m_result.successfulPackages)) {
            const auto singers = packageInfo.singers();
            for (const auto &singerInfo : singers) {
                const auto identifier = singerInfo.identifier();
                m_packageLocator[identifier] = packageInfo;
                m_singerLocator[identifier] = singerInfo;
            }
        }
        Q_EMIT packagesRefreshed(m_result.successfulPackages);
    }
    return result;
}

GetInstalledPackagesResult PackageManager::installedPackages() const {
    QReadLocker readLocker(&m_resultRwLock);
    return m_result;
}

PackageInfo PackageManager::findPackageByIdentifier(const SingerIdentifier &identifier) const {
    QReadLocker readLocker(&m_resultRwLock);
    const auto it = m_packageLocator.constFind(identifier);
    if (it == m_packageLocator.constEnd()) {
        return {};
    }
    return it.value();
}

SingerInfo PackageManager::findSingerByIdentifier(const SingerIdentifier &identifier) const {
    QReadLocker readLocker(&m_resultRwLock);
    const auto it = m_singerLocator.constFind(identifier);
    if (it == m_singerLocator.constEnd()) {
        return {};
    }
    return it.value();
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