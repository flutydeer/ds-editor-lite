#ifndef AUTOMATIONWIRE_AUTOMATIONPROFILE_H
#define AUTOMATIONWIRE_AUTOMATIONPROFILE_H

#include <QString>
#include <QStringList>

#include <optional>

namespace AutomationWire {

    enum class AutomationProfile {
        Meta,
        L1,
        L2,
        L3,
        Custom,
    };

    QString automationProfileName(AutomationProfile profile);
    std::optional<AutomationProfile> automationProfileFromName(const QString &name);
    QStringList automationProfileNames();

    // Custom membership is evaluated from its explicit operation set. This helper
    // only implements the cumulative preset relationship.
    bool presetIncludes(AutomationProfile selected, AutomationProfile minimum);

}

#endif // AUTOMATIONWIRE_AUTOMATIONPROFILE_H
