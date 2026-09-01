#include "ControlLevel.h"

#include "PublicValueDomains.h"

namespace AutomationWire {
    namespace {
        constexpr auto LastControlLevel = ControlLevel::Custom;
    }

    QString controlLevelName(const ControlLevel level) {
        const auto names = publicStringValueDomainValues(PublicValueDomain::ControlLevel);
        const auto index = static_cast<qsizetype>(level);
        return index >= 0 && index < names.size() ? names.at(index) : QString();
    }

    std::optional<ControlLevel> controlLevelFromName(const QString &name) {
        const auto names = publicStringValueDomainValues(PublicValueDomain::ControlLevel);
        const auto index = names.indexOf(name);
        if (index < 0 || index > static_cast<qsizetype>(LastControlLevel))
            return std::nullopt;
        return static_cast<ControlLevel>(index);
    }

    QStringList controlLevelNames() {
        return publicStringValueDomainValues(PublicValueDomain::ControlLevel);
    }

    bool presetIncludes(const ControlLevel selected, const ControlLevel minimum) {
        if (minimum == ControlLevel::L0)
            return true;
        if (selected == ControlLevel::Custom || minimum == ControlLevel::Custom)
            return false;
        return static_cast<int>(selected) >= static_cast<int>(minimum);
    }

}
