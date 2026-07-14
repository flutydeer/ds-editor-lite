#ifndef PACKAGECATALOG_H
#define PACKAGECATALOG_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include <diffsinger/Bank/PackageManifest.h>
#include <diffsinger/Bank/PackageStatus.h>
#include <diffsinger/Bank/SingerSnapshot.h>
#include <diffsinger/Bank/VoicebankScanner.h>
#include <synthrt/Core/Support/Error.h>
#include <synthrt/Core/Support/Expected.h>

#include "Modules/Inference/Models/SingerIdentifier.h"

class PackageCatalog final {
public:
    struct PackageRecord {
        ds::bank::PackageStatus status;
        std::optional<ds::bank::PackageManifest> manifest;
        std::optional<srt::core::Error> parseError;
        std::vector<ds::bank::SingerSnapshot> singers;
    };

    struct Snapshot {
        uint64_t generation = 0;
        std::vector<PackageRecord> packages;
        std::string catalogFingerprint;
        std::string languageFingerprint;

        const PackageRecord *findPackage(const SingerIdentifier &identifier) const;
        const ds::bank::SingerSnapshot *findSinger(const SingerIdentifier &identifier) const;
    };

    struct Candidate {
        Candidate(Candidate &&) noexcept = default;
        Candidate &operator=(Candidate &&) noexcept = default;

        Candidate(const Candidate &) = delete;
        Candidate &operator=(const Candidate &) = delete;

        const Snapshot &snapshot() const;
        bool unchanged() const noexcept;

    private:
        friend class PackageCatalog;

        Candidate(std::unique_ptr<Snapshot> snapshot,
                  std::vector<std::filesystem::path> searchPaths, uint64_t baseGeneration,
                  bool unchanged);

        std::unique_ptr<Snapshot> m_snapshot;
        std::vector<std::filesystem::path> m_searchPaths;
        uint64_t m_baseGeneration = 0;
        bool m_unchanged = false;
    };

    PackageCatalog();

    srt::core::Expected<Candidate>
        prepareRefresh(const std::vector<std::filesystem::path> &searchPaths,
                       bool allowReuse = true);
    srt::core::Expected<std::shared_ptr<const Snapshot>> commit(Candidate candidate);
    std::shared_ptr<const Snapshot> snapshot() const;

    static srt::core::Expected<void> validate(const Snapshot &snapshot);

private:
    mutable std::mutex m_mutex;
    ds::bank::VoicebankScanner m_scanner;
    std::shared_ptr<const Snapshot> m_snapshot;
    std::vector<std::filesystem::path> m_searchPaths;
};

#endif // PACKAGECATALOG_H
