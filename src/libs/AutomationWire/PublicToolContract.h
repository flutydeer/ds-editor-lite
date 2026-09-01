#ifndef AUTOMATIONWIRE_PUBLICTOOLCONTRACT_H
#define AUTOMATIONWIRE_PUBLICTOOLCONTRACT_H

#include "ControlLevel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

namespace AutomationWire {

    inline constexpr quint64 PublicToolsetVersion = 1;

    enum class OperationKind {
        Query,
        Command,
    };

    enum class SyncMode {
        Synchronous,
        Asynchronous,
    };

    enum class FileAccess {
        None,
        Read,
        Write,
    };

    QString operationKindName(OperationKind kind);
    QString syncModeName(SyncMode mode);
    QString fileAccessName(FileAccess access);

    struct ToolContract {
        QString operationId;
        quint64 minimumToolsetVersion = 1;
        QString title;
        QString description;
        QString category;
        ControlLevel minimumControlLevel = ControlLevel::L0;
        OperationKind kind = OperationKind::Query;
        SyncMode syncMode = SyncMode::Synchronous;
        FileAccess fileAccess = FileAccess::None;
        QString hostAvailability = QStringLiteral("gui");
        QJsonObject inputSchema;
        QJsonObject outputSchema;
        QJsonArray valueSources;
        QJsonObject annotations;

        QJsonObject toMcpToolJson() const;
    };

    const QList<ToolContract> &publicToolContracts();
    const ToolContract *findPublicTool(const QString &operationId);
    QStringList publicToolIds();
    QList<ToolContract> toolsForControlLevel(ControlLevel controlLevel,
                                             const QSet<QString> &customEnabled = {});

}

#endif // AUTOMATIONWIRE_PUBLICTOOLCONTRACT_H
