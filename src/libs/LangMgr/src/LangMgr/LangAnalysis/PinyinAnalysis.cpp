#include "PinyinAnalysis.h"

#include <QRandomGenerator>

namespace LangMgr {
    bool PinyinAnalysis::initialize(QString &errMsg) {
        loadDict();
        return true;
    }

    void PinyinAnalysis::loadDict() {
        QStringList initials = {"b", "p", "m",  "f",  "d",  "t", "n", "l", "g", "k", "h", "j",
                                "q", "x", "zh", "ch", "sh", "r", "z", "c", "s", "y", "w"};
        QStringList finals = {"a",  "o",  "e",  "i",  "u",   "v",   "ai",  "ei",
                              "ui", "ao", "ou", "iu", "ie",  "ve",  "er",  "an",
                              "en", "in", "un", "vn", "ang", "eng", "ing", "ong"};

        for (const auto &initial : initials) {
            for (const auto &final : finals) {
                pinyinSet.insert(initial + final);
            }
        }

        for (const auto &initial : initials) {
            pinyinSet.insert(initial);
        }

        for (const auto &final : finals) {
            pinyinSet.insert(final);
        }
    }

    static bool isLetter(QChar c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '\'';
    }

    bool PinyinAnalysis::contains(const QString &input) const {
        return pinyinSet.contains(input);
    }

    QList<LangNote> PinyinAnalysis::split(const QString &input) const {
        QList<LangNote> result;

        int pos = 0;
        while (pos < input.length()) {
            const auto &currentChar = input[pos];
            LangNote note;
            if (isLetter(currentChar)) {
                const int start = pos;
                while (pos < input.length() && isLetter(input[pos])) {
                    pos++;
                }

                note.lyric = input.mid(start, pos - start);
                if (contains(note.lyric)) {
                    note.language = id();
                    note.category = category();
                } else {
                    note.language = QStringLiteral("English");
                    note.category = QStringLiteral("English");
                }
            } else {
                const int start = pos;
                while (pos < input.length() && !isLetter(input[pos])) {
                    pos++;
                }
                note.lyric = input.mid(start, pos - start);
                note.language = QStringLiteral("Unknown");
                note.category = QStringLiteral("Unknown");
            }
            if (!note.lyric.isEmpty())
                result.append(note);
        }
        return result;
    }

    template <typename T>
    static T getRandomElementFromSet(const QSet<T> &set) {
        const int size = set.size();

        int randomIndex = QRandomGenerator::global()->bounded(size);

        auto it = set.constBegin();
        std::advance(it, randomIndex);

        return *it;
    }

    QString PinyinAnalysis::randString() const {
        return getRandomElementFromSet(pinyinSet);
    }

} // LangMgr