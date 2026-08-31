#include "ExposurePolicy.h"

#include "PublicValueDomains.h"
#include "PublicToolContract.h"

#include <QRegularExpression>

#include <algorithm>

namespace AutomationWire {
    namespace {
        bool containsInvalidUnicode(const QString &value) {
            for (qsizetype index = 0; index < value.size(); ++index) {
                const auto character = value.at(index);
                if (character.isHighSurrogate()) {
                    if (index + 1 >= value.size() || !value.at(index + 1).isLowSurrogate())
                        return true;
                    ++index;
                } else if (character.isLowSurrogate()) {
                    return true;
                }
            }
            return false;
        }

        bool presetContains(const ExposureLevel level, const ControlLevel minimum) {
            if (minimum == ControlLevel::L0)
                return true;
            if (level == ExposureLevel::L0)
                return false;
            if (minimum == ControlLevel::L1)
                return true;
            if (minimum == ControlLevel::L2)
                return level == ExposureLevel::L2 || level == ExposureLevel::L3;
            if (minimum == ControlLevel::L3)
                return level == ExposureLevel::L3;
            return false;
        }

        QStringList uniqueSelectors(const QStringList &selectors) {
            QStringList result;
            QSet<QString> seen;
            for (const auto &selector : selectors) {
                if (!seen.contains(selector)) {
                    seen.insert(selector);
                    result.append(selector);
                }
            }
            return result;
        }
    }

    QString exposureLevelName(const ExposureLevel level) {
        const auto names = publicStringValueDomainValues(PublicValueDomain::ExposureLevel);
        const auto index = static_cast<qsizetype>(level);
        return index >= 0 && index < names.size() ? names.at(index) : QString();
    }

    std::optional<ExposureLevel> exposureLevelFromName(const QString &name) {
        const auto names = publicStringValueDomainValues(PublicValueDomain::ExposureLevel);
        const auto index = names.indexOf(name);
        if (index < 0 || index > static_cast<qsizetype>(ExposureLevel::L3))
            return std::nullopt;
        return static_cast<ExposureLevel>(index);
    }

    QStringList exposureLevelNames() {
        return publicStringValueDomainValues(PublicValueDomain::ExposureLevel);
    }

    QString ExposureSelector::normalized() const {
        switch (kind) {
            case SelectorKind::Id:
                return QStringLiteral("id:%1").arg(value);
            case SelectorKind::Category:
                return QStringLiteral("category:%1").arg(value);
            case SelectorKind::Prefix:
                return QStringLiteral("prefix:%1").arg(value);
        }
        return {};
    }

    SelectorParseResult parseExposureSelector(const QString &text) {
        SelectorParseResult result;
        if (text.isEmpty() || text != text.trimmed() || containsInvalidUnicode(text)) {
            result.error = QStringLiteral("Exposure selector is empty or contains invalid whitespace");
            return result;
        }
        for (const auto character : text) {
            if (character.unicode() < 0x20 || character.unicode() == 0x7f) {
                result.error = QStringLiteral("Exposure selector contains a control character");
                return result;
            }
        }

        SelectorKind kind = SelectorKind::Id;
        auto value = text;
        const auto separator = text.indexOf(u':');
        if (separator >= 0) {
            const auto prefix = text.first(separator);
            value = text.sliced(separator + 1);
            if (prefix == QStringLiteral("id"))
                kind = SelectorKind::Id;
            else if (prefix == QStringLiteral("category"))
                kind = SelectorKind::Category;
            else if (prefix == QStringLiteral("prefix"))
                kind = SelectorKind::Prefix;
            else {
                result.error = QStringLiteral("Unknown exposure selector prefix: %1").arg(prefix);
                return result;
            }
        }

        static const QRegularExpression valueExpression(QStringLiteral("^[A-Za-z0-9_.-]+$"));
        if (value.isEmpty() || !valueExpression.match(value).hasMatch()) {
            result.error = QStringLiteral("Exposure selector value has invalid syntax");
            return result;
        }
        result.selector = ExposureSelector{kind, value};
        return result;
    }

    bool selectorMatches(const ExposureSelector &selector, const ExposureTarget &target) {
        switch (selector.kind) {
            case SelectorKind::Id:
                return target.operationId == selector.value;
            case SelectorKind::Category:
                return target.category == selector.value;
            case SelectorKind::Prefix:
                return target.operationId.startsWith(selector.value);
        }
        return false;
    }

    QList<ExposureTarget> publicExposureTargets() {
        QList<ExposureTarget> result;
        result.reserve(publicToolContracts().size());
        for (const auto &tool : publicToolContracts()) {
            result.append({tool.operationId, tool.category, tool.minimumControlLevel});
        }
        return result;
    }

    ExposureSelection selectExposure(const ExposureConfig &config,
                                     const QList<ExposureTarget> &targets) {
        ExposureSelection result;
        QList<ExposureSelector> includes;
        QList<ExposureSelector> excludes;

        const auto parseAll = [&](const QStringList &texts, QList<ExposureSelector> &parsed,
                                  QStringList &normalized) {
            for (const auto &text : texts) {
                const auto parseResult = parseExposureSelector(text);
                if (!parseResult.valid()) {
                    result.error = parseResult.error;
                    return false;
                }
                const auto canonical = parseResult.selector->normalized();
                if (!normalized.contains(canonical)) {
                    normalized.append(canonical);
                    parsed.append(*parseResult.selector);
                }
            }
            return true;
        };
        if (!parseAll(config.includes, includes, result.normalizedIncludes) ||
            !parseAll(config.excludes, excludes, result.normalizedExcludes)) {
            return result;
        }

        for (const auto &selector : includes) {
            if (std::none_of(targets.constBegin(), targets.constEnd(), [&](const auto &target) {
                    return selectorMatches(selector, target);
                })) {
                result.pendingSelectors.append(selector.normalized());
            }
        }
        for (const auto &selector : excludes) {
            if (std::none_of(targets.constBegin(), targets.constEnd(), [&](const auto &target) {
                    return selectorMatches(selector, target);
                })) {
                result.pendingSelectors.append(selector.normalized());
            }
        }
        result.pendingSelectors = uniqueSelectors(result.pendingSelectors);

        QSet<QString> seenTargetIds;
        for (const auto &target : targets) {
            if (target.operationId.isEmpty() || seenTargetIds.contains(target.operationId))
                continue;
            seenTargetIds.insert(target.operationId);
            const auto included = presetContains(config.controlLevel, target.minimumControlLevel) ||
                                  std::any_of(includes.constBegin(), includes.constEnd(),
                                              [&](const auto &selector) {
                                                  return selectorMatches(selector, target);
                                              });
            const auto excluded = std::any_of(excludes.constBegin(), excludes.constEnd(),
                                              [&](const auto &selector) {
                                                  return selectorMatches(selector, target);
                                              });
            if (included &&
                (target.minimumControlLevel == ControlLevel::L0 || !excluded)) {
                result.targets.append(target);
                result.exposedIds.insert(target.operationId);
            }
        }
        return result;
    }

    ExposureSelection selectExposure(const ExposureConfig &config) {
        return selectExposure(config, publicExposureTargets());
    }

    bool isExposed(const ExposureSelection &selection, const QString &operationId) {
        return selection.valid() && selection.exposedIds.contains(operationId);
    }

}
