#include "ExposurePolicy.h"

#include <QSet>

#include <utility>

namespace DsConnector {
    namespace {
        AutomationWire::AutomationProfile profileFromTool(const QJsonObject &tool) {
            const auto name = ExposurePolicy::minimumProfile(tool);
            const auto profile = AutomationWire::automationProfileFromName(name);
            return profile.value_or(AutomationWire::AutomationProfile::L3);
        }
    }

    ExposurePolicy::ExposurePolicy(ConnectorOptions options) : m_options(std::move(options)) {
    }

    QList<AutomationWire::ToolContract> ExposurePolicy::typedContracts() const {
        const auto selection = AutomationWire::selectExposure(m_options.exposure);
        QList<AutomationWire::ToolContract> result;
        for (const auto &tool : AutomationWire::publicToolContracts()) {
            if (AutomationWire::isExposed(selection, tool.operationId))
                result.append(tool);
        }
        return result;
    }

    bool ExposurePolicy::allowsKnownTool(const AutomationWire::ToolContract &tool) const {
        const auto selection = AutomationWire::selectExposure(m_options.exposure);
        return AutomationWire::isExposed(selection, tool.operationId);
    }

    bool ExposurePolicy::allowsTarget(const QString &operationId, const QString &category,
                                      const QString &minimumProfile) const {
        QJsonObject tool{
            {QStringLiteral("operation_id"), operationId},
            {QStringLiteral("category"), category},
            {QStringLiteral("minimum_profile"), minimumProfile},
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
        if (value.isEmpty()) {
            value = tool.value(QStringLiteral("annotations"))
                        .toObject()
                        .value(QStringLiteral("category"))
                        .toString();
        }
        return value;
    }

    QString ExposurePolicy::minimumProfile(const QJsonObject &tool) {
        auto value = tool.value(QStringLiteral("minimum_profile")).toString();
        if (value.isEmpty())
            value = tool.value(QStringLiteral("minimumProfile")).toString();
        if (value.isEmpty()) {
            value = tool.value(QStringLiteral("annotations"))
                        .toObject()
                        .value(QStringLiteral("minimumProfile"))
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
            result.append({id, category(tool), profileFromTool(tool)});
        }
        for (const auto &target : AutomationWire::publicExposureTargets()) {
            if (!seen.contains(target.operationId))
                result.append(target);
        }
        return result;
    }

    AutomationWire::ExposureSelection
        ExposurePolicy::selectionFor(const QJsonArray &tools) const {
        return AutomationWire::selectExposure(m_options.exposure, targetsFor(tools));
    }

}
