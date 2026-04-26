#ifndef LYRIC_TAB_UTILS_QSS_PARSER_H
#define LYRIC_TAB_UTILS_QSS_PARSER_H

#include <QBrush>
#include <QGraphicsView>
#include <QPen>
#include <QStringList>
#include <QVector>

namespace FillLyric
{
    namespace QssParser
    {
        QVector<QBrush> parseBrushes(const QString &value, int count);
        QVector<QPen> parsePens(const QString &value, int count);
        QString propertyValue(const QGraphicsView *view, const QString &propertyName);
    } // namespace QssParser
} // namespace FillLyric

#endif // LYRIC_TAB_UTILS_QSS_PARSER_H
