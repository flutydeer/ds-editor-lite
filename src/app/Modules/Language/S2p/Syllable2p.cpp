#include "Syllable2p.h"
#include "Syllable2p_p.h"

#include <iostream>
#include <type_traits>

#include <QFile>
#include <QDebug>
#include <QVarLengthArray>



#include <utility>

namespace FillLyric {
    Syllable2pPrivate::Syllable2pPrivate(QString dictPath) : dictPath(std::move(dictPath)) {
    }

    Syllable2pPrivate::~Syllable2pPrivate() = default;

    void Syllable2pPrivate::init() {
        const std::filesystem::path &path = dictPath.
#ifdef _WIN32
                                            toStdWString()
#else
                                            toStdString()
#endif
            ;

        if (std::error_code ec; !phonemeDict.load(path, &ec)) {
            qWarning() << "Failed to read dictionary " << path.string() << ":" << ec.value();

            return;
        }
        initialized = false;
    }

    QStringList Syllable2pPrivate::lookup(const QString &key) const {
        if (const auto it = phonemeMap.find(key); it != phonemeMap.end()) {
            return it->second;
        }

        if (const auto it = phonemeDict.find(key.toStdString().c_str()); it != phonemeDict.end()) {
            const auto &phonemes = it->second;
            QStringList tokens;
            for (const char *buf : phonemes) {
                tokens.push_back(QString::fromUtf8(buf));
            }
            phonemeMap[key] = tokens;
            return tokens;
        }
        return {};
    }

    Syllable2p::Syllable2p(QString dictPath)
        : Syllable2p(*new Syllable2pPrivate(std::move(dictPath))) {
    }

    Syllable2p::~Syllable2p() = default;

    QVector<QStringList> Syllable2p::syllableToPhoneme(const QStringList &syllables) const {
        QVector<QStringList> phonemeList;
        for (const QString &word : syllables) {
            QStringList res = d_ptr->lookup(word);
            phonemeList.push_back(res);
        }
        return phonemeList;
    }

    QStringList Syllable2p::syllableToPhoneme(const QString &syllable) const {
        return d_ptr->lookup(syllable);
    }

    Syllable2p::Syllable2p(Syllable2pPrivate &d) : d_ptr(&d) {
        d.q_ptr = this;

        d.init();
    }
}