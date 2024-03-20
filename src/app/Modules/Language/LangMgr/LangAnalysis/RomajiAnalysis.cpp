#include "RomajiAnalysis.h"

#include "Modules/Language/LangMgr/ILanguageManager.h"

namespace LangMgr {
    void RomajiAnalysis::loadDict() {
        QStringList initial = {"k",  "g",  "s",  "z",  "t",  "d",  "n",  "h",  "b",
                               "p",  "m",  "y",  "r",  "w",  "ky", "gy", "sh", "j",
                               "ch", "ny", "hy", "by", "py", "my", "ry"};
        QStringList final = {"a", "i", "u", "e", "o"};
        QStringList special = {"n", "nn", "m"};

        for (const auto &i : initial) {
            for (const auto &f : final) {
                m_trie->insert(i + f);
            }
        }

        for (const auto &s : special) {
            m_trie->insert(s);
        }

        for (const auto &i : initial) {
            m_trie->insert(i);
        }

        for (const auto &f : final) {
            m_trie->insert(f);
        }
    }

    QList<LangNote> RomajiAnalysis::split(const QString &input) const {
        auto enRes = ILanguageManager::instance()->language("English")->split(input);
        for (auto &note : enRes) {
            if (note.language == "English") {
                if (contains(note.lyric))
                    note.language = category();
            }
        }
        return enRes;
    }


} // LangMgr