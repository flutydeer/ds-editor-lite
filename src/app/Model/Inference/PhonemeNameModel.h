//
// Created by fluty on 24-9-10.
//

#ifndef PHONEMENAMEMODEL_H
#define PHONEMENAMEMODEL_H

#include <QList>
#include <QString>

class PhonemeNameInput {
public:
    QString lyric;
    QString pronunciation;
};

class PhonemeNameResult {
public:
    QStringList aheadNames;
    QStringList normalNames;
    QStringList finalNames;
};



#endif // PHONEMENAMEMODEL_H
