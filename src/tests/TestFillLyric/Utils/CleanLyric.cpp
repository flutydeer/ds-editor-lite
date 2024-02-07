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

    QPair<QList<QStringList>, QList<QList<int>>> CleanLyric::cleanLyric(const QString &input,
                                                                        const QString &fermata) {
        QList<QStringList> res;
        QList<QList<int>> label;

        QStringList currentLine;
        QList<int> currentLabel;

        int pos = 0;
        while (pos < input.length()) {
            QChar currentChar = input[pos];
            if (isLetter(currentChar)) {
                int start = pos;
                while (pos < input.length() && isLetter(input[pos])) {
                    pos++;
                }
                currentLabel.append(LyricType::Letter);
                currentLine.append(input.mid(start, pos - start));
            } else if (isHanzi(currentChar)) {
                currentLabel.append(LyricType::Hanzi);
                currentLine.append(input.mid(pos, 1));
                pos++;
            } else if (currentChar.isDigit()) {
                currentLabel.append(LyricType::Digit);
                currentLine.append(input.mid(pos, 1));
                pos++;
            } else if (currentChar == fermata) {
                currentLabel.append(LyricType::Fermata);
                currentLine.append(input.mid(pos, 1));
                pos++;
            } else if (isKana(currentChar)) {
                int length = (pos + 1 < input.length() && isSpecialKana(input[pos + 1])) ? 2 : 1;
                currentLabel.append(LyricType::Kana);
                currentLine.append(input.mid(pos, length));
                pos += length;
            } else if (!currentChar.isSpace()) {
                currentLabel.append(LyricType::Space);
                currentLine.append(input.mid(pos, 1));
                pos++;
            } else if (currentChar == QChar::LineFeed) {
                res.append(currentLine);
                label.append(currentLabel);

                currentLine.clear();
                currentLabel.clear();
                pos++;
            } else {
                currentLabel.append(LyricType::Other);
                pos++;
            }
        }
        if (!currentLine.isEmpty()) {
            res.append(currentLine);
            label.append(currentLabel);
        }
        return {res, label};
    }
}