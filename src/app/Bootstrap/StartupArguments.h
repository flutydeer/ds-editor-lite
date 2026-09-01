#ifndef STARTUPARGUMENTS_H
#define STARTUPARGUMENTS_H

#include "Model/AppOptions/Options/AutomationOption.h"

#include <QStringList>

#include <optional>

namespace StartupArguments {

    enum class ParseErrorCode {
        MissingValue,
        InvalidValue,
        ConflictingOptions,
        UnknownOption,
    };

    struct ParseError {
        ParseErrorCode code = ParseErrorCode::InvalidValue;
        QString option;
        QString message;

        friend bool operator==(const ParseError &, const ParseError &) = default;
    };

    struct AutomationOverrides {
        std::optional<bool> mcpEnabled;
        std::optional<quint16> controlPort;
        std::optional<AutomationOption::ControlLevel> controlLevel;

        [[nodiscard]] bool isEmpty() const;
    };

    struct ParsedArguments {
        QStringList projectFilePaths;
        AutomationOverrides automation;
        std::optional<ParseError> error;

        [[nodiscard]] bool isValid() const;
    };

    enum class ConfigSource {
        Persisted,
        CommandLine,
    };

    struct EffectiveAutomationConfig {
        bool mcpEnabled = false;
        quint16 controlPort = AutomationOption::kRandomControlPortMinimum;
        AutomationOption::ControlLevel controlLevel = AutomationOption::ControlLevel::L1;
        ConfigSource mcpEnabledSource = ConfigSource::Persisted;
        ConfigSource controlPortSource = ConfigSource::Persisted;
        ConfigSource controlLevelSource = ConfigSource::Persisted;
    };

    // Parses arguments after the executable name.
    [[nodiscard]] ParsedArguments parseArguments(const QStringList &arguments,
                                                 const QString &workingDirectory = {});
    [[nodiscard]] ParsedArguments parseApplicationArguments();
    [[nodiscard]] EffectiveAutomationConfig
        effectiveAutomationConfig(const AutomationOption &persisted,
                                  const AutomationOverrides &overrides);

    // Returns project paths passed on the command line as cleaned absolute paths.
    QStringList projectFilePaths();

} // namespace StartupArguments

#endif // STARTUPARGUMENTS_H
