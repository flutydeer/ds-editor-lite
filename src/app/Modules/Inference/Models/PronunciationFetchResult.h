#ifndef PRONUNCIATIONFETCHRESULT_H
#define PRONUNCIATIONFETCHRESULT_H

#include <QString>
#include <QStringList>

class PronunciationFetchResult {
public:
    QString pronunciation;
    QStringList candidates;
};

#endif // PRONUNCIATIONFETCHRESULT_H
