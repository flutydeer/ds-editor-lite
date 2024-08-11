#include "JyutpingAnalysis.h"

#include <QRandomGenerator>

namespace LangMgr {
    bool JyutpingAnalysis::initialize(QString &errMsg) {
        loadDict();
        return true;
    }

    void JyutpingAnalysis::loadDict() {
        QStringList initials = {"b", "p", "m", "f", "d", "t", "n", "l", "g", "k", "h", "ng", "z",
                                "c", "s", "j", "gw", "kw", "w"};
        QStringList finals = {"aa", "aai", "aau", "aam", "aan", "aang", "aap", "aat", "aak", "a",
                              "ai", "au", "am", "an", "ang", "ap", "at", "ak", "e", "ei", "eu",
                              "em", "eng", "ep", "ek", "eoi", "eon", "eot", "oe", "oeng", "oet",
                              "oek", "o", "oi", "ou", "on", "ong", "ot", "ok", "i", "iu", "im",
                              "in", "ing", "ip", "it", "ik", "u", "ui", "un", "ung", "ut", "uk",
                              "yu", "yun", "yut"};

        for (const auto &initial : initials) {
            for (const auto &final : finals) {
                jyutpingSet.insert(initial + final);
            }
        }

        for (const auto &initial : initials) {
            jyutpingSet.insert(initial);
        }

        for (const auto &final : finals) {
            jyutpingSet.insert(final);
        }
    }

    static bool isLetter(QChar c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '\'';
    }

    bool JyutpingAnalysis::contains(const QString &input) const {
        return jyutpingSet.contains(input);
    }

    QList<LangNote> JyutpingAnalysis::split(const QString &input) const {
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

    QString JyutpingAnalysis::randString() const {
        return getRandomElementFromSet(jyutpingSet);
    }

} // LangMgr