#include "CleanLyric.h"
#include "Modules/Language/LangMgr/ILanguageManager.h"

namespace FillLyric {
    bool CleanLyric::isLetter(const QChar &c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    bool CleanLyric::isHanzi(const QChar &c) {
        return (c >= QChar(0x4e00) && c <= QChar(0x9fa5));
    }

    bool CleanLyric::isKana(const QChar &c) {
        return ((c >= QChar(0x3040) && c <= QChar(0x309F)) ||
                (c >= QChar(0x30A0) && c <= QChar(0x30FF)));
    }

    bool CleanLyric::isSpecialKana(const QChar &c) {
        static QStringView specialKana = QStringLiteral("ャュョゃゅょァィゥェォぁぃぅぇぉ");
        return specialKana.contains(c);
    }

    bool CleanLyric::isLineBreak(const QChar &c) {
        return c == QChar::LineFeed || c == QChar::LineSeparator || c == QChar::ParagraphSeparator;
    }

    bool CleanLyric::isEnglishWord(const QString &word) {
        for (const QChar &ch : word) {
            if (!isLetter(ch)) {
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


    QString CleanLyric::lyricType(const QString &lyric, const QString &fermata) {
        if (lyric.size() > 1) {
            if (isEnglishWord(lyric)) {
                return "English";
            }
            if (isNumber(lyric)) {
                return "Number";
            }
        } else if (lyric.size() == 1) {
            const QChar firstChar = lyric.at(0);
            if (isHanzi(firstChar)) {
                return "Mandarin";
            }
            if (firstChar.isDigit()) {
                return "Number";
            }
            if (firstChar == fermata) {
                return "Slur";
            }
            if (isKana(firstChar)) {
                return "Kana";
            }
            if (firstChar.isLetter()) {
                return "English";
            }
            if (firstChar == ' ') {
                return "Space";
            }
        }
        return "Unknown";
    }

    QList<Phonic> CleanLyric::splitAuto(const QString &input, const bool &excludeSpace,
                                        const QString &fermata) {
        QList<Phonic> phonics;
        const auto langMgr = LangMgr::ILanguageManager::instance();
        const auto res = langMgr->split(input);

        for (const auto &note : res) {
            Phonic phonic;
            phonic.lyric = note.lyric;
            phonic.language = note.language;
            if (note.language == "Linebreak")
                phonic.lineFeed = true;
            phonics.append(phonic);
        }

        return phonics;
    }

    QList<Phonic> CleanLyric::splitByChar(const QString &input, const bool &excludeSpace,
                                          const QString &fermata) {
        QList<Phonic> phonics;
        for (int i = 0; i < input.length(); i++) {
            const QChar &currentChar = input[i];
            if (excludeSpace && currentChar == ' ') {
                continue;
            }
            if (isLineBreak(currentChar)) {
                if (!phonics.isEmpty()) {
                    phonics.last().lineFeed = true;
                }
                continue;
            }
            Phonic phonic;
            phonic.lyric = currentChar;
            phonic.language = lyricType(phonic.lyric, fermata);
            phonics.append(phonic);
        }
        return phonics;
    }

    QList<Phonic> CleanLyric::splitCustom(const QString &input, const QStringList &splitter,
                                          const bool &excludeSpace, const QString &fermata) {
        QList<Phonic> phonics;
        int pos = 0;
        while (pos < input.length()) {
            const int start = pos;
            while (pos < input.length() && !splitter.contains(input[pos]) &&
                   !(excludeSpace && input[pos] == ' ') && !isLineBreak(input[pos])) {
                pos++;
            }

            const auto lyric = input.mid(start, pos - start);
            if (!lyric.isEmpty() && !splitter.contains(lyric) && !(excludeSpace && lyric == ' ')) {
                Phonic phonic;
                phonic.lyric = lyric;
                phonic.language = lyricType(phonic.lyric, fermata);
                phonics.append(phonic);
            }

            if (isLineBreak(input[pos])) {
                if (!phonics.isEmpty()) {
                    phonics.last().lineFeed = true;
                }
            }
            pos++;
        }
        return phonics;
    }
}