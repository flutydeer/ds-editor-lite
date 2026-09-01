#include "ConnectorOptions.h"

namespace DsConnector {
    namespace {
        void appendUnique(QStringList &target, const QString &value) {
            if (!target.contains(value))
                target.append(value);
        }
    }

    bool parseConnectorOptions(const QStringList &arguments, ConnectorOptions &options,
                               QString &error) {
        error.clear();
        options = {};
        for (qsizetype index = 0; index < arguments.size(); ++index) {
            auto argument = arguments.at(index);
            QString value;
            const auto takeValue = [&](const QString &option) -> bool {
                const auto inlinePrefix = option + u'=';
                if (argument.startsWith(inlinePrefix)) {
                    value = argument.sliced(inlinePrefix.size());
                    return !value.isEmpty();
                }
                if (argument != option || index + 1 >= arguments.size())
                    return false;
                value = arguments.at(++index);
                return !value.isEmpty();
            };

            if (argument == QStringLiteral("--control-level") ||
                argument.startsWith(QStringLiteral("--control-level="))) {
                if (!takeValue(QStringLiteral("--control-level"))) {
                    error = QStringLiteral("--control-level requires a value");
                    return false;
                }
                const auto level = AutomationWire::exposureLevelFromName(value);
                if (!level) {
                    error = QStringLiteral("Invalid control level: %1").arg(value);
                    return false;
                }
                options.exposure.controlLevel = *level;
                continue;
            }

            QStringList *target = nullptr;
            QString option;
            if (argument == QStringLiteral("--include-tool") ||
                argument.startsWith(QStringLiteral("--include-tool="))) {
                target = &options.exposure.includes;
                option = QStringLiteral("--include-tool");
            } else if (argument == QStringLiteral("--exclude-tool") ||
                       argument.startsWith(QStringLiteral("--exclude-tool="))) {
                target = &options.exposure.excludes;
                option = QStringLiteral("--exclude-tool");
            }
            if (target) {
                if (!takeValue(option)) {
                    error = QStringLiteral("%1 requires a value").arg(option);
                    return false;
                }
                const auto selector = AutomationWire::parseExposureSelector(value);
                if (!selector.valid()) {
                    error = selector.error;
                    return false;
                }
                appendUnique(*target, selector.selector->normalized());
                continue;
            }

            error = QStringLiteral("Unknown connector argument: %1").arg(argument);
            return false;
        }

        const auto selection = AutomationWire::selectExposure(options.exposure);
        if (!selection.valid()) {
            error = selection.error;
            return false;
        }
        options.exposure.includes = selection.normalizedIncludes;
        options.exposure.excludes = selection.normalizedExcludes;
        return true;
    }

}
