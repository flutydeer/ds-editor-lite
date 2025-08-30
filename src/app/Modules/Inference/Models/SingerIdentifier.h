#ifndef SINGER_IDENTIFIER_H
#define SINGER_IDENTIFIER_H

#include <QString>
#include <QVersionNumber>
#include <QHashFunctions>
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

    bool isEmpty() const noexcept {
        return singerId.isEmpty() && packageId.isEmpty() && packageVersion.isNull();
    }
};

Q_DECLARE_TYPEINFO(SingerIdentifier, Q_RELOCATABLE_TYPE);

// qHash for Qt containers
inline size_t qHash(const SingerIdentifier &key, size_t seed = 0) noexcept {
    return ::qHashMulti(seed, key.singerId, key.packageId, key.packageVersion);
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

Q_DECLARE_METATYPE(SingerIdentifier)

inline QDebug &operator<<(QDebug &debug, const SingerIdentifier &info) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "SingerIdentifier(singerId=" << info.singerId << ", packageId=" << info.packageId
                    << ", packageVersion=" << info.packageVersion << ")";
    return debug;
}

#endif // SINGER_IDENTIFIER_H