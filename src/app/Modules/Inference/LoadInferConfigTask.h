//
// Created by fluty on 24-9-25.
//

#ifndef LOADINFERCONFIGTASK_H
#define LOADINFERCONFIGTASK_H

#include "Modules/Task/Task.h"

class LoadInferConfigTask : public Task {
    Q_OBJECT
public:
    explicit LoadInferConfigTask(const QString &path);
    bool success = false;

private:
    void runTask() override;
    QString m_path;
};



#endif // LOADINFERCONFIGTASK_H
