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
    /// 解析 colors.json。palette 只允许颜色字面量，tokens 可引用 palette 或其他 token。
    static std::optional<ThemeColorTable> parse(const QByteArray &data, QString *error = nullptr);

    /// 将 QSS 中的 ${token.name} 替换为 Qt 可识别的颜色；QSS 不允许直接引用 palette。
    static std::optional<QString> applyToStyleSheet(const QString &styleSheet,
                                                    const ThemeColorTable &colors,
                                                    QSet<QString> *usedTokens = nullptr,
                                                    QString *error = nullptr);

    /// 将主题颜色序列化为稳定的 QSS 表示（#RRGGBB / #AARRGGBB / transparent）。
    static QString toStyleSheetColor(const QColor &color);
};

#endif // THEMECOLORRESOLVER_H
