#ifndef SYLLABLE2P_H
#define SYLLABLE2P_H

#include <memory>
#include <QObject>

namespace FillLyric {
    class Syllable2pPrivate;

    class Syllable2p : public QObject {
        Q_OBJECT
        Q_DECLARE_PRIVATE(Syllable2p)
    public:
        explicit Syllable2p(QString dictPath);
        ~Syllable2p() override;

        QStringList syllableToPhoneme(const QString &syllable) const;

        QVector<QStringList> syllableToPhoneme(const QStringList &syllables) const;

    protected:
        explicit Syllable2p(Syllable2pPrivate &d);

        QScopedPointer<Syllable2pPrivate> d_ptr;
    };
}


#endif // SYLLABLE2P_H