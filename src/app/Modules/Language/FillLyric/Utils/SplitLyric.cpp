#include "SplitLyric.h"
#include "Modules/Language/LangMgr/ILanguageManager.h"

#include <QDebug>

namespace FillLyric {
    QList<Phonic> CleanLyric::splitAuto(const QString &input) {
        QList<Phonic> phonics;
        const auto langMgr = LangMgr::ILanguageManager::instance();
        const auto res = langMgr->split(input);

        for (const auto &note : res) {
            Phonic phonic;
            phonic.lyric = note.lyric;
            phonic.language = note.language;
            phonic.category = note.category;
            if (note.language == "Linebreak")
                phonic.lineFeed = true;
            phonics.append(phonic);
        }

        return phonics;
    }

    QList<Phonic> CleanLyric::splitByChar(const QString &input) {
        const auto langMgr = LangMgr::ILanguageManager::instance();
        const auto linebreakFactory = langMgr->language("Linebreak");

        QList<Phonic> phonics;
        for (int i = 0; i < input.length(); i++) {
            const QChar &currentChar = input[i];
            if (currentChar == ' ') {
                continue;
            }
            if (linebreakFactory->contains(currentChar)) {
                if (!phonics.isEmpty()) {
                    phonics.last().lineFeed = true;
                }
                continue;
            }
            Phonic phonic;
            phonic.lyric = currentChar;
            phonic.language = langMgr->analysis(currentChar);
            phonic.category = langMgr->language(phonic.language)->category();
            phonics.append(phonic);
        }
        return phonics;
    }

    QList<Phonic> CleanLyric::splitCustom(const QString &input, const QStringList &splitter) {
        const auto langMgr = LangMgr::ILanguageManager::instance();
        const auto linebreakFactory = langMgr->language("Linebreak");

        QList<Phonic> phonics;
        int pos = 0;
        while (pos < input.length()) {
            const int start = pos;
            while (pos < input.length() && !splitter.contains(input[pos]) && input[pos] != ' ' &&
                   !linebreakFactory->contains(input[pos])) {
                pos++;
            }

            const auto lyric = input.mid(start, pos - start);
            if (!lyric.isEmpty() && !splitter.contains(lyric) && lyric != ' ') {
                Phonic phonic;
                phonic.lyric = lyric;
                phonic.language = langMgr->analysis(lyric);
                phonic.category = langMgr->language(phonic.language)->category();
                phonics.append(phonic);
            }

            if (linebreakFactory->contains(input[pos])) {
                if (!phonics.isEmpty()) {
                    phonics.last().lineFeed = true;
                }
            }
            pos++;
        }
        return phonics;
    }
}