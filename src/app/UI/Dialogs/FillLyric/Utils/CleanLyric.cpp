#include "CleanLyric.h"
#include "Modules/Language/LangMgr/ILanguageManager.h"

namespace FillLyric {
    QList<Phonic> CleanLyric::splitAuto(const QString &input) {
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

    QList<Phonic> CleanLyric::splitByChar(const QString &input, const bool &excludeSpace) {
        const auto langMgr = LangMgr::ILanguageManager::instance();
        const auto linebreakFactory = langMgr->language("Linebreak");

        QList<Phonic> phonics;
        for (int i = 0; i < input.length(); i++) {
            const QChar &currentChar = input[i];
            if (excludeSpace && currentChar == ' ') {
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
            phonics.append(phonic);
        }
        return phonics;
    }

    QList<Phonic> CleanLyric::splitCustom(const QString &input, const QStringList &splitter,
                                          const bool &excludeSpace) {
        const auto langMgr = LangMgr::ILanguageManager::instance();
        const auto linebreakFactory = langMgr->language("Linebreak");

        QList<Phonic> phonics;
        int pos = 0;
        while (pos < input.length()) {
            const int start = pos;
            while (pos < input.length() && !splitter.contains(input[pos]) &&
                   !(excludeSpace && input[pos] == ' ') &&
                   !linebreakFactory->contains(input[pos])) {
                pos++;
            }

            const auto lyric = input.mid(start, pos - start);
            if (!lyric.isEmpty() && !splitter.contains(lyric) && !(excludeSpace && lyric == ' ')) {
                Phonic phonic;
                phonic.lyric = lyric;
                phonic.language = langMgr->analysis(lyric);
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