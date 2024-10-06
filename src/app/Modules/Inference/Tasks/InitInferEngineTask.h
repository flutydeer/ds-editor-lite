//
// Created by fluty on 24-9-25.
//

#ifndef INITINFERENGINETASK_H
#define INITINFERENGINETASK_H

#include "Modules/Task/Task.h"

class InitInferEngineTask : public Task {
    Q_OBJECT

public:
    explicit InitInferEngineTask(QObject *parent = nullptr);
    bool success = false;
    QString errorMessage;

private:
    void runTask() override;
};



#endif // INITINFERENGINETASK_H
