#include "Syllable2p.h"

#include <iostream>
#include <QFile>
#include <QDebug>

#include "Syllable2p_p.h"

#include <utility>


namespace FillLyric {
    Syllable2pPrivate::Syllable2pPrivate(QString dictPath, QString dictName, const QChar &sep1,
                                         QString sep2) : dictPath(std::move(dictPath)),
                                                         dictName(std::move(dictName)),
                                                         sep1(sep1), sep2(std::move(sep2)) {
    }

    Syllable2pPrivate::~Syllable2pPrivate() = default;

    static bool loadDict(const QString &dict_dir, const QString &fileName,
                         std::unordered_map<QString, QStringList> &resultMap,
                         const QChar &sep1,
                         const QString &sep2) {
        QFile file(dict_dir + "/" + fileName);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qDebug() << "Cannot open file " << fileName;
            return false;
        }

        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty())
                continue;
            QStringList parts = line.split(sep1);
            if (parts.size() < 2) {
                qDebug() << "Invalid line: " << line;
                continue;
            }
            QString key = parts[0];
            QString value = parts[1];
            value = value.replace(sep2, " ");
            const QStringList phonemes = value.split(" ");
            resultMap[key] = phonemes;
        }
        file.close();
        return true;
    }

    void Syllable2pPrivate::init() {
        initialized = loadDict(dictPath, dictName, phonemeMap, sep1, sep2);
    }

    Syllable2p::Syllable2p(QString dictPath, QString dictName, const QChar &sep1,
                           const QString &sep2) : Syllable2p(
        *new Syllable2pPrivate(std::move(dictPath), std::move(dictName), sep1, sep2)) {
    }

    Syllable2p::~Syllable2p() = default;

    QVector<QStringList> Syllable2p::syllableToPhoneme(const QStringList &syllables) const {
        QVector<QStringList> phonemeList;
        for (const QString &word : syllables) {
            QStringList res;
            if (d_ptr->phonemeMap.find(word) != d_ptr->phonemeMap.end())
                res = d_ptr->phonemeMap.find(word)->second;
            phonemeList.push_back(res);
        }
        return phonemeList;
    }

    QStringList Syllable2p::syllableToPhoneme(const QString &syllable) const {
        if (d_ptr->phonemeMap.find(syllable) != d_ptr->phonemeMap.end())
            return d_ptr->phonemeMap.find(syllable)->second;
        return {};
    }

    Syllable2p::Syllable2p(Syllable2pPrivate &d) : d_ptr(&d) {
        d.q_ptr = this;

        d.init();
    }
}