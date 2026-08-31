#ifndef AUTOMATIONWIRE_CONTROLLEVEL_H
#define AUTOMATIONWIRE_CONTROLLEVEL_H

#include <QString>
#include <QStringList>

#include <optional>

namespace AutomationWire {

    enum class ControlLevel {
        L0,
        L1,
        L2,
        L3,
        Custom,
    };

    QString controlLevelName(ControlLevel level);
    std::optional<ControlLevel> controlLevelFromName(const QString &name);
    QStringList controlLevelNames();

    // Custom membership is evaluated from its explicit operation set. This helper
    // only implements the cumulative preset relationship.
    bool presetIncludes(ControlLevel selected, ControlLevel minimum);

}

#endif // AUTOMATIONWIRE_CONTROLLEVEL_H
