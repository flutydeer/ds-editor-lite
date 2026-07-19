#include "ThemeColorResolver.h"

#include "UI/Utils/ColorUtils.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

#include <cmath>
#include <functional>

namespace {

    void setError(QString *error, const QString &message);

    class DuplicateKeyScanner {
    public:
        explicit DuplicateKeyScanner(const QByteArray &data) : m_data(data) {
        }

        bool scan(QString *error) {
            m_error = error;
            m_offset = 0;
            skipWhitespace();
            return parseValue();
        }

    private:
        void skipWhitespace() {
            while (m_offset < m_data.size() &&
                   (m_data.at(m_offset) == ' ' || m_data.at(m_offset) == '\t' ||
                    m_data.at(m_offset) == '\r' || m_data.at(m_offset) == '\n')) {
                ++m_offset;
            }
        }

        bool parseValue() {
            skipWhitespace();
            if (m_offset >= m_data.size())
                return false;
            if (m_data.at(m_offset) == '{')
                return parseObject();
            if (m_data.at(m_offset) == '[')
                return parseArray();
            if (m_data.at(m_offset) == '"') {
                QString ignored;
                return parseString(ignored);
            }

            while (m_offset < m_data.size() && m_data.at(m_offset) != ',' &&
                   m_data.at(m_offset) != ']' && m_data.at(m_offset) != '}') {
                ++m_offset;
            }
            return true;
        }

        bool parseObject() {
            ++m_offset;
            skipWhitespace();
            QSet<QString> keys;
            if (m_offset < m_data.size() && m_data.at(m_offset) == '}') {
                ++m_offset;
                return true;
            }

            while (m_offset < m_data.size()) {
                QString key;
                if (!parseString(key))
                    return false;
                if (keys.contains(key)) {
                    setError(m_error, QStringLiteral("Duplicate JSON key '%1'").arg(key));
                    return false;
                }
                keys.insert(key);

                skipWhitespace();
                if (m_offset >= m_data.size() || m_data.at(m_offset) != ':')
                    return false;
                ++m_offset;
                if (!parseValue())
                    return false;
                skipWhitespace();
                if (m_offset < m_data.size() && m_data.at(m_offset) == '}') {
                    ++m_offset;
                    return true;
                }
                if (m_offset >= m_data.size() || m_data.at(m_offset) != ',')
                    return false;
                ++m_offset;
                skipWhitespace();
            }
            return false;
        }

        bool parseArray() {
            ++m_offset;
            skipWhitespace();
            if (m_offset < m_data.size() && m_data.at(m_offset) == ']') {
                ++m_offset;
                return true;
            }
            while (m_offset < m_data.size()) {
                if (!parseValue())
                    return false;
                skipWhitespace();
                if (m_offset < m_data.size() && m_data.at(m_offset) == ']') {
                    ++m_offset;
                    return true;
                }
                if (m_offset >= m_data.size() || m_data.at(m_offset) != ',')
                    return false;
                ++m_offset;
            }
            return false;
        }

        bool parseString(QString &value) {
            skipWhitespace();
            if (m_offset >= m_data.size() || m_data.at(m_offset) != '"')
                return false;
            const qsizetype start = m_offset++;
            bool escaped = false;
            while (m_offset < m_data.size()) {
                const char character = m_data.at(m_offset++);
                if (escaped) {
                    escaped = false;
                    continue;
                }
                if (character == '\\') {
                    escaped = true;
                    continue;
                }
                if (character != '"')
                    continue;

                const QByteArray quoted = m_data.mid(start, m_offset - start);
                const auto keyDocument =
                    QJsonDocument::fromJson(QByteArray("[") + quoted + QByteArray("]"));
                value = keyDocument.array().first().toString();
                return true;
            }
            return false;
        }

        const QByteArray &m_data;
        qsizetype m_offset = 0;
        QString *m_error = nullptr;
    };

    const QRegularExpression &colorKeyPattern() {
        static const QRegularExpression pattern(
            QStringLiteral(R"(^[a-z][A-Za-z0-9]*(?:\.[A-Za-z0-9][A-Za-z0-9]*)+$)"));
        return pattern;
    }

    const QRegularExpression &aliasPattern() {
        static const QRegularExpression pattern(QStringLiteral(R"(^\{([^{}]+)\}$)"));
        return pattern;
    }

    const QRegularExpression &hexPattern() {
        static const QRegularExpression pattern(
            QStringLiteral(R"(^#(?:[0-9A-Fa-f]{3}|[0-9A-Fa-f]{6}|[0-9A-Fa-f]{8})$)"));
        return pattern;
    }

    const QRegularExpression &oklchPattern() {
        static const QString number = QStringLiteral(R"([+-]?(?:\d+(?:\.\d*)?|\.\d+))");
        static const QRegularExpression pattern(
            QStringLiteral(R"(^oklch\(\s*(%1)(%?)\s+(%1)\s+(%1)(?:deg)?(?:\s*/\s*(%1)(%?))?\s*\)$)")
                .arg(number),
            QRegularExpression::CaseInsensitiveOption);
        return pattern;
    }

    void setError(QString *error, const QString &message) {
        if (error)
            *error = message;
    }

    bool validateColorKey(const QString &key, const QString &section, QString *error) {
        if (colorKeyPattern().match(key).hasMatch())
            return true;
        setError(error, QStringLiteral("Invalid color key '%1' in %2").arg(key, section));
        return false;
    }

    std::optional<QColor> parseColorLiteral(const QString &source, QString *error) {
        const QString value = source.trimmed();
        if (value.compare(QStringLiteral("transparent"), Qt::CaseInsensitive) == 0)
            return QColor(Qt::transparent);

        if (hexPattern().match(value).hasMatch()) {
            const QColor color(value);
            if (color.isValid())
                return color;
        }

        const auto match = oklchPattern().match(value);
        if (match.hasMatch()) {
            bool lightnessOk = false;
            bool chromaOk = false;
            bool hueOk = false;
            bool alphaOk = true;
            double lightness = match.captured(1).toDouble(&lightnessOk);
            const double chroma = match.captured(3).toDouble(&chromaOk);
            double hue = match.captured(4).toDouble(&hueOk);
            double alpha = 1.0;

            if (match.captured(2) == QStringLiteral("%"))
                lightness /= 100.0;
            if (!match.captured(5).isEmpty()) {
                alpha = match.captured(5).toDouble(&alphaOk);
                if (match.captured(6) == QStringLiteral("%"))
                    alpha /= 100.0;
            }

            if (!lightnessOk || !chromaOk || !hueOk || !alphaOk || lightness < 0.0 ||
                lightness > 1.0 || chroma < 0.0 || alpha < 0.0 || alpha > 1.0) {
                setError(error, QStringLiteral("OKLCH component out of range: '%1'").arg(source));
                return std::nullopt;
            }

            hue = std::fmod(hue, 360.0);
            if (hue < 0.0)
                hue += 360.0;
            QColor color = ColorUtils::oklchToSRGB({lightness, chroma, hue});
            color.setAlphaF(alpha);
            return color;
        }

        setError(error, QStringLiteral("Unsupported color literal '%1'").arg(source));
        return std::nullopt;
    }

}

std::optional<ThemeColorTable> ThemeColorResolver::parse(const QByteArray &data, QString *error) {
    if (error)
        error->clear();

    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("colors.json is not a valid JSON object: %1 at offset %2")
                            .arg(parseError.errorString())
                            .arg(parseError.offset));
        return std::nullopt;
    }

    DuplicateKeyScanner duplicateKeyScanner(data);
    if (!duplicateKeyScanner.scan(error)) {
        if (error && error->isEmpty())
            *error = QStringLiteral("Failed to validate JSON object keys");
        return std::nullopt;
    }

    const auto root = document.object();
    if (!root.value(QStringLiteral("palette")).isObject() ||
        !root.value(QStringLiteral("tokens")).isObject()) {
        setError(error,
                 QStringLiteral("colors.json requires object fields 'palette' and 'tokens'"));
        return std::nullopt;
    }

    const auto paletteObject = root.value(QStringLiteral("palette")).toObject();
    const auto tokenObject = root.value(QStringLiteral("tokens")).toObject();
    if (paletteObject.isEmpty() || tokenObject.isEmpty()) {
        setError(error, QStringLiteral("colors.json palette and tokens must not be empty"));
        return std::nullopt;
    }

    ThemeColorTable result;
    for (auto it = paletteObject.constBegin(); it != paletteObject.constEnd(); ++it) {
        if (!validateColorKey(it.key(), QStringLiteral("palette"), error))
            return std::nullopt;
        if (tokenObject.contains(it.key())) {
            setError(error, QStringLiteral("Color key '%1' is declared in both palette and tokens")
                                .arg(it.key()));
            return std::nullopt;
        }
        if (!it.value().isString()) {
            setError(error,
                     QStringLiteral("Palette value '%1' must be a color string").arg(it.key()));
            return std::nullopt;
        }

        QString literalError;
        const auto color = parseColorLiteral(it.value().toString(), &literalError);
        if (!color) {
            setError(error,
                     QStringLiteral("Invalid palette value '%1': %2").arg(it.key(), literalError));
            return std::nullopt;
        }
        result.palette.insert(it.key(), *color);
    }

    QHash<QString, QString> rawTokens;
    for (auto it = tokenObject.constBegin(); it != tokenObject.constEnd(); ++it) {
        if (!validateColorKey(it.key(), QStringLiteral("tokens"), error))
            return std::nullopt;
        if (!it.value().isString()) {
            setError(
                error,
                QStringLiteral("Token value '%1' must be a color string or alias").arg(it.key()));
            return std::nullopt;
        }
        rawTokens.insert(it.key(), it.value().toString().trimmed());
    }

    enum class ResolveState {
        Resolving,
        Resolved,
    };
    QHash<QString, ResolveState> states;
    QStringList resolutionStack;
    std::function<std::optional<QColor>(const QString &)> resolveToken;
    resolveToken = [&](const QString &key) -> std::optional<QColor> {
        const auto state = states.constFind(key);
        if (state != states.constEnd()) {
            if (*state == ResolveState::Resolved)
                return result.tokens.value(key);

            const qsizetype cycleStart = resolutionStack.indexOf(key);
            QStringList cycle = resolutionStack.mid(cycleStart);
            cycle.append(key);
            setError(error, QStringLiteral("Color token alias cycle: %1").arg(cycle.join(" -> ")));
            return std::nullopt;
        }

        states.insert(key, ResolveState::Resolving);
        resolutionStack.append(key);
        const QString rawValue = rawTokens.value(key);
        const auto aliasMatch = aliasPattern().match(rawValue);
        std::optional<QColor> color;
        if (aliasMatch.hasMatch()) {
            const QString target = aliasMatch.captured(1);
            if (result.palette.contains(target)) {
                color = result.palette.value(target);
            } else if (rawTokens.contains(target)) {
                color = resolveToken(target);
            } else {
                setError(error, QStringLiteral("Unknown color alias '%1' referenced by token '%2'")
                                    .arg(target, key));
            }
        } else {
            QString literalError;
            color = parseColorLiteral(rawValue, &literalError);
            if (!color)
                setError(error,
                         QStringLiteral("Invalid token value '%1': %2").arg(key, literalError));
        }

        resolutionStack.removeLast();
        if (!color)
            return std::nullopt;
        result.tokens.insert(key, *color);
        states.insert(key, ResolveState::Resolved);
        return color;
    };

    for (auto it = rawTokens.constBegin(); it != rawTokens.constEnd(); ++it) {
        if (!resolveToken(it.key()))
            return std::nullopt;
    }
    return result;
}

std::optional<QString> ThemeColorResolver::applyToStyleSheet(const QString &styleSheet,
                                                             const ThemeColorTable &colors,
                                                             QSet<QString> *usedTokens,
                                                             QString *error) {
    if (error)
        error->clear();

    static const QRegularExpression placeholderPattern(QStringLiteral(R"(\$\{([^{}]+)\})"));
    QString result;
    result.reserve(styleSheet.size());
    qsizetype sourceOffset = 0;
    auto iterator = placeholderPattern.globalMatch(styleSheet);
    while (iterator.hasNext()) {
        const auto match = iterator.next();
        result += styleSheet.mid(sourceOffset, match.capturedStart() - sourceOffset);
        const QString key = match.captured(1);
        if (!colorKeyPattern().match(key).hasMatch()) {
            setError(error, QStringLiteral("Malformed QSS color placeholder '${%1}'").arg(key));
            return std::nullopt;
        }
        if (colors.palette.contains(key)) {
            setError(
                error,
                QStringLiteral(
                    "QSS placeholder '${%1}' references palette directly; use a semantic token")
                    .arg(key));
            return std::nullopt;
        }
        if (!colors.tokens.contains(key)) {
            setError(error, QStringLiteral("Unknown QSS color token '${%1}'").arg(key));
            return std::nullopt;
        }

        result += toStyleSheetColor(colors.tokens.value(key));
        sourceOffset = match.capturedEnd();
        if (usedTokens)
            usedTokens->insert(key);
    }
    result += styleSheet.mid(sourceOffset);

    if (result.contains(QStringLiteral("${"))) {
        setError(error, QStringLiteral("Malformed or unresolved QSS color placeholder"));
        return std::nullopt;
    }
    return result;
}

QString ThemeColorResolver::toStyleSheetColor(const QColor &color) {
    if (color.alpha() == 0)
        return QStringLiteral("transparent");
    if (color.alpha() == 255)
        return color.name(QColor::HexRgb).toUpper();
    return color.name(QColor::HexArgb).toUpper();
}
