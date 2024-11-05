#ifndef SYLLABLE2PPRIVATE_H
#define SYLLABLE2PPRIVATE_H

#include <unordered_map>

#include "Syllable2p.h"

#include "phonemedictionary.h"

namespace FillLyric {
    class Syllable2pPrivate {
    public:
        explicit Syllable2pPrivate(QString dictPath, QString dictName);
        ~Syllable2pPrivate();

        void init();

        bool initialized = false;

        Syllable2p *q_ptr{};

        QStringList lookup(const QString &key) const;

        dsutils::PhonemeDictionary fileMap;
        mutable std::unordered_map<QString, QStringList> phonemeMap;

    private:
        QString dictPath;
        QString dictName;
    };
}
#endif // SYLLABLE2PPRIVATE_H