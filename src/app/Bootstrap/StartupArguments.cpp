#include "StartupArguments.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <utility>

namespace StartupArguments {

    namespace {

        ParsedArguments fail(ParsedArguments result, const ParseErrorCode code,
                             const QString &option, const QString &message) {
            result.error = ParseError{code, option, message};
            return result;
        }

        QString absoluteProjectPath(const QString &path, const QString &workingDirectory) {
            const QFileInfo info(path);
            if (info.isAbsolute())
                return QDir::cleanPath(info.absoluteFilePath());
            const QDir baseDirectory(workingDirectory.isEmpty() ? QDir::currentPath()
                                                                : workingDirectory);
            return QDir::cleanPath(baseDirectory.absoluteFilePath(path));
        }

    } // namespace

    bool AutomationOverrides::isEmpty() const {
        return !mcpEnabled && !controlPort && !controlLevel;
    }

    bool ParsedArguments::isValid() const {
        return !error.has_value();
    }

    ParsedArguments parseArguments(const QStringList &arguments, const QString &workingDirectory) {
        ParsedArguments result;
        result.projectFilePaths.reserve(arguments.size());
        bool positionalOnly = false;

        for (qsizetype index = 0; index < arguments.size(); ++index) {
            const auto argument = arguments.at(index);
            if (argument.isEmpty())
                continue;

            if (!positionalOnly && argument == QStringLiteral("--")) {
                positionalOnly = true;
                continue;
            }

            if (!positionalOnly && argument == QStringLiteral("--mcp")) {
                if (result.automation.mcpEnabled && !*result.automation.mcpEnabled) {
                    return fail(
                        std::move(result), ParseErrorCode::ConflictingOptions, argument,
                        QStringLiteral("Options --mcp and --no-mcp cannot be used together."));
                }
                result.automation.mcpEnabled = true;
                continue;
            }

            if (!positionalOnly && argument == QStringLiteral("--no-mcp")) {
                if (result.automation.mcpEnabled && *result.automation.mcpEnabled) {
                    return fail(
                        std::move(result), ParseErrorCode::ConflictingOptions, argument,
                        QStringLiteral("Options --mcp and --no-mcp cannot be used together."));
                }
                result.automation.mcpEnabled = false;
                continue;
            }

            const auto readValue = [&](const QString &option) -> std::optional<QString> {
                const auto attachedPrefix = option + QLatin1Char('=');
                if (argument.startsWith(attachedPrefix))
                    return argument.mid(attachedPrefix.size());
                if (argument != option)
                    return std::nullopt;
                if (index + 1 >= arguments.size() || arguments.at(index + 1).startsWith("--"))
                    return QString{};
                return arguments.at(++index);
            };

            if (!positionalOnly && (argument == QStringLiteral("--control-port") ||
                                    argument.startsWith(QStringLiteral("--control-port=")))) {
                const auto value = readValue(QStringLiteral("--control-port"));
                if (!value || value->isEmpty()) {
                    return fail(std::move(result), ParseErrorCode::MissingValue,
                                QStringLiteral("--control-port"),
                                QStringLiteral("Option --control-port requires a value."));
                }
                static const QRegularExpression decimalInteger(QStringLiteral("^[0-9]+$"));
                bool converted = false;
                const auto port = value->toUInt(&converted, 10);
                if (!converted || !decimalInteger.match(*value).hasMatch() || port < 1 ||
                    port > 65535) {
                    return fail(
                        std::move(result), ParseErrorCode::InvalidValue,
                        QStringLiteral("--control-port"),
                        QStringLiteral("Invalid value for --control-port: \"%1\"; expected an "
                                       "integer from 1 to 65535.")
                            .arg(*value));
                }
                const auto parsedPort = static_cast<quint16>(port);
                if (result.automation.controlPort && *result.automation.controlPort != parsedPort) {
                    return fail(
                        std::move(result), ParseErrorCode::ConflictingOptions,
                        QStringLiteral("--control-port"),
                        QStringLiteral("Conflicting values were provided for --control-port."));
                }
                result.automation.controlPort = parsedPort;
                continue;
            }

            if (!positionalOnly && (argument == QStringLiteral("--control-level") ||
                                    argument.startsWith(QStringLiteral("--control-level=")))) {
                const auto value = readValue(QStringLiteral("--control-level"));
                if (!value || value->isEmpty()) {
                    return fail(std::move(result), ParseErrorCode::MissingValue,
                                QStringLiteral("--control-level"),
                                QStringLiteral("Option --control-level requires a value."));
                }
                const auto level = AutomationOption::controlLevelFromString(*value);
                if (!level) {
                    return fail(std::move(result), ParseErrorCode::InvalidValue,
                                QStringLiteral("--control-level"),
                                QStringLiteral("Invalid value for --control-level: \"%1\"; "
                                               "expected l1, l2, l3, or custom.")
                                    .arg(*value));
                }
                if (result.automation.controlLevel && *result.automation.controlLevel != *level) {
                    return fail(std::move(result), ParseErrorCode::ConflictingOptions,
                                QStringLiteral("--control-level"),
                                QStringLiteral(
                                    "Conflicting values were provided for --control-level."));
                }
                result.automation.controlLevel = *level;
                continue;
            }

            if (!positionalOnly && argument.startsWith(QLatin1Char('-'))) {
                return fail(std::move(result), ParseErrorCode::UnknownOption, argument,
                            QStringLiteral("Unknown option: %1.").arg(argument));
            }

            result.projectFilePaths.append(absoluteProjectPath(argument, workingDirectory));
        }

        return result;
    }

    ParsedArguments parseApplicationArguments() {
        static const auto result = parseArguments(QCoreApplication::arguments().mid(1));
        return result;
    }

    EffectiveAutomationConfig effectiveAutomationConfig(const AutomationOption &persisted,
                                                        const AutomationOverrides &overrides) {
        EffectiveAutomationConfig result;
        result.mcpEnabled = persisted.mcpEnabled;
        result.controlPort = persisted.controlPort;
        result.controlLevel = persisted.controlLevel;

        if (overrides.mcpEnabled) {
            result.mcpEnabled = *overrides.mcpEnabled;
            result.mcpEnabledSource = ConfigSource::CommandLine;
        }
        if (overrides.controlPort) {
            result.controlPort = *overrides.controlPort;
            result.controlPortSource = ConfigSource::CommandLine;
        }
        if (overrides.controlLevel) {
            result.controlLevel = *overrides.controlLevel;
            result.controlLevelSource = ConfigSource::CommandLine;
        }
        return result;
    }

    QStringList projectFilePaths() {
        return parseApplicationArguments().projectFilePaths;
    }

} // namespace StartupArguments
