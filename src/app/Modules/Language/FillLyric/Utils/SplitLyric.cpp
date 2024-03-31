#include "SplitLyric.h"
#include <LangMgr/ILanguageManager.h>

#include <QDebug>

namespace FillLyric {
    QList<QList<LangNote>> CleanLyric::splitAuto(const QString &input) {
        QList<QList<LangNote>> result;
        QList<LangNote> notes;
        const auto langMgr = LangMgr::ILanguageManager::instance();
        const auto res = langMgr->split(input);

        for (const auto &note : res) {
            if (note.language == "Linebreak") {
                if (!notes.isEmpty())
                    result.append(notes);
                notes.clear();
                continue;
            }
            notes.append(note);
        }

        if (!notes.isEmpty())
            result.append(notes);
        return result;
    }

    QList<QList<LangNote>> CleanLyric::splitByChar(const QString &input) {
        const auto langMgr = LangMgr::ILanguageManager::instance();
        const auto linebreakFactory = langMgr->language("Linebreak");

        QList<QList<LangNote>> result;
        QList<LangNote> notes;
        for (int i = 0; i < input.length(); i++) {
            const QChar &currentChar = input[i];
            if (currentChar == ' ') {
                continue;
            }
            if (linebreakFactory->contains(currentChar)) {
                if (!notes.isEmpty())
                    result.append(notes);
                notes.clear();
                continue;
            }
            LangNote note;
            note.lyric = currentChar;
            note.language = langMgr->analysis(currentChar);
            note.category = langMgr->language(note.language)->category();
            notes.append(note);
        }

        if (!notes.isEmpty())
            result.append(notes);
        return result;
    }

    QList<QList<LangNote>> CleanLyric::splitCustom(const QString &input,
                                                   const QStringList &splitter) {
        const auto langMgr = LangMgr::ILanguageManager::instance();
        const auto linebreakFactory = langMgr->language("Linebreak");

        QList<QList<LangNote>> result;
        QList<LangNote> notes;
        int pos = 0;
        while (pos < input.length()) {
            const int start = pos;
            while (pos < input.length() && !splitter.contains(input[pos]) && input[pos] != ' ' &&
                   !linebreakFactory->contains(input[pos])) {
                pos++;
            }

            const auto lyric = input.mid(start, pos - start);
            if (!lyric.isEmpty() && !splitter.contains(lyric) && lyric != ' ') {
                LangNote note;
                note.lyric = lyric;
                note.language = langMgr->analysis(lyric);
                note.category = langMgr->language(note.language)->category();
                notes.append(note);
            }

            if (linebreakFactory->contains(input[pos])) {
                if (!notes.isEmpty())
                    result.append(notes);
                notes.clear();
                continue;
            }
            pos++;
        }

        if (!notes.isEmpty())
            result.append(notes);
        return result;
    }
}