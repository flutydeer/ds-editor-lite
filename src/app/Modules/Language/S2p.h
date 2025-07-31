#ifndef DS_EDITOR_LITE_S2P_H
#define DS_EDITOR_LITE_S2P_H

#include <QApplication>

#include "S2p/Syllable2p.h"
#include "Utils/Singleton.h"

#include "Model/AppOptions/AppOptions.h"

class S2p final : public Singleton<S2p> {
public:
    explicit S2p() {
        m_s2ps.insert("cmn", new FillLyric::Syllable2p(getDefaultDictPath("cmn")));
        m_s2ps.insert("jpn", new FillLyric::Syllable2p(getDefaultDictPath("jpn")));
        m_s2ps.insert("yue", new FillLyric::Syllable2p(getDefaultDictPath("yue")));
    }

    QStringList syllableToPhoneme(const QString &syllable, const QString &language) const {
        const auto &it = m_s2ps.find(language);
        return it != m_s2ps.end() ? it.value()->syllableToPhoneme(syllable) : QStringList();
    }

    QVector<QStringList> syllableToPhoneme(const QList<QPair<QString, QString>> &syllables) const {
        QVector<QStringList> result;
        for (const auto &[syllable, language] : syllables)
            result.append(syllableToPhoneme(syllable, language));
        return result;
    }

private:
    QMap<QString, FillLyric::Syllable2p *> m_s2ps;

    // TODO: support multi-dict
    static QString getDefaultDictPath(const QString &language) {
        auto dictPath = QStringLiteral("opencpop-extension.txt");
        if (language == "jpn") {
            dictPath = QStringLiteral("japanese_dict_full.txt");
        } else if (language == "yue") {
            dictPath = QStringLiteral("jyutping_dict.txt");
        }
        return QApplication::applicationDirPath() + "/" +
#ifdef Q_OS_MAC
               QStringLiteral("../Resources/phonemeDict/")
#else
               QStringLiteral("Resources/phonemeDict/")
#endif
               + dictPath;
    }
};

#endif // DS_EDITOR_LITE_S2P_H