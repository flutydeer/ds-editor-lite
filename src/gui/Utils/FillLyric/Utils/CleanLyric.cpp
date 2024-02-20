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

    QList<Phonic> CleanLyric::splitAuto(const QString &input, const QString &fermata) {
        QList<Phonic> phonics;

        int pos = 0;
        while (pos < input.length()) {
            QChar currentChar = input[pos];
            Phonic phonic;
            if (isLetter(currentChar)) {
                int start = pos;
                while (pos < input.length() && isLetter(input[pos])) {
                    pos++;
                }
                phonic.lyric = input.mid(start, pos - start);
                phonic.lyricType = TextType::EnWord;
            } else if (isHanzi(currentChar)) {
                phonic.lyric = input.mid(pos, 1);
                phonic.lyricType = TextType::Hanzi;
                pos++;
            } else if (currentChar.isDigit()) {
                phonic.lyric = input.mid(pos, 1);
                phonic.lyricType = TextType::Digit;
                pos++;
            } else if (currentChar == fermata) {
                phonic.lyric = input.mid(pos, 1);
                phonic.lyricType = TextType::Slur;
                pos++;
            } else if (isKana(currentChar)) {
                int length = (pos + 1 < input.length() && isSpecialKana(input[pos + 1])) ? 2 : 1;
                phonic.lyric = input.mid(pos, length);
                phonic.lyricType = TextType::Kana;
                pos += length;
            } else if (!currentChar.isSpace()) {
                phonic.lyric = input.mid(pos, 1);
                phonic.lyricType = TextType::Space;
                pos++;
            } else if (currentChar == QChar::LineFeed) {
                if (!phonics.isEmpty()) {
                    phonics.last().lineFeed = true;
                }
                pos++;
            } else {
                phonic.lyric = input.mid(pos, 1);
                phonic.lyricType = TextType::Other;
                pos++;
            }
            if (!phonic.lyric.isEmpty())
                phonics.append(phonic);
        }

        return phonics;
    }
}