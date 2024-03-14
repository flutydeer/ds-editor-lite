#include "KanaAnalysis.h"

namespace LangMgr {
    bool isKana(const QChar &c) {
        return ((c >= QChar(0x3040) && c <= QChar(0x309F)) ||
                (c >= QChar(0x30A0) && c <= QChar(0x30FF)));
    }

    bool isSpecialKana(const QChar &c) {
        static QStringView specialKana = QStringLiteral("ャュョゃゅょァィゥェォぁぃぅぇぉ");
        return specialKana.contains(c);
    }

    bool KanaAnalysis::contains(const QString &input) const {
        for (const QChar &ch : input) {
            if (!(isKana(ch) && !isSpecialKana(ch))) {
                return false;
            }
        }
        return true;
    }

    QList<LangNote> KanaAnalysis::split(const QString &input) const {
        QList<LangNote> results;

        int pos = 0;
        while (pos < input.length()) {
            const auto &currentChar = input[pos];
            LangNote note;
            if (isKana(currentChar)) {
                const int length =
                    (pos + 1 < input.length() && isSpecialKana(input[pos + 1])) ? 2 : 1;
                note.lyric = input.mid(pos, length);
                note.language = LangCommon::Language::Kana;
                pos += length;
            } else {
                const int start = pos;
                while (pos < input.length() && !contains(input[pos])) {
                    pos++;
                }
                note.lyric = input.mid(start, pos - start);
                note.language = LangCommon::Language::Unknown;
            }
            if (!note.lyric.isEmpty())
                results.append(note);
        }

        return results;
    }

} // LangMgr