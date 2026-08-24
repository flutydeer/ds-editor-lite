#ifndef AUTOMATIONWIRE_PUBLICTOOLCONTRACT_H
#define AUTOMATIONWIRE_PUBLICTOOLCONTRACT_H

#include "AutomationProfile.h"

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

    QString operationKindName(OperationKind kind);
    QString syncModeName(SyncMode mode);

    struct ToolContract {
        QString trackingId;
        QString operationId;
        quint64 version = 1;
        quint64 introducedVersion = 1;
        quint64 minimumCompatibleVersion = 1;
        QString title;
        QString description;
        QString category;
        AutomationProfile minimumProfile = AutomationProfile::Meta;
        OperationKind kind = OperationKind::Query;
        SyncMode syncMode = SyncMode::Synchronous;
        QJsonObject inputSchema;
        QJsonObject outputSchema;
        QJsonArray valueSources;
        QJsonObject annotations;

        QJsonObject toMcpToolJson() const;
        QJsonObject toManifestJson() const;
    };

    const QList<ToolContract> &publicToolContracts();
    const ToolContract *findPublicTool(const QString &operationId);
    QStringList publicToolIds();
    QList<ToolContract> toolsForProfile(AutomationProfile profile,
                                        const QSet<QString> &customEnabled = {});

    struct PublicManifest {
        quint64 toolsetVersion = PublicToolsetVersion;
        QString digest;
        AutomationProfile profile = AutomationProfile::L1;
        QString hostMode = QStringLiteral("gui");
        QList<ToolContract> operations;
        QJsonObject extensions;
        QString nextCursor;

        QJsonObject toJson() const;
    };

    PublicManifest buildPublicManifest(AutomationProfile profile,
                                       const QSet<QString> &customEnabled = {},
                                       const QString &hostMode = QStringLiteral("gui"),
                                       qsizetype offset = 0, qsizetype limit = 0);

}

#endif // AUTOMATIONWIRE_PUBLICTOOLCONTRACT_H
