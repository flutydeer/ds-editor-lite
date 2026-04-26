#include "Modules/FillLyric/Utils/QssParser.h"

#include <QGraphicsView>

namespace FillLyric
{
    static QColor parseColor(const QStringList &components) {
        if (components.size() >= 4)
            return {components[0].toInt(), components[1].toInt(), components[2].toInt(), components[3].toInt()};
        return {};
    }

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
            if (brushList[i] == "NoBrush") {
                result.append(QBrush(Qt::NoBrush));
            } else {
                const auto color = parseColor(brushList[i].split(','));
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
            const auto color = parseColor(penValue);
            if (!color.isValid())
                continue;
            if (penValue.size() == 5)
                result.append(QPen(color, penValue[4].toInt()));
            else
                result.append(QPen(color));
        }
        return result;
    }
} // namespace FillLyric
