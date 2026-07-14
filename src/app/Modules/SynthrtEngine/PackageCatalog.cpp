#include "PackageCatalog.h"

#include "Utils/VersionUtils.h"

#include <algorithm>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_set>

#include <diffsinger/Bank/PackageParser.h>

namespace {
    std::string pathString(const std::filesystem::path &path) {
        return path.lexically_normal().generic_string();
    }

    template <class T>
    void appendValue(std::ostringstream &stream, const T &value) {
        const auto text = [&value] {
            std::ostringstream converted;
            converted << value;
            return std::move(converted).str();
        }();
        stream << text.size() << ':' << text;
    }

    void appendDiagnostic(std::ostringstream &stream, const srt::core::Diagnostic &diagnostic) {
        appendValue(stream, static_cast<int>(diagnostic.code));
        appendValue(stream, static_cast<int>(diagnostic.severity));
        appendValue(stream, diagnostic.message);
        appendValue(stream, diagnostic.location);
        appendValue(stream, diagnostic.packageId);
        appendValue(stream, diagnostic.singerId);
        appendValue(stream, diagnostic.language);
        appendValue(stream, diagnostic.moduleId);
        appendValue(stream, diagnostic.providerKey);
        appendValue(stream, diagnostic.trace.size());
        for (const auto &entry : diagnostic.trace) {
            appendValue(stream, entry);
        }
    }

    void appendLanguage(std::ostringstream &stream, const ds::bank::LanguageInfo &language) {
        appendValue(stream, language.languageId());
        appendValue(stream, language.name());
        appendValue(stream, language.g2pVersion());
        appendValue(stream, language.g2pId());
        appendValue(stream, pathString(language.dict()));
        appendValue(stream, language.s2pMode());
        appendValue(stream, language.onsetMode());
        appendValue(stream, pathString(language.s2pFile()));
        appendValue(stream, pathString(language.onsetFile()));
        appendValue(stream, language.hasG2pPackageVersion());
        if (language.hasG2pPackageVersion()) {
            appendValue(stream, language.g2pPackageVersion().toString());
        }
        appendValue(stream, language.g2pPackages().size());
        for (const auto &path : language.g2pPackages()) {
            appendValue(stream, pathString(path));
        }
    }

    void appendSpeaker(std::ostringstream &stream, const ds::bank::SpeakerInfo &speaker) {
        appendValue(stream, speaker.speakerId());
        appendValue(stream, speaker.name());
        appendValue(stream, speaker.singerId());
    }

    void appendManifest(std::ostringstream &stream, const ds::bank::PackageManifest &manifest) {
        appendValue(stream, manifest.packageId());
        appendValue(stream, manifest.version().toString());
        appendValue(stream, pathString(manifest.rootPath()));
        appendValue(stream, manifest.name());
        appendValue(stream, manifest.author());
        appendValue(stream, manifest.description());
        appendValue(stream, manifest.license());
        appendValue(stream, manifest.singers().size());
        for (const auto &singer : manifest.singers()) {
            appendValue(stream, singer.singerId());
            appendValue(stream, singer.packageId());
            appendValue(stream, singer.packageVersion().toString());
            appendValue(stream, singer.name());
            appendValue(stream, singer.phonemeLength());
            appendValue(stream, singer.defaultLanguage());
            appendValue(stream, singer.languages().size());
            for (const auto &language : singer.languages()) {
                appendLanguage(stream, language);
            }
            appendValue(stream, singer.speakers().size());
            for (const auto &speaker : singer.speakers()) {
                appendSpeaker(stream, speaker);
            }
        }
    }

    void appendLanguageState(std::ostringstream &stream,
                             const ds::bank::PackageManifest &manifest) {
        appendValue(stream, manifest.packageId());
        appendValue(stream, manifest.version().toString());
        appendValue(stream, pathString(manifest.rootPath()));
        appendValue(stream, manifest.singers().size());
        for (const auto &singer : manifest.singers()) {
            appendValue(stream, singer.singerId());
            appendValue(stream, singer.packageId());
            appendValue(stream, singer.packageVersion().toString());
            appendValue(stream, singer.defaultLanguage());
            appendValue(stream, singer.languages().size());
            for (const auto &language : singer.languages()) {
                appendLanguage(stream, language);
            }
        }
    }

    void appendSnapshot(std::ostringstream &stream, const ds::bank::SingerSnapshot &singer) {
        appendValue(stream, singer.ref.packageId);
        appendValue(stream, singer.ref.singerId);
        appendValue(stream, singer.ref.version);
        appendValue(stream, singer.name);
        appendValue(stream, static_cast<int>(singer.resolutionState));
        appendDiagnostic(stream, singer.resolutionError);
        appendValue(stream, singer.phonemeLength);
        appendValue(stream, singer.languages.size());
        for (const auto &language : singer.languages) {
            appendValue(stream, language);
        }
        appendValue(stream, singer.speakerIds.size());
        for (const auto &speakerId : singer.speakerIds) {
            appendValue(stream, speakerId);
        }
        appendValue(stream, singer.defaultLanguage);
        appendValue(stream, singer.inferenceIds.size());
        for (const auto &inferenceId : singer.inferenceIds) {
            appendValue(stream, inferenceId);
        }
        appendValue(stream, singer.version);
    }

    void buildFingerprints(PackageCatalog::Snapshot &snapshot) {
        std::ostringstream catalog;
        std::ostringstream language;
        for (const auto &record : snapshot.packages) {
            const auto &status = record.status;
            appendValue(catalog, status.packageId);
            appendValue(catalog, status.version.toString());
            appendValue(catalog, pathString(status.rootPath));
            appendValue(catalog, status.valid);
            appendDiagnostic(catalog, status.error);
            appendValue(catalog, status.dependencies.size());
            for (const auto &dependency : status.dependencies) {
                appendValue(catalog, dependency);
            }
            appendValue(catalog, status.unresolvedDependencies.size());
            for (const auto &dependency : status.unresolvedDependencies) {
                appendValue(catalog, dependency);
            }

            appendValue(language, status.packageId);
            appendValue(language, status.version.toString());
            appendValue(language, pathString(status.rootPath));
            appendValue(language, status.valid);
            if (record.manifest) {
                appendValue(catalog, true);
                appendManifest(catalog, *record.manifest);
                appendValue(language, true);
                appendLanguageState(language, *record.manifest);
            } else if (record.parseError) {
                appendValue(catalog, false);
                appendDiagnostic(catalog, record.parseError->diagnostic());
                appendValue(language, false);
                appendDiagnostic(language, record.parseError->diagnostic());
            } else {
                appendValue(catalog, false);
                appendValue(language, false);
            }
            appendValue(catalog, record.singers.size());
            for (const auto &singer : record.singers) {
                appendSnapshot(catalog, singer);
                appendSnapshot(language, singer);
            }
        }
        snapshot.catalogFingerprint = std::move(catalog).str();
        snapshot.languageFingerprint = std::move(language).str();
    }

    bool matches(const ds::bank::PackageStatus &status, const SingerIdentifier &identifier) {
        return status.valid && status.packageId == identifier.packageId.toStdString() &&
               VersionUtils::stdc_to_qt(status.version) == identifier.packageVersion;
    }
}

PackageCatalog::PackageCatalog() : m_snapshot(std::make_shared<const Snapshot>()) {
}

PackageCatalog::Candidate::Candidate(std::unique_ptr<Snapshot> snapshot,
                                     std::vector<std::filesystem::path> searchPaths,
                                     uint64_t baseGeneration, bool unchanged)
    : m_snapshot(std::move(snapshot)), m_searchPaths(std::move(searchPaths)),
      m_baseGeneration(baseGeneration), m_unchanged(unchanged) {
}

const PackageCatalog::Snapshot &PackageCatalog::Candidate::snapshot() const {
    return *m_snapshot;
}

bool PackageCatalog::Candidate::unchanged() const noexcept {
    return m_unchanged;
}

const PackageCatalog::PackageRecord *
    PackageCatalog::Snapshot::findPackage(const SingerIdentifier &identifier) const {
    const PackageRecord *match = nullptr;
    for (const auto &package : packages) {
        if (!matches(package.status, identifier)) {
            continue;
        }
        if (match) {
            return nullptr;
        }
        match = &package;
    }
    return match;
}

const ds::bank::SingerSnapshot *
    PackageCatalog::Snapshot::findSinger(const SingerIdentifier &identifier) const {
    const auto *package = findPackage(identifier);
    if (!package) {
        return nullptr;
    }
    const ds::bank::SingerSnapshot *match = nullptr;
    for (const auto &singer : package->singers) {
        if (singer.ref.singerId != identifier.singerId.toStdString()) {
            continue;
        }
        if (match) {
            return nullptr;
        }
        match = &singer;
    }
    return match;
}

srt::core::Expected<void> PackageCatalog::validate(const Snapshot &snapshot) {
    std::unordered_set<std::string> packageIds;
    std::unordered_set<std::string> roots;
    std::set<std::tuple<std::string, std::string, std::string>> singerIdentifiers;
    for (const auto &record : snapshot.packages) {
        const auto &status = record.status;
        const auto root = pathString(status.rootPath);
        if (!roots.insert(root).second) {
            return srt::core::Error(srt::core::ErrorCode::PackageDuplicate,
                                    "Duplicate package root: " + root);
        }
        if (!status.valid) {
            continue;
        }
        if (!packageIds.insert(status.packageId).second) {
            return srt::core::Error(srt::core::ErrorCode::PackageVersionConflict,
                                    "Duplicate package ID: " + status.packageId);
        }
        if (record.manifest && (record.manifest->packageId() != status.packageId ||
                                record.manifest->version() != status.version ||
                                record.manifest->rootPath().lexically_normal() !=
                                    status.rootPath.lexically_normal())) {
            return srt::core::Error(srt::core::ErrorCode::PackageManifestInvalid,
                                    "Package status and parsed manifest identity differ at " +
                                        root);
        }
        for (const auto &singer : record.singers) {
            const auto identifier =
                std::make_tuple(singer.ref.singerId, singer.ref.packageId, singer.ref.version);
            if (!singerIdentifiers.insert(identifier).second) {
                return srt::core::Error(srt::core::ErrorCode::PackageDuplicate,
                                        "Duplicate singer identifier: " + singer.ref.singerId);
            }
        }
    }
    return {};
}

srt::core::Expected<PackageCatalog::Candidate>
    PackageCatalog::prepareRefresh(const std::vector<std::filesystem::path> &searchPaths,
                                   bool allowReuse) {
    std::scoped_lock lock(m_mutex);
    std::vector<std::filesystem::path> validSearchPaths;
    validSearchPaths.reserve(searchPaths.size());
    for (const auto &path : searchPaths) {
        std::error_code error;
        if (std::filesystem::is_directory(path, error)) {
            validSearchPaths.push_back(path.lexically_normal());
        }
    }
    if (allowReuse && m_snapshot->generation != 0 && m_searchPaths == validSearchPaths) {
        return Candidate{std::make_unique<Snapshot>(*m_snapshot), validSearchPaths,
                         m_snapshot->generation, true};
    }

    m_scanner.clear();
    m_scanner.setSearchPaths(validSearchPaths);
    auto statuses = m_scanner.refresh();
    if (!statuses) {
        return statuses.error();
    }

    auto candidate = std::make_unique<Snapshot>();
    candidate->packages.reserve(statuses->size());
    ds::bank::PackageParser parser;
    for (auto &status : *statuses) {
        PackageRecord record;
        record.status = std::move(status);
        auto parsed = parser.parsePackage(record.status.rootPath);
        if (parsed) {
            record.manifest = std::move(*parsed);
        } else {
            record.parseError = parsed.error();
        }
        candidate->packages.push_back(std::move(record));
    }

    for (const auto &singer : m_scanner.singers()) {
        const auto version = QVersionNumber::fromString(QString::fromStdString(singer.ref.version));
        for (auto &package : candidate->packages) {
            if (package.status.valid && package.status.packageId == singer.ref.packageId &&
                VersionUtils::stdc_to_qt(package.status.version) == version) {
                package.singers.push_back(singer);
            }
        }
    }
    std::sort(
        candidate->packages.begin(), candidate->packages.end(),
        [](const PackageRecord &left, const PackageRecord &right) {
            return std::tie(left.status.packageId, left.status.version, left.status.rootPath) <
                   std::tie(right.status.packageId, right.status.version, right.status.rootPath);
        });
    for (auto &package : candidate->packages) {
        std::sort(package.singers.begin(), package.singers.end(),
                  [](const ds::bank::SingerSnapshot &left, const ds::bank::SingerSnapshot &right) {
                      return std::tie(left.ref.packageId, left.ref.singerId, left.ref.version) <
                             std::tie(right.ref.packageId, right.ref.singerId, right.ref.version);
                  });
    }
    if (auto valid = validate(*candidate); !valid) {
        return valid.error();
    }
    buildFingerprints(*candidate);
    const bool unchanged = m_snapshot->generation != 0 &&
                           candidate->catalogFingerprint == m_snapshot->catalogFingerprint &&
                           m_searchPaths == validSearchPaths;
    return Candidate{std::move(candidate), std::move(validSearchPaths), m_snapshot->generation,
                     unchanged};
}

srt::core::Expected<std::shared_ptr<const PackageCatalog::Snapshot>>
    PackageCatalog::commit(Candidate candidate) {
    std::scoped_lock lock(m_mutex);
    if (candidate.m_baseGeneration != m_snapshot->generation) {
        return srt::core::Error(srt::core::ErrorCode::Aborted,
                                "Package catalog changed while refresh was being prepared");
    }
    if (candidate.m_unchanged) {
        return m_snapshot;
    }
    if (!candidate.m_snapshot) {
        return srt::core::Error(srt::core::ErrorCode::InvalidArgument,
                                "Package catalog candidate has no snapshot");
    }
    if (auto valid = validate(*candidate.m_snapshot); !valid) {
        return valid.error();
    }
    candidate.m_snapshot->generation = m_snapshot->generation + 1;
    m_snapshot = std::shared_ptr<const Snapshot>(std::move(candidate.m_snapshot));
    m_searchPaths = std::move(candidate.m_searchPaths);
    return m_snapshot;
}

std::shared_ptr<const PackageCatalog::Snapshot> PackageCatalog::snapshot() const {
    std::scoped_lock lock(m_mutex);
    return m_snapshot;
}
