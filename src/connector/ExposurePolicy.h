#ifndef DSCONNECTOR_EXPOSUREPOLICY_H
#define DSCONNECTOR_EXPOSUREPOLICY_H

#include "ConnectorOptions.h"

#include <lite/AutomationWire/PublicToolContract.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QStringList>

namespace DsConnector {

    class ExposurePolicy {
    public:
        explicit ExposurePolicy(ConnectorOptions options);

        const QList<AutomationWire::ToolContract> &typedContracts() const;
        bool allowsKnownTool(const AutomationWire::ToolContract &tool) const;
        bool allowsTarget(const QString &operationId, const QString &category,
                          const QString &minimumControlLevel) const;
        QJsonArray filterActualTools(const QJsonArray &tools) const;
        QStringList pendingSelectors(const QJsonArray &tools) const;

        static QString operationId(const QJsonObject &tool);
        static QString category(const QJsonObject &tool);
        static QString minimumControlLevel(const QJsonObject &tool);

    private:
        QList<AutomationWire::ExposureTarget> targetsFor(const QJsonArray &tools) const;
        AutomationWire::ExposureSelection selectionFor(const QJsonArray &tools) const;

        ConnectorOptions m_options;
        AutomationWire::ExposureSelection m_knownSelection;
        QList<AutomationWire::ToolContract> m_typedContracts;
    };

}

#endif // DSCONNECTOR_EXPOSUREPOLICY_H
