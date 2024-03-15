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
    IOption(const IOption &other) : m_model(other.m_model), m_key(other.m_key) {
    }
    IOption &operator=(const IOption &other);
    virtual ~IOption() = default;

    virtual void load(const QJsonObject &object) = 0;
    [[nodiscard]] const QJsonObject &value() {
        serialize();
        return m_model;
    }
    [[nodiscard]] const QString &key() const {
        return m_key;
    }

    // signals:
    //     void settingsChanged();

protected:
    QJsonObject m_model;

    virtual void serialize() = 0;

private:
    QString m_key;
};
inline IOption &IOption::operator=(const IOption &other) {
    m_model = other.m_model;
    m_key = other.m_key;
    return *this;
}

#endif // IOPTION_H
