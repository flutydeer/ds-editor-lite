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

    bool isEnglishWord(const QString &word) {
        // 遍历字符串中的每个字符
        for (const QChar &ch : word) {
            if (!ch.isLetter()) {
                return false;
            }
        }
        return true;
    }

    bool isNumber(const QString &word) {
        for (const QChar &ch : word) {
            if (!ch.isDigit()) {
                return false;
            }
        }
        return true;
    }

    TextType CleanLyric::lyricType(const QString &lyric, const QString &fermata) {
        if (lyric.size() > 1) {
            if (isEnglishWord(lyric)) {
                return TextType::EnWord;
            } else if (isNumber(lyric)) {
                return TextType::Number;
            }
        } else if (lyric.size() == 1) {
            QChar firstChar = lyric.at(0);
            if (isHanzi(firstChar)) {
                return TextType::Hanzi;
            } else if (firstChar.isDigit()) {
                return TextType::Digit;
            } else if (firstChar == fermata) {
                return TextType::Slur;
            } else if (isKana(firstChar)) {
                return TextType::Kana;
            } else if (firstChar.isLetter()) {
                return TextType::EnWord;
            }
        }
        return TextType::Other;
    }

    QPair<QList<QStringList>, QList<QList<TextType>>>
        CleanLyric::cleanLyric(const QString &input, const QString &fermata) {
        QList<QStringList> res;
        QList<QList<TextType>> label;

        QStringList currentLine;
        QList<TextType> currentLabel;

        int pos = 0;
        while (pos < input.length()) {
            QChar currentChar = input[pos];
            if (isLetter(currentChar)) {
                int start = pos;
                while (pos < input.length() && isLetter(input[pos])) {
                    pos++;
                }
                currentLabel.append(TextType::EnWord);
                currentLine.append(input.mid(start, pos - start));
            } else if (isHanzi(currentChar)) {
                currentLabel.append(TextType::Hanzi);
                currentLine.append(input.mid(pos, 1));
                pos++;
            } else if (currentChar.isDigit()) {
                currentLabel.append(TextType::Digit);
                currentLine.append(input.mid(pos, 1));
                pos++;
            } else if (currentChar == fermata) {
                currentLabel.append(TextType::Slur);
                currentLine.append(input.mid(pos, 1));
                pos++;
            } else if (isKana(currentChar)) {
                int length = (pos + 1 < input.length() && isSpecialKana(input[pos + 1])) ? 2 : 1;
                currentLabel.append(TextType::Kana);
                currentLine.append(input.mid(pos, length));
                pos += length;
            } else if (!currentChar.isSpace()) {
                currentLabel.append(TextType::Space);
                currentLine.append(input.mid(pos, 1));
                pos++;
            } else if (currentChar == QChar::LineFeed) {
                res.append(currentLine);
                label.append(currentLabel);

                currentLine.clear();
                currentLabel.clear();
                pos++;
            } else {
                currentLabel.append(TextType::Other);
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