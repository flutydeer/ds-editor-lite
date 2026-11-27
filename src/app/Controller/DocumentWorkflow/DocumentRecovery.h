#pragma once

#include "Automation/ProjectAutomationDtos.h"

class AppModel;

namespace DocumentRecovery {
    struct Record {
        QString file;
        QString path;
        QString name;
    };

    QList<Record> pending();
    bool write(const QString &key, const QString &path, const QString &name, const AppModel *model,
               const LoopSettings &loop, QString &error);
    bool read(const Record &record, Automation::DocumentDraftDto &draft, QString &error);
    void remove(const QString &key);
}
