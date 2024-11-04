//
// Created by fluty on 24-3-19.
//

#ifndef RUNLANGUAGEENGINETASK_H
#define RUNLANGUAGEENGINETASK_H

#include "Modules/Task/Task.h"

class LaunchLanguageEngineTask final : public Task {
    Q_OBJECT

public:
    explicit LaunchLanguageEngineTask(QObject *parent = nullptr);

    bool success = false;
    QString errorMessage;

protected:
    void runTask() override;
};



#endif // RUNLANGUAGEENGINETASK_H
