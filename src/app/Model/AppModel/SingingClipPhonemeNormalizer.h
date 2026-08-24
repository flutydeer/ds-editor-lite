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

    // Computes the cascade reset closure: the selected word roots plus every
    // edited right-neighbor root whose first phoneme overlaps the restored
    // (original) last phoneme of the word before it. Each node participates at
    // most once, so the closure always converges. Ordered by time.
    static QList<Note *> collectCascadeResetRoots(SingingClip &clip,
                                                  const QList<Note *> &selectedRoots,
                                                  const Timeline &timeline);
    static QList<Note *> collectCascadeResetRoots(SingingClip &clip,
                                                  const QList<Note *> &selectedRoots);

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
