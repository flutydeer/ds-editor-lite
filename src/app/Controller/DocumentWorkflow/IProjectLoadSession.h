#ifndef IPROJECTLOADSESSION_H
#define IPROJECTLOADSESSION_H

#include "ProjectLoadTypes.h"

#include <QObject>

class IProjectLoadSession : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
    ~IProjectLoadSession() override = default;

    virtual void start() = 0;
    virtual void cancel() = 0;
    virtual PreparedProject takeResult() = 0;
    [[nodiscard]] virtual quint64 requestId() const = 0;

signals:
    void progressChanged(const ProjectLoadProgress &progress);
    void ready();
    void failed(const ProjectOperationError &error);
    void canceled();
};

#endif // IPROJECTLOADSESSION_H
