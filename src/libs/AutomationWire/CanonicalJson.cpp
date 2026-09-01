#include "CanonicalJson.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>

namespace AutomationWire {
    namespace {
        bool appendCanonical(const QJsonValue &value, QByteArray &output, QString *errorMessage,
                             const int depth) {
            if (depth > 256) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("JSON nesting exceeds the canonicalization limit");
                return false;
            }

            switch (value.type()) {
                case QJsonValue::Null:
                    output.append("null");
                    return true;
                case QJsonValue::Bool:
                    output.append(value.toBool() ? "true" : "false");
                    return true;
                case QJsonValue::Double: {
                    const auto number = value.toDouble();
                    if (!std::isfinite(number)) {
                        if (errorMessage)
                            *errorMessage = QStringLiteral("Non-finite JSON numbers are not canonicalizable");
                        return false;
                    }
                    QJsonArray wrapper;
                    wrapper.append(number == 0.0 ? 0.0 : number);
                    const auto encoded = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
                    output.append(encoded.constData() + 1, encoded.size() - 2);
                    return true;
                }
                case QJsonValue::String: {
                    QJsonArray wrapper;
                    wrapper.append(value.toString());
                    const auto encoded = QJsonDocument(wrapper).toJson(QJsonDocument::Compact);
                    output.append(encoded.constData() + 1, encoded.size() - 2);
                    return true;
                }
                case QJsonValue::Array: {
                    output.append('[');
                    const auto array = value.toArray();
                    for (qsizetype index = 0; index < array.size(); ++index) {
                        if (index > 0)
                            output.append(',');
                        if (!appendCanonical(array.at(index), output, errorMessage, depth + 1))
                            return false;
                    }
                    output.append(']');
                    return true;
                }
                case QJsonValue::Object: {
                    output.append('{');
                    const auto object = value.toObject();
                    auto keys = object.keys();
                    std::sort(keys.begin(), keys.end());
                    for (qsizetype index = 0; index < keys.size(); ++index) {
                        if (index > 0)
                            output.append(',');
                        if (!appendCanonical(keys.at(index), output, errorMessage, depth + 1))
                            return false;
                        output.append(':');
                        if (!appendCanonical(object.value(keys.at(index)), output, errorMessage,
                                             depth + 1)) {
                            return false;
                        }
                    }
                    output.append('}');
                    return true;
                }
                case QJsonValue::Undefined:
                    if (errorMessage)
                        *errorMessage = QStringLiteral("Undefined is not a JSON value");
                    return false;
            }
            return false;
        }
    }

    QByteArray canonicalJson(const QJsonValue &value, QString *errorMessage) {
        if (errorMessage)
            errorMessage->clear();
        QByteArray output;
        if (!appendCanonical(value, output, errorMessage, 0))
            return {};
        return output;
    }

    QString sha256Digest(const QJsonValue &value, QString *errorMessage) {
        const auto canonical = canonicalJson(value, errorMessage);
        if (canonical.isEmpty())
            return {};
        return QStringLiteral("sha256:%1")
            .arg(QString::fromLatin1(
                QCryptographicHash::hash(canonical, QCryptographicHash::Sha256).toHex()));
    }

    bool canonicalJsonEqual(const QJsonValue &left, const QJsonValue &right) {
        QString leftError;
        QString rightError;
        const auto leftJson = canonicalJson(left, &leftError);
        const auto rightJson = canonicalJson(right, &rightError);
        return leftError.isEmpty() && rightError.isEmpty() && leftJson == rightJson;
    }

}
