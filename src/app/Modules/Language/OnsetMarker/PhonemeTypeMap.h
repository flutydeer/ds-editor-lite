#ifndef PHONEMETYPEMAP_H
#define PHONEMETYPEMAP_H

#include <QHash>
#include <QJsonObject>
#include <QString>

class PhonemeTypeMap {
public:
    bool load(const QJsonObject &obj);
    QString type(const QString &phonemeName) const;

private:
    QHash<QString, QString> m_map;
};

#endif // PHONEMETYPEMAP_H
