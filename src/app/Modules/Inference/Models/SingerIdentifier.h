#ifndef SINGER_IDENTIFIER_H
#define SINGER_IDENTIFIER_H

#include <QString>
#include <QVersionNumber>
#include <QHash>
#include <QDebug>
#include <functional> // std::hash
#include <tuple>      // std::tie

struct SingerIdentifier {
    QString singerId;
    QString packageId;
    QVersionNumber packageVersion;

    inline bool operator==(const SingerIdentifier &other) const noexcept {
        return singerId == other.singerId && packageId == other.packageId &&
               packageVersion == other.packageVersion;
    }

    inline bool operator<(const SingerIdentifier &other) const noexcept {
        return std::tie(singerId, packageId, packageVersion) <
               std::tie(other.singerId, other.packageId, other.packageVersion);
    }
};

// qHash for Qt containers
inline size_t qHash(const SingerIdentifier &key, size_t seed = 0) noexcept {
    seed = ::qHash(key.singerId, seed);
    seed = ::qHash(key.packageId, seed);
    seed = ::qHash(key.packageVersion, seed);
    return seed;
}

// std::hash for STL containers like std::unordered_map, std::unordered_set
namespace std {
    template <>
    struct hash<SingerIdentifier> {
        size_t operator()(const SingerIdentifier &key) const noexcept {
            return static_cast<size_t>(::qHash(key));
        }
    };
}

Q_DECLARE_TYPEINFO(SingerIdentifier, Q_MOVABLE_TYPE);

inline QDebug operator<<(QDebug debug, const SingerIdentifier &info) {
    debug.nospace() << "SingerInfo(singerId=" << info.singerId << ", packageId=" << info.packageId
                    << ", packageVersion=" << info.packageVersion << ")";
    return debug;
}

#endif // SINGER_IDENTIFIER_H