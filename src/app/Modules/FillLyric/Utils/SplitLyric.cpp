#include "Modules/FillLyric/Utils/SplitLyric.h"

#include "Modules/FillLyric/Utils/TextSplitter.h"
#include "Modules/FillLyric/Utils/TextTagger.h"

#include "Modules/FillLyric/LangCommon.h"

namespace FillLyric
{
    static void flushLine(QList<QList<LangNote>> &result, QList<LangNote> &notes) {
        if (!notes.isEmpty()) {
            result.append(notes);
            notes.clear();
        }
    }

    QList<QList<LangNote>> LyricSplitter::splitAuto(const QString &input,
                                                     const std::vector<std::string> &priorityG2pIds) {
        QList<QList<LangNote>> result;
        QList<LangNote> notes;

        const auto splitRes = TextSplitter::split(input.toStdString());
        const auto tagRes = TextTagger::tag(splitRes, true, priorityG2pIds);

        for (const auto &tagger_res : tagRes) {
            if (tagger_res.tag == "linebreak") {
                flushLine(result, notes);
                continue;
            }
            auto tempNote = LangNote(tagger_res.lyric.c_str());
            tempNote.language = tagger_res.language.c_str();
            tempNote.g2pId = QStringLiteral("unknown");
            notes.append(tempNote);
        }

        flushLine(result, notes);
        return result;
    }

    static bool containLinebreak(const QChar &c) {
        return c == QChar::CarriageReturn || c == QChar::LineFeed || c == QChar::LineSeparator ||
               c == QChar::ParagraphSeparator;
    }

    QList<QList<LangNote>> LyricSplitter::splitByChar(const QString &input) {
        struct CharInfo {
            int lineIdx;
            QChar ch;
        };

        QList<QList<LangNote>> result;
        std::vector<CharInfo> chars;
        std::vector<std::string> taggerInput;
        int currentLine = 0;

        struct LineBreakPos {
            int charIndex;
        };
        std::vector<int> lineBreaks;

        for (int i = 0; i < input.length(); i++) {
            const QChar &c = input[i];
            if (c == ' ')
                continue;
            if (containLinebreak(c)) {
                lineBreaks.push_back(static_cast<int>(chars.size()));
                continue;
            }
            chars.push_back({currentLine, c});
            taggerInput.push_back(QString(c).toStdString());
        }

        if (taggerInput.empty())
            return result;

        const auto taggerRes = TextTagger::tag(taggerInput, false, {});

        QList<LangNote> notes;
        size_t lbIdx = 0;
        for (size_t i = 0; i < chars.size(); i++) {
            while (lbIdx < lineBreaks.size() && lineBreaks[lbIdx] == static_cast<int>(i)) {
                flushLine(result, notes);
                lbIdx++;
            }

            LangNote note;
            note.lyric = chars[i].ch;
            if (i < taggerRes.size()) {
                note.g2pId = QStringLiteral("unknown");
                note.language = taggerRes[i].language.c_str();
            }
            notes.append(note);
        }
        while (lbIdx < lineBreaks.size()) {
            flushLine(result, notes);
            lbIdx++;
        }

        flushLine(result, notes);
        return result;
    }

    QList<QList<LangNote>> LyricSplitter::splitCustom(const QString &input, const QStringList &splitter) {
        struct WordInfo {
            QString lyric;
            bool isLineBreak;
        };
        std::vector<WordInfo> words;
        std::vector<std::string> taggerInput;
        std::vector<size_t> taggerToWordIndex;

        int pos = 0;
        while (pos < input.length()) {
            if (containLinebreak(input[pos])) {
                words.push_back({QString(), true});
                pos++;
                continue;
            }

            if (input[pos] == ' ' || splitter.contains(input[pos])) {
                pos++;
                continue;
            }

            const int start = pos;
            while (pos < input.length() && !splitter.contains(input[pos]) && input[pos] != ' ' &&
                   !containLinebreak(input[pos])) {
                pos++;
            }

            const auto lyric = input.mid(start, pos - start);
            if (!lyric.isEmpty()) {
                taggerToWordIndex.push_back(words.size());
                words.push_back({lyric, false});
                taggerInput.push_back(lyric.toStdString());
            }
        }

        if (taggerInput.empty()) {
            QList<QList<LangNote>> result;
            return result;
        }

        const auto taggerRes = TextTagger::tag(taggerInput, false, {});

        QList<QList<LangNote>> result;
        QList<LangNote> notes;
        size_t taggerIdx = 0;

        for (size_t i = 0; i < words.size(); i++) {
            if (words[i].isLineBreak) {
                flushLine(result, notes);
                continue;
            }

            LangNote note;
            note.lyric = words[i].lyric;
            if (taggerIdx < taggerRes.size()) {
                note.g2pId = QStringLiteral("unknown");
                note.language = taggerRes[taggerIdx].language.c_str();
                taggerIdx++;
            }
            notes.append(note);
        }

        flushLine(result, notes);
        return result;
    }
}
