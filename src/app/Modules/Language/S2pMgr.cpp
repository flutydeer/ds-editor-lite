#include "S2pMgr.h"

#include <PhonemeConverter/DictionaryS2P.h>
#include <PhonemeConverter/DirectS2P.h>
#include <PhonemeConverter/LuaS2P.h>
#include <PhonemeConverter/LuaScript.h>
#include <PhonemeConverter/MappingS2P.h>

#include <QCryptographicHash>
#include <QDebug>
#include <QFile>

#include <sstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
    QStringList toQStringList(const std::vector<std::string> &phonemes) {
        QStringList result;
        result.reserve(static_cast<QStringList::size_type>(phonemes.size()));
        for (const auto &phoneme : phonemes) {
            result.append(QString::fromUtf8(phoneme.data(), static_cast<int>(phoneme.size())));
        }
        return result;
    }

    std::string toUtf8String(const QByteArray &bytes) {
        return {bytes.constData(), static_cast<std::string::size_type>(bytes.size())};
    }

    QByteArray readAll(const QString &filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qCritical() << "S2pMgr: failed to open file:" << filePath;
            return {};
        }
        return file.readAll();
    }
}

class IS2pConverter {
public:
    virtual ~IS2pConverter() = default;
    virtual QStringList convert(const QString &pronunciation) const = 0;
};

class DirectS2pConverter final : public IS2pConverter {
public:
    QStringList convert(const QString &pronunciation) const override {
        const auto utf8 = pronunciation.trimmed().toUtf8();
        return toQStringList(PhonemeConverter::DirectS2P::convert(
            std::string_view(utf8.constData(), static_cast<size_t>(utf8.size()))));
    }
};

class DictionaryS2pConverter final : public IS2pConverter {
public:
    explicit DictionaryS2pConverter(const QByteArray &fileContent) {
        std::istringstream stream(toUtf8String(fileContent));
        m_converter = std::make_unique<PhonemeConverter::DictionaryS2P>(stream);
    }

    QStringList convert(const QString &pronunciation) const override {
        const auto utf8 = pronunciation.toUtf8();
        return toQStringList(m_converter->convert(
            std::string_view(utf8.constData(), static_cast<size_t>(utf8.size()))));
    }

private:
    std::unique_ptr<PhonemeConverter::DictionaryS2P> m_converter;
};

class MappingS2pConverter final : public IS2pConverter {
public:
    explicit MappingS2pConverter(const QByteArray &fileContent) {
        std::istringstream stream(toUtf8String(fileContent));
        m_converter = std::make_unique<PhonemeConverter::MappingS2P>(stream);
    }

    QStringList convert(const QString &pronunciation) const override {
        const auto utf8 = pronunciation.trimmed().toUtf8();
        return toQStringList(m_converter->convert(
            std::string_view(utf8.constData(), static_cast<size_t>(utf8.size()))));
    }

private:
    std::unique_ptr<PhonemeConverter::MappingS2P> m_converter;
};

class LuaS2pConverter final : public IS2pConverter {
public:
    LuaS2pConverter(const QByteArray &fileContent, const QString &chunkName) {
        const PhonemeConverter::LuaScript script(toUtf8String(fileContent),
                                                 chunkName.toUtf8().constData());
        m_converter = std::make_unique<PhonemeConverter::LuaS2P>(script);
    }

    QStringList convert(const QString &pronunciation) const override {
        const auto utf8 = pronunciation.toUtf8();
        return toQStringList(m_converter->convert(
            std::string_view(utf8.constData(), static_cast<size_t>(utf8.size()))));
    }

private:
    std::unique_ptr<PhonemeConverter::LuaS2P> m_converter;
};

S2pMgr::S2pMgr() {
}

S2pMgr::~S2pMgr() {
    qDeleteAll(m_cacheKeyToS2p);
    m_cacheKeyToS2p.clear();
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(S2pMgr)

QByteArray S2pMgr::calculateFileHash(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&file)) {
        return hash.result();
    }

    return {};
}

QString S2pMgr::cacheKey(const QString &mode, const QString &filePath, const QByteArray &hash) {
    if (mode == QStringLiteral("direct")) {
        return QStringLiteral("direct");
    }
    return mode + QLatin1Char(':') + filePath + QLatin1Char(':') +
           QString::fromLatin1(hash.toHex());
}

void S2pMgr::addS2p(const SingerIdentifier &singerId, const QString &g2pId, const QString &mode,
                    const QString &filePath) {
    const auto normalizedMode = mode.trimmed().toLower();
    if (normalizedMode.isEmpty()) {
        qWarning() << "S2pMgr: missing s2p mode for singer:" << singerId << "g2pId:" << g2pId;
        return;
    }

    QByteArray fileHash;
    QByteArray fileContent;
    if (normalizedMode != QStringLiteral("direct")) {
        if (filePath.isEmpty()) {
            qWarning() << "S2pMgr: missing s2p file for singer:" << singerId << "g2pId:" << g2pId
                       << "mode:" << normalizedMode;
            return;
        }
        fileHash = calculateFileHash(filePath);
        if (fileHash.isEmpty()) {
            qWarning() << "S2pMgr: failed to calculate hash for file:" << filePath;
            return;
        }
        fileContent = readAll(filePath);
        if (fileContent.isEmpty()) {
            qWarning() << "S2pMgr: empty or unreadable s2p file:" << filePath;
            return;
        }
    }

    const auto key = cacheKey(normalizedMode, filePath, fileHash);
    m_idToCacheKey[{singerId, g2pId}] = key;

    if (m_cacheKeyToS2p.contains(key)) {
        return;
    }

    try {
        if (normalizedMode == QStringLiteral("direct")) {
            m_cacheKeyToS2p[key] = new DirectS2pConverter;
        } else if (normalizedMode == QStringLiteral("dict")) {
            m_cacheKeyToS2p[key] = new DictionaryS2pConverter(fileContent);
        } else if (normalizedMode == QStringLiteral("map")) {
            m_cacheKeyToS2p[key] = new MappingS2pConverter(fileContent);
        } else if (normalizedMode == QStringLiteral("custom")) {
            m_cacheKeyToS2p[key] = new LuaS2pConverter(fileContent, filePath);
        } else {
            m_idToCacheKey.remove({singerId, g2pId});
            qWarning() << "S2pMgr: unsupported s2p mode:" << mode << "singer:" << singerId
                       << "g2pId:" << g2pId;
        }
    } catch (const std::exception &e) {
        m_idToCacheKey.remove({singerId, g2pId});
        qCritical() << "S2pMgr: failed to register s2p:" << "singer:" << singerId
                    << "g2pId:" << g2pId << "mode:" << normalizedMode << "file:" << filePath
                    << "error:" << e.what();
    }
}

QStringList S2pMgr::syllableToPhoneme(const SingerIdentifier &singerIdentifier,
                                      const QString &g2pId,
                                      const QString &syllable) {
    const auto it = m_idToCacheKey.constFind({singerIdentifier, g2pId});
    if (it == m_idToCacheKey.constEnd()) {
        if (singerIdentifier.isEmpty()) {
            qWarning() << "No Singer, S2p can not work.";
        } else {
            qWarning() << "No S2p instance found for singer:" << singerIdentifier
                       << "g2pId:" << g2pId;
        }
        return {};
    }
    const auto s2pInstance = m_cacheKeyToS2p.value(it.value(), nullptr);
    if (!s2pInstance) {
        qWarning() << "No S2p instance found for cache key:" << it.value()
                   << "singer:" << singerIdentifier << "g2pId:" << g2pId;
        return {};
    }
    try {
        return s2pInstance->convert(syllable);
    } catch (const std::exception &e) {
        qCritical() << "S2pMgr: failed to convert pronunciation:" << syllable
                    << "singer:" << singerIdentifier << "g2pId:" << g2pId
                    << "error:" << e.what();
        return {};
    }
}

QList<QStringList> S2pMgr::syllableToPhoneme(const SingerIdentifier &singerIdentifier,
                                             const QString &g2pId,
                                             const QStringList &syllables) {
    const auto it = m_idToCacheKey.constFind({singerIdentifier, g2pId});
    if (it == m_idToCacheKey.constEnd()) {
        qWarning() << "No S2p instance found for singer:" << singerIdentifier
                   << "g2pId:" << g2pId;
        return {};
    }

    const auto s2pInstance = m_cacheKeyToS2p.value(it.value(), nullptr);
    if (!s2pInstance) {
        qWarning() << "No S2p instance found for cache key:" << it.value()
                   << "singer:" << singerIdentifier << "g2pId:" << g2pId;
        return {};
    }

    QVector<QStringList> result;
    if (syllables.isEmpty()) {
        return result;
    }
    result.reserve(syllables.size());
    for (const auto &syllable : syllables) {
        try {
            result.append(s2pInstance->convert(syllable));
        } catch (const std::exception &e) {
            qCritical() << "S2pMgr: failed to convert pronunciation:" << syllable
                        << "singer:" << singerIdentifier << "g2pId:" << g2pId
                        << "error:" << e.what();
            result.append(QStringList{});
        }
    }
    return result;
}
