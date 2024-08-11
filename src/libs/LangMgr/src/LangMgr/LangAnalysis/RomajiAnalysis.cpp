#include "RomajiAnalysis.h"

#include <QRandomGenerator>

namespace LangMgr {
    bool RomajiAnalysis::initialize(QString &errMsg) {
        loadDict();
        return true;
    }

    void RomajiAnalysis::loadDict() {
        QStringList initial = {"k", "g", "s", "z", "t", "d", "n", "h", "b",
                               "p", "m", "y", "r", "w", "ky", "gy", "sh", "j",
                               "ch", "ny", "hy", "by", "py", "my", "ry"};
        QStringList final = {"a", "i", "u", "e", "o"};
        QStringList special = {"n", "nn", "m"};

        for (const auto &i : initial) {
            for (const auto &f : final) {
                romajiSet.insert(i + f);
            }
        }

        for (const auto &s : special) {
            romajiSet.insert(s);
        }

        for (const auto &i : initial) {
            romajiSet.insert(i);
        }

        for (const auto &f : final) {
            romajiSet.insert(f);
        }
    }

    static bool isLetter(QChar c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '\'';
    }

    bool RomajiAnalysis::contains(const QString &input) const {
        return romajiSet.contains(input);
    }

    QList<LangNote> RomajiAnalysis::split(const QString &input) const {
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
                    note.language = QStringLiteral("en");
                    note.category = QStringLiteral("en");
                }
            } else {
                const int start = pos;
                while (pos < input.length() && !isLetter(input[pos])) {
                    pos++;
                }
                note.lyric = input.mid(start, pos - start);
                note.language = QStringLiteral("unknown");
                note.category = QStringLiteral("unknown");
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

    QString RomajiAnalysis::randString() const {
        return getRandomElementFromSet(romajiSet);
    }

} // LangMgr