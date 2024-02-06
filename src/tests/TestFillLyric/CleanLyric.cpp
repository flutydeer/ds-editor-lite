#include "CleanLyric.h"

namespace FillLyric {
    bool CleanLyric::isLetter(QChar c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    bool CleanLyric::isHanzi(QChar c) {
        return (c >= QChar(0x4e00) && c <= QChar(0x9fa5));
    }

    bool CleanLyric::isKana(QChar c) {
        return ((c >= QChar(0x3040) && c <= QChar(0x309F)) ||
                (c >= QChar(0x30A0) && c <= QChar(0x30FF)));
    }

    bool CleanLyric::isSpecialKana(QChar c) {
        static QStringView specialKana = QStringLiteral("ャュョゃゅょァィゥェォぁぃぅぇぉ");
        return specialKana.contains(c);
    }

    QList<QStringList> CleanLyric::cleanLyric(const QString &input) {
        QList<QStringList> res;
        QStringList currentLine;
        int pos = 0;
        while (pos < input.length()) {
            QChar currentChar = input[pos];
            if (isLetter(currentChar)) {
                int start = pos;
                while (pos < input.length() && isLetter(input[pos])) {
                    pos++;
                }
                currentLine.append(input.mid(start, pos - start));
            } else if (isHanzi(currentChar) || currentChar.isDigit()) {
                currentLine.append(input.mid(pos, 1));
                pos++;
            } else if (isKana(currentChar)) {
                int length = (pos + 1 < input.length() && isSpecialKana(input[pos + 1])) ? 2 : 1;
                currentLine.append(input.mid(pos, length));
                pos += length;
            } else if (!currentChar.isSpace()) {
                currentLine.append(input.mid(pos, 1));
                pos++;
            } else if (currentChar == QChar::LineFeed) {
                res.append(currentLine);
                currentLine.clear();
                pos++;
            } else {
                pos++;
            }
        }
        if (!currentLine.isEmpty()) {
            res.append(currentLine);
        }
        return res;
    }
}