#ifndef SINGERG2PIDENTIFIER_H
#define SINGERG2PIDENTIFIER_H

#include <functional>
#include <tuple>

#include <QDebug>
#include <QHashFunctions>
#include <QMetaType>
#include <QString>

#include "Modules/Inference/Models/SingerIdentifier.h"

struct SingerG2pIdentifier {
    SingerIdentifier singerIdentifier;
    QString g2pId;

    inline bool operator==(const SingerG2pIdentifier &other) const noexcept {
        return singerIdentifier == other.singerIdentifier && g2pId == other.g2pId;
    }

    inline bool operator<(const SingerG2pIdentifier &other) const noexcept {
        return std::tie(singerIdentifier, g2pId) < std::tie(other.singerIdentifier, other.g2pId);
    }

    bool isEmpty() const noexcept {
        return singerIdentifier.isEmpty() && g2pId.isEmpty();
    }
};

Q_DECLARE_TYPEINFO(SingerG2pIdentifier, Q_RELOCATABLE_TYPE);

// qHash for Qt containers
inline size_t qHash(const SingerG2pIdentifier &key, size_t seed = 0) noexcept {
    return ::qHashMulti(seed, key.singerIdentifier, key.g2pId);
}

// std::hash for STL containers like std::unordered_map, std::unordered_set
namespace std {
    template <>
    struct hash<SingerG2pIdentifier> {
        size_t operator()(const SingerG2pIdentifier &key) const noexcept {
            return static_cast<size_t>(::qHash(key));
        }
    };
}

Q_DECLARE_METATYPE(SingerG2pIdentifier)

inline QDebug &operator<<(QDebug &debug, const SingerG2pIdentifier &info) {
    QDebugStateSaver saver(debug);
    debug.nospace() << "SingerG2pIdentifier(singerId=" << info.singerIdentifier.singerId
                    << ", packageId=" << info.singerIdentifier.packageId
                    << ", packageVersion=" << info.singerIdentifier.packageVersion
                    << ", g2pId=" << info.g2pId << ")";
    return debug;
}

#endif // SINGERG2PIDENTIFIER_H