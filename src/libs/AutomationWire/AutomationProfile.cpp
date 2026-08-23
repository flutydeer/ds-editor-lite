#include "AutomationProfile.h"

#include "PublicValueDomains.h"

namespace AutomationWire {
    namespace {
        constexpr auto LastPresetProfile = AutomationProfile::Custom;
    }

    QString automationProfileName(const AutomationProfile profile) {
        const auto names = publicStringValueDomainValues(PublicValueDomain::AutomationProfile);
        const auto index = static_cast<qsizetype>(profile);
        return index >= 0 && index < names.size() ? names.at(index) : QString();
    }

    std::optional<AutomationProfile> automationProfileFromName(const QString &name) {
        const auto names = publicStringValueDomainValues(PublicValueDomain::AutomationProfile);
        const auto index = names.indexOf(name);
        if (index < 0 || index > static_cast<qsizetype>(LastPresetProfile))
            return std::nullopt;
        return static_cast<AutomationProfile>(index);
    }

    QStringList automationProfileNames() {
        return publicStringValueDomainValues(PublicValueDomain::AutomationProfile);
    }

    bool presetIncludes(const AutomationProfile selected, const AutomationProfile minimum) {
        if (minimum == AutomationProfile::Meta)
            return true;
        if (selected == AutomationProfile::Custom || minimum == AutomationProfile::Custom)
            return false;
        return static_cast<int>(selected) >= static_cast<int>(minimum);
    }

}
