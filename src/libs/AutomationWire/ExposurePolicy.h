#ifndef AUTOMATIONWIRE_EXPOSUREPOLICY_H
#define AUTOMATIONWIRE_EXPOSUREPOLICY_H

#include "AutomationProfile.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

namespace AutomationWire {

    enum class ExposureProfile {
        L0,
        L1,
        L2,
        L3,
    };

    QString exposureProfileName(ExposureProfile profile);
    std::optional<ExposureProfile> exposureProfileFromName(const QString &name);
    QStringList exposureProfileNames();

    enum class SelectorKind {
        Id,
        Category,
        Prefix,
    };

    struct ExposureSelector {
        SelectorKind kind = SelectorKind::Id;
        QString value;

        QString normalized() const;
    };

    struct SelectorParseResult {
        std::optional<ExposureSelector> selector;
        QString error;

        bool valid() const {
            return selector.has_value();
        }
    };

    struct ExposureTarget {
        QString operationId;
        QString category;
        AutomationProfile minimumProfile = AutomationProfile::L0;
    };

    struct ExposureConfig {
        ExposureProfile profile = ExposureProfile::L1;
        QStringList includes;
        QStringList excludes;
    };

    struct ExposureSelection {
        QList<ExposureTarget> targets;
        QSet<QString> exposedIds;
        QStringList normalizedIncludes;
        QStringList normalizedExcludes;
        QStringList pendingSelectors;
        QString error;

        bool valid() const {
            return error.isEmpty();
        }
    };

    SelectorParseResult parseExposureSelector(const QString &text);
    bool selectorMatches(const ExposureSelector &selector, const ExposureTarget &target);
    QList<ExposureTarget> publicExposureTargets();
    ExposureSelection selectExposure(const ExposureConfig &config,
                                     const QList<ExposureTarget> &targets);
    ExposureSelection selectExposure(const ExposureConfig &config);
    bool isExposed(const ExposureSelection &selection, const QString &operationId);

}

#endif // AUTOMATIONWIRE_EXPOSUREPOLICY_H
