#ifndef PRONUNCIATIONTEXT_H
#define PRONUNCIATIONTEXT_H

#include <string>

#include <QString>
#include <QStringList>

/// Editor/language-layer text conversion helpers.
namespace PronunciationText {

    std::string toUtf8(const QString &value);

    QString fromUtf8(const std::string &value);

    /// Treat split pronunciation tokens as one candidate. Trim other candidates and drop empty
    /// ones.
    QStringList normalizeCandidates(const QString &pronunciation, QStringList candidates);

}

#endif // PRONUNCIATIONTEXT_H
