#include "Modules/FillLyric/Utils/QssParser.h"

#include <lite/GUI/Theme/ThemeManager.h>

#include <QColor>
#include <QGraphicsView>

namespace FillLyric {
    namespace {
        // Theme tokens are injected into the qproperty string as ${token.name};
        // resolve them against the current theme first, then parse the color.
        QString resolveToken(const QString &component) {
            const QString trimmed = component.trimmed();
            if (trimmed.startsWith(QStringLiteral("${")) && trimmed.endsWith(QChar('}'))) {
                const QString token = trimmed.mid(2, trimmed.size() - 3);
                const auto color = ThemeManager::instance()->semanticColor(token);
                if (color.isValid())
                    return color.name(QColor::HexArgb);
            }
            return component;
        }

        QColor parseColorComponent(const QString &component) {
            const auto color = QColor(resolveToken(component).trimmed());
            if (color.isValid())
                return color;
            return {};
        }

        QColor parseColor(const QStringList &components) {
            if (components.size() >= 4)
                return {components[0].toInt(), components[1].toInt(), components[2].toInt(),
                        components[3].toInt()};
            return {};
        }
    } // namespace

    QString QssParser::propertyValue(const QGraphicsView *view, const QString &propertyName) {
        const auto list = view->property(propertyName.toUtf8()).toStringList();
        if (list.size() < 2)
            return {};
        return list[1];
    }

    QVector<QBrush> QssParser::parseBrushes(const QString &value, int count) {
        QVector<QBrush> result;
        if (value.isEmpty())
            return result;

        const auto brushList = value.split('|');
        if (brushList.size() != count)
            return result;

        for (int i = 0; i < count; i++) {
            const QString brush = brushList[i].trimmed();
            if (brush == "NoBrush") {
                result.append(QBrush(Qt::NoBrush));
            } else {
                // Single color literal / token, or legacy r,g,b,a components
                auto color = parseColorComponent(brush);
                if (!color.isValid() && brush.contains(','))
                    color = parseColor(brush.split(','));
                if (color.isValid())
                    result.append(QBrush(color));
            }
        }
        return result;
    }

    QVector<QPen> QssParser::parsePens(const QString &value, int count) {
        QVector<QPen> result;
        if (value.isEmpty())
            return result;

        const auto penListStr = value.split('|');
        if (penListStr.size() != count)
            return result;

        for (const auto &pen : penListStr) {
            const auto penValue = pen.split(',');
            QColor color;
            if (penValue.size() == 1) {
                // Single color literal / token
                color = parseColorComponent(penValue[0]);
            } else {
                color = parseColor(penValue);
            }
            if (!color.isValid())
                continue;
            if (penValue.size() == 5)
                result.append(QPen(color, penValue[4].toInt()));
            else if (penValue.size() == 2 && !penValue[0].contains('#'))
                result.append(QPen(color, penValue[1].toInt()));
            else
                result.append(QPen(color));
        }
        return result;
    }
} // namespace FillLyric
