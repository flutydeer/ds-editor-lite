//
// Created by fluty on 24-3-13.
//

#ifndef IOPTION_H
#define IOPTION_H

#include <QJsonObject>

class IOption {
public:
    explicit IOption(QString key) : m_key(std::move(key)) {
    }

    virtual ~IOption() = default;

    virtual void load(const QJsonObject &object) = 0;

    [[nodiscard]] QJsonObject value() {
        QJsonObject object;
        save(object);
        return object;
    }

    [[nodiscard]] const QString &key() const {
        return m_key;
    }

    // signals:
    //     void settingsChanged();

    virtual void save(QJsonObject &object) = 0;

private:
    QString m_key;
};

#endif // IOPTION_H
