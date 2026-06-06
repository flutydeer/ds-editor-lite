#include "OnsetMarkerMgr.h"

#include <PhonemeConverter/LuaOnsetMarker.h>
#include <PhonemeConverter/LuaScript.h>
#include <PhonemeConverter/RuleOnsetMarker.h>

#include <QDebug>
#include <QFile>

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    QByteArray readAll(const QString &filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qCritical() << "OnsetMarkerMgr: failed to open file:" << filePath;
            return {};
        }
        return file.readAll();
    }

    std::string toUtf8String(const QByteArray &bytes) {
        return {bytes.constData(), static_cast<std::string::size_type>(bytes.size())};
    }

    std::vector<std::string> toStdVector(const QStringList &phonemeNames) {
        std::vector<std::string> result;
        result.reserve(static_cast<std::vector<std::string>::size_type>(phonemeNames.size()));
        for (const auto &name : phonemeNames) {
            result.push_back(name.toUtf8().toStdString());
        }
        return result;
    }

    QList<PhonemeName> toPhonemeNames(const QStringList &phonemeNames,
                                      const std::vector<bool> &onsets,
                                      const QString &language) {
        if (onsets.size() != static_cast<size_t>(phonemeNames.size())) {
            qCritical() << "OnsetMarkerMgr: onset result length mismatch:"
                        << "phonemeCount:" << phonemeNames.size()
                        << "onsetCount:" << static_cast<int>(onsets.size());
            return {};
        }

        QList<PhonemeName> result;
        result.reserve(phonemeNames.size());
        for (int i = 0; i < phonemeNames.size(); i++) {
            PhonemeName phoneme;
            phoneme.name = phonemeNames[i];
            phoneme.language = language;
            phoneme.isOnset = onsets[static_cast<size_t>(i)];
            result.append(phoneme);
        }
        return result;
    }
}

class RuleOnsetMarkerAdapter final : public IOnsetMarker {
public:
    explicit RuleOnsetMarkerAdapter(const QByteArray &fileContent) {
        std::istringstream stream(toUtf8String(fileContent));
        m_marker = std::make_unique<PhonemeConverter::RuleOnsetMarker>(stream);
    }

    QList<PhonemeName> mark(const QStringList &phonemeNames,
                            const QString &language) const override {
        try {
            return toPhonemeNames(phonemeNames, m_marker->mark(toStdVector(phonemeNames)),
                                  language);
        } catch (const std::exception &e) {
            qCritical() << "RuleOnsetMarkerAdapter: failed to mark onset:" << e.what();
            return {};
        }
    }

private:
    std::unique_ptr<PhonemeConverter::RuleOnsetMarker> m_marker;
};

class LuaOnsetMarkerAdapter final : public IOnsetMarker {
public:
    LuaOnsetMarkerAdapter(const QByteArray &fileContent, const QString &chunkName) {
        const PhonemeConverter::LuaScript script(toUtf8String(fileContent),
                                                 chunkName.toUtf8().constData());
        m_marker = std::make_unique<PhonemeConverter::LuaOnsetMarker>(script);
    }

    QList<PhonemeName> mark(const QStringList &phonemeNames,
                            const QString &language) const override {
        try {
            return toPhonemeNames(phonemeNames, m_marker->mark(toStdVector(phonemeNames)),
                                  language);
        } catch (const std::exception &e) {
            qCritical() << "LuaOnsetMarkerAdapter: failed to mark onset:" << e.what();
            return {};
        }
    }

private:
    std::unique_ptr<PhonemeConverter::LuaOnsetMarker> m_marker;
};

LITE_SINGLETON_IMPLEMENT_INSTANCE(OnsetMarkerMgr)

OnsetMarkerMgr::OnsetMarkerMgr() {
}

OnsetMarkerMgr::~OnsetMarkerMgr() {
    qDeleteAll(m_markers);
    m_markers.clear();
}

QString OnsetMarkerMgr::markerKey(const SingerIdentifier &singerIdentifier,
                                  const QString &language) {
    return singerIdentifier.packageId + QLatin1Char('|') +
           singerIdentifier.packageVersion.toString() + QLatin1Char('|') +
           singerIdentifier.singerId + QLatin1Char('|') + language;
}

void OnsetMarkerMgr::registerMarker(const SingerIdentifier &singerIdentifier,
                                    const QString &language, IOnsetMarker *marker) {
    const auto key = markerKey(singerIdentifier, language);
    delete m_markers.take(key);
    if (marker) {
        m_markers.insert(key, marker);
    }
}

bool OnsetMarkerMgr::addOnsetMarker(const SingerIdentifier &singerIdentifier,
                                    const QString &language, const QString &mode,
                                    const QString &filePath) {
    const auto normalizedMode = mode.trimmed().toLower();
    if (normalizedMode.isEmpty()) {
        qWarning() << "OnsetMarkerMgr: missing onset mode for singer:" << singerIdentifier
                   << "language:" << language;
        return false;
    }
    if (filePath.isEmpty()) {
        qWarning() << "OnsetMarkerMgr: missing onset file for singer:" << singerIdentifier
                   << "language:" << language << "mode:" << normalizedMode;
        return false;
    }

    const auto fileContent = readAll(filePath);
    if (fileContent.isEmpty()) {
        qWarning() << "OnsetMarkerMgr: empty or unreadable onset file:" << filePath;
        return false;
    }

    try {
        if (normalizedMode == QStringLiteral("rule")) {
            registerMarker(singerIdentifier, language, new RuleOnsetMarkerAdapter(fileContent));
            return true;
        }
        if (normalizedMode == QStringLiteral("custom")) {
            registerMarker(singerIdentifier, language,
                           new LuaOnsetMarkerAdapter(fileContent, filePath));
            return true;
        }
        qWarning() << "OnsetMarkerMgr: unsupported onset mode:" << mode
                   << "singer:" << singerIdentifier << "language:" << language;
    } catch (const std::exception &e) {
        qCritical() << "OnsetMarkerMgr: failed to register onset marker:"
                    << "singer:" << singerIdentifier << "language:" << language
                    << "mode:" << normalizedMode << "file:" << filePath << "error:" << e.what();
    }
    return false;
}

bool OnsetMarkerMgr::loadRuleBasedMarker(const SingerIdentifier &singerIdentifier,
                                         const QString &language, const QString &configPath) {
    return addOnsetMarker(singerIdentifier, language, QStringLiteral("rule"), configPath);
}

const IOnsetMarker *OnsetMarkerMgr::marker(const SingerIdentifier &singerIdentifier,
                                           const QString &language) const {
    if (const auto it = m_markers.find(markerKey(singerIdentifier, language));
        it != m_markers.end()) {
        return it.value();
    }
    qWarning() << "OnsetMarkerMgr: no onset marker found for singer:" << singerIdentifier
               << "language:" << language;
    return nullptr;
}
