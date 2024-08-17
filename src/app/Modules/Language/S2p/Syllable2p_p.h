#ifndef SYLLABLE2PPRIVATE_H
#define SYLLABLE2PPRIVATE_H

#include <unordered_map>

#include "Syllable2p.h"

namespace IKg2p {
    class Syllable2pPrivate {
    public:
        explicit Syllable2pPrivate(QString dictPath, QString dictName, const QChar &sep1 = '\t',
                                   QString sep2 = " ");

        ~Syllable2pPrivate();

        void init();

        bool initialized = false;

        Syllable2p *q_ptr{};

        std::unordered_map<QString, QStringList> phonemeMap;

    private:
        QString dictPath;
        QString dictName;
        QChar sep1;
        QString sep2;
    };
}
#endif // SYLLABLE2PPRIVATE_H