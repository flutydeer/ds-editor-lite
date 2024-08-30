//
// Created by fluty on 24-8-30.
//

#ifndef IMODULE_H
#define IMODULE_H

#include <QObject>

class IModule : public QObject {
    Q_OBJECT

public:
    enum StatusType { Ready, Loading, Error, Unknown };

    class Status {
    public:
        [[nodiscard]] StatusType type() const;
        [[nodiscard]] QString message() const;

    private:
        StatusType m_type = Unknown;
        QString m_message;
    };
};

#endif // IMODULE_H
