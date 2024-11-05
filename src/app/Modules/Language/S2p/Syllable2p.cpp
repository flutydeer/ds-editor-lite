#include "Syllable2p.h"

#include <iostream>
#include <type_traits>

#include <QFile>
#include <QDebug>
#include <QVarLengthArray>

#include "Syllable2p_p.h"

#include <utility>

namespace FillLyric {
    Syllable2pPrivate::Syllable2pPrivate(QString dictPath, QString dictName)
        : dictPath(std::move(dictPath)), dictName(std::move(dictName)) {
    }

    Syllable2pPrivate::~Syllable2pPrivate() = default;

    void Syllable2pPrivate::init() {
        const std::filesystem::path &path = (dictPath + "/" + dictName)
                                                .
#ifdef _WIN32
                                            toStdWString()
#else
                                            toStdString()
#endif
            ;
        initialized = fileMap.load(path);
    }

    QStringList Syllable2pPrivate::lookup(const QString &key) const {
        if (const auto it = phonemeMap.find(key); it != phonemeMap.end()) {
            return it->second;
        }
        auto &map = fileMap.get();
        if (const auto it = map.find(key.toStdString()); it != map.end()) {
            const auto &entry = it->second;
            QVarLengthArray<std::string_view> buf;
            buf.resize(entry.count);
            fileMap.readEntry(entry, buf.data(), entry.count);

            QStringList tokens;
            tokens.reserve(entry.count);
            for (int i = 0; i < entry.count; ++i) {
                tokens.push_back(QString::fromUtf8(buf[i]));
            }
            phonemeMap[key] = tokens;
            return tokens;
        }
        return {};
    }

    Syllable2p::Syllable2p(QString dictPath, QString dictName)
        : Syllable2p(*new Syllable2pPrivate(std::move(dictPath), std::move(dictName))) {
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