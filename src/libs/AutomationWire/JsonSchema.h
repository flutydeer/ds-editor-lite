#ifndef AUTOMATIONWIRE_JSONSCHEMA_H
#define AUTOMATIONWIRE_JSONSCHEMA_H

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

namespace AutomationWire {

    inline constexpr auto JsonSchema202012 = "https://json-schema.org/draft/2020-12/schema";
    inline constexpr qint64 MaximumSafeJsonInteger = 9007199254740991LL;

    struct JsonResourceLimits {
        qint64 maximumStringCodeUnits = 16 * 1024 * 1024;
        qint64 maximumArrayItems = 1024 * 1024;
        qint64 maximumObjectProperties = 64 * 1024;
        qint64 maximumNodes = 2 * 1024 * 1024;
        int maximumDepth = 128;
    };

    enum class SchemaIssueCode {
        InvalidSchema,
        UnsupportedKeyword,
        UnresolvedReference,
        ValidationFailed,
        LimitExceeded,
    };

    struct SchemaIssue {
        SchemaIssueCode code = SchemaIssueCode::InvalidSchema;
        QString instancePath;
        QString schemaPath;
        QString message;
    };

    struct SchemaValidationResult {
        QList<SchemaIssue> issues;

        bool valid() const {
            return issues.isEmpty();
        }
    };

    class JsonSchema final {
    public:
        static QJsonObject document(QJsonObject root, const QJsonObject &definitions = {});
        static QJsonObject object(const QJsonObject &properties = {},
                                  const QStringList &required = {},
                                  bool additionalProperties = false);
        static QJsonObject objectWithAdditionalSchema(const QJsonObject &properties,
                                                      const QStringList &required,
                                                      const QJsonValue &additionalSchema);
        static QJsonObject array(const QJsonValue &items, std::optional<qint64> minimumItems = {},
                                 std::optional<qint64> maximumItems = {});
        static QJsonObject string(const QStringList &values = {},
                                  std::optional<qint64> minimumLength = {},
                                  std::optional<qint64> maximumLength = {});
        static QJsonObject integer(std::optional<double> minimum = {},
                                   std::optional<double> maximum = {});
        static QJsonObject number(std::optional<double> minimum = {},
                                  std::optional<double> maximum = {});
        static QJsonObject boolean();
        static QJsonObject null();
        static QJsonObject enumeration(const QJsonArray &values);
        static QJsonObject constant(const QJsonValue &value);
        static QJsonObject oneOf(const QJsonArray &schemas);
        static QJsonObject reference(const QString &pointer);
    };

    SchemaValidationResult checkJsonSchema(const QJsonValue &schema);
    SchemaValidationResult checkJsonResourceLimits(const QJsonValue &value,
                                                   const JsonResourceLimits &limits = {});
    SchemaValidationResult validateJsonValue(const QJsonValue &instance, const QJsonValue &schema);

}

#endif // AUTOMATIONWIRE_JSONSCHEMA_H
