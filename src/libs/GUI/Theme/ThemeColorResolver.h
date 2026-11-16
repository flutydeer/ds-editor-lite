#ifndef THEMECOLORRESOLVER_H
#define THEMECOLORRESOLVER_H

#include <QColor>
#include <QHash>
#include <QSet>
#include <QString>

#include <optional>

struct ThemeColorTable {
    QHash<QString, QColor> palette;
    QHash<QString, QColor> tokens;
};

class ThemeColorResolver {
public:
    /// Parse colors.json. Palette entries are color literals. Tokens can reference palette
    /// entries or other tokens.
    static std::optional<ThemeColorTable> parse(const QByteArray &data, QString *error = nullptr);

    /// Replace QSS token references of the form ${token.name} with Qt color values.
    /// QSS references are limited to tokens.
    static std::optional<QString> applyToStyleSheet(const QString &styleSheet,
                                                    const ThemeColorTable &colors,
                                                    QSet<QString> *usedTokens = nullptr,
                                                    QString *error = nullptr);

    /// Format a color as #RRGGBB, #AARRGGBB, or transparent for QSS.
    static QString toStyleSheetColor(const QColor &color);
};

#endif // THEMECOLORRESOLVER_H
