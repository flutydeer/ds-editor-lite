//
// Created by fluty on 24-2-26.
//

#ifndef TESTTASK_H
#define TESTTASK_H

#include "ITask.h"

class TestTask : public ITask {
    Q_OBJECT
public:
    explicit TestTask(const QString &inputData, QObject *parent = nullptr);
    QString resultData();

protected:
    void runTask() override;

private:
    QString m_inputData;
    QString m_resultData;
};

#endif // TESTTASK_H
