#ifndef SYLLABLE2PPRIVATE_H
#define SYLLABLE2PPRIVATE_H

#include <unordered_map>

#include "Syllable2p.h"

#include "phonemedictionary.h"

namespace FillLyric {
    class Syllable2pPrivate final : public QObject {
        Q_OBJECT
        Q_DECLARE_PUBLIC(Syllable2p)
    public:
        explicit Syllable2pPrivate(QString dictPath);
        ~Syllable2pPrivate() override;

        void init();

        bool initialized = false;

        Syllable2p *q_ptr{};

        QStringList lookup(const QString &key) const;

        dsutils::PhonemeDictionary fileMap;
        mutable std::unordered_map<QString, QStringList> phonemeMap;

    private:
        QString dictPath;
    };
}
#endif // SYLLABLE2PPRIVATE_H