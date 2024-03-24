#include "PinyinAnalysis.h"

#include "Modules/Language/LangMgr/ILanguageManager.h"

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
                m_trie->insert(initial + final);
            }
        }

        for (const auto &initial : initials) {
            m_trie->insert(initial);
        }

        for (const auto &final : finals) {
            m_trie->insert(final);
        }
    }

    QList<LangNote> PinyinAnalysis::split(const QString &input) const {
        auto enRes = ILanguageManager::instance()->language("English")->split(input);
        for (auto &note : enRes) {
            if (note.language == "English") {
                if (contains(note.lyric))
                    note.language = id();
                note.category = category();
            }
        }
        return enRes;
    }

} // LangMgr