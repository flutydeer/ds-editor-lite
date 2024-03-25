#include "MultiCharFactory.h"

namespace LangMgr {
    QList<LangNote> MultiCharFactory::split(const QString &input) const {
        QList<LangNote> result;

        int pos = 0;
        while (pos < input.length()) {
            const auto &currentChar = input[pos];
            LangNote note;
            if (contains(currentChar)) {
                const int start = pos;
                while (pos < input.length() && contains(input[pos])) {
                    pos++;
                }
                note.lyric = input.mid(start, pos - start);
                note.language = id();
                note.category = category();
            } else {
                const int start = pos;
                while (pos < input.length() && !contains(input[pos])) {
                    pos++;
                }
                note.lyric = input.mid(start, pos - start);
                note.language = "Unknown";
                note.category = "Unknown";
            }
            if (!note.lyric.isEmpty())
                result.append(note);
        }
        return result;
    }
} // LangMgr