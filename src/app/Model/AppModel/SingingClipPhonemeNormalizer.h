#ifndef SINGINGCLIPPHONEMENORMALIZER_H
#define SINGINGCLIPPHONEMENORMALIZER_H

#include <QList>
#include <QHash>
#include <QString>

class AppModel;
class Note;
class SingingClip;
class Timeline;

class SingingClipPhonemeNormalizer {
public:
    class ResetRecord {
    public:
        Note *note = nullptr;
        QList<int> editedOffsets;
    };

    class WordMemberState {
    public:
        Note *note = nullptr;
        QString lyric;
        int relativeStartMs = 0;

        bool operator==(const WordMemberState &other) const = default;
    };

    class WordState {
    public:
        int rootSyllabificationCount = 0;
        QList<bool> rootOnsets;
        QList<WordMemberState> members;

        bool operator==(const WordState &other) const = default;
    };

    using WordStates = QHash<Note *, WordState>;

    static WordStates captureWordStates(const SingingClip &clip);
    static WordStates captureWordStates(const SingingClip &clip, const Timeline &timeline);
    static QList<Note *> collectInvalidEditedOffsetNotes(SingingClip &clip);
    static QList<ResetRecord> normalizeEditedOffsets(SingingClip &clip);
    static QList<ResetRecord> normalizeEditedOffsets(SingingClip &clip,
                                                      const WordStates &previousWordStates);
    static QList<ResetRecord> normalizeEditedOffsets(SingingClip &clip,
                                                      const WordStates &previousWordStates,
                                                      const Timeline &timeline);
    static QList<Note *> notesFromResetRecords(const QList<ResetRecord> &records);
    static void restoreEditedOffsets(const QList<ResetRecord> &records);
    static void normalizeEditedOffsets(AppModel &model);
};

#endif // SINGINGCLIPPHONEMENORMALIZER_H
