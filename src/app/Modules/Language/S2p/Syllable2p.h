#ifndef SYLLABLE2P_H
#define SYLLABLE2P_H

#include <memory>
#include <QString>

namespace IKg2p {
    class Syllable2pPrivate;

    class Syllable2p {
    public:
        explicit Syllable2p(QString dictPath, QString dictName, const QChar &sep1 = '\t',
                            const QString &sep2 = " ");

        ~Syllable2p();

        QStringList syllableToPhoneme(const QString &syllable) const;

        QVector<QStringList> syllableToPhoneme(const QStringList &syllables) const;

    protected:
        explicit Syllable2p(Syllable2pPrivate &d);

        std::unique_ptr<Syllable2pPrivate> d_ptr;
    };
}


#endif // SYLLABLE2P_H