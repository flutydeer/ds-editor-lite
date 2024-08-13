//
// Created by fluty on 24-8-14.
//

#ifndef PRONUNCIATION_H
#define PRONUNCIATION_H

#include <QString>

class QJsonObject;
class Pronunciation {
public:
    QString original;
    QString edited;

    Pronunciation() = default;
    Pronunciation(QString original, QString edited)
        : original(std::move(original)), edited(std::move(edited)){};

    [[nodiscard]] bool isEdited() const;

    friend QDataStream &operator<<(QDataStream &out, const Pronunciation &pronunciation);
    friend QDataStream &operator>>(QDataStream &in, Pronunciation &pronunciation);
    static QJsonObject serialize(const Pronunciation &pronunciation);
};


#endif // PRONUNCIATION_H
