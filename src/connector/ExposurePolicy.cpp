#include "ExposurePolicy.h"

#include <QSet>

#include <utility>

namespace DsConnector {
    namespace {
        QJsonObject toolMetadata(const QJsonObject &tool) {
            return tool.value(QStringLiteral("_meta"))
                .toObject()
                .value(QStringLiteral("io.openvpi.ds-editor-lite/tool"))
                .toObject();
        }

        AutomationWire::ControlLevel controlLevelFromTool(const QJsonObject &tool) {
            const auto name = ExposurePolicy::minimumControlLevel(tool);
            const auto controlLevel = AutomationWire::controlLevelFromName(name);
            return controlLevel.value_or(AutomationWire::ControlLevel::L3);
        }
    }

    ExposurePolicy::ExposurePolicy(ConnectorOptions options)
        : m_options(std::move(options)),
          m_knownSelection(AutomationWire::selectExposure(m_options.exposure)) {
        for (const auto &tool : AutomationWire::publicToolContracts()) {
            if (AutomationWire::isExposed(m_knownSelection, tool.operationId))
                m_typedContracts.append(tool);
        }
    }

    const QList<AutomationWire::ToolContract> &ExposurePolicy::typedContracts() const {
        return m_typedContracts;
    }

    bool ExposurePolicy::allowsKnownTool(const AutomationWire::ToolContract &tool) const {
        return AutomationWire::isExposed(m_knownSelection, tool.operationId);
    }

    bool ExposurePolicy::allowsTarget(const QString &operationId, const QString &category,
                                      const QString &minimumControlLevel) const {
        QJsonObject tool{
            {QStringLiteral("operation_id"),    operationId   },
            {QStringLiteral("category"),        category      },
            {QStringLiteral("minimum_control_level"), minimumControlLevel},
        };
        const auto selection = selectionFor(QJsonArray{tool});
        return AutomationWire::isExposed(selection, operationId);
    }

    QJsonArray ExposurePolicy::filterActualTools(const QJsonArray &tools) const {
        const auto selection = selectionFor(tools);
        QJsonArray result;
        for (const auto &entry : tools) {
            const auto tool = entry.toObject();
            if (AutomationWire::isExposed(selection, operationId(tool)))
                result.append(tool);
        }
        return result;
    }

    QStringList ExposurePolicy::pendingSelectors(const QJsonArray &tools) const {
        return selectionFor(tools).pendingSelectors;
    }

    QString ExposurePolicy::operationId(const QJsonObject &tool) {
        auto value = tool.value(QStringLiteral("operation_id")).toString();
        if (value.isEmpty())
            value = tool.value(QStringLiteral("operationId")).toString();
        if (value.isEmpty())
            value = tool.value(QStringLiteral("name")).toString();
        return value;
    }

    QString ExposurePolicy::category(const QJsonObject &tool) {
        auto value = tool.value(QStringLiteral("category")).toString();
        if (value.isEmpty())
            value = toolMetadata(tool).value(QStringLiteral("category")).toString();
        if (value.isEmpty()) {
            value = tool.value(QStringLiteral("annotations"))
                        .toObject()
                        .value(QStringLiteral("category"))
                        .toString();
        }
        return value;
    }

    QString ExposurePolicy::minimumControlLevel(const QJsonObject &tool) {
        auto value = tool.value(QStringLiteral("minimum_control_level")).toString();
        if (value.isEmpty())
            value = tool.value(QStringLiteral("minimumControlLevel")).toString();
        if (value.isEmpty())
            value = toolMetadata(tool).value(QStringLiteral("minimum_control_level")).toString();
        if (value.isEmpty()) {
            value = tool.value(QStringLiteral("annotations"))
                        .toObject()
                        .value(QStringLiteral("minimumControlLevel"))
                        .toString();
        }
        return value.isEmpty() ? QStringLiteral("l3") : value;
    }

    QList<AutomationWire::ExposureTarget>
        ExposurePolicy::targetsFor(const QJsonArray &tools) const {
        QList<AutomationWire::ExposureTarget> result;
        QSet<QString> seen;
        for (const auto &entry : tools) {
            const auto tool = entry.toObject();
            const auto id = operationId(tool);
            if (id.isEmpty() || seen.contains(id))
                continue;
            seen.insert(id);
            result.append({id, category(tool), controlLevelFromTool(tool)});
        }
        for (const auto &target : AutomationWire::publicExposureTargets()) {
            if (!seen.contains(target.operationId))
                result.append(target);
        }
        return result;
    }

    AutomationWire::ExposureSelection ExposurePolicy::selectionFor(const QJsonArray &tools) const {
        return AutomationWire::selectExposure(m_options.exposure, targetsFor(tools));
    }

}
