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

    class GroupMemberState {
    public:
        Note *note = nullptr;
        QString lyric;
        int relativeStartMs = 0;

        bool operator==(const GroupMemberState &other) const = default;
    };

    class GroupState {
    public:
        int rootSyllabificationCount = 0;
        QList<bool> rootOnsets;
        QList<GroupMemberState> members;

        bool operator==(const GroupState &other) const = default;
    };

    using GroupStates = QHash<Note *, GroupState>;

    static GroupStates captureGroupStates(const SingingClip &clip);
    static GroupStates captureGroupStates(const SingingClip &clip, const Timeline &timeline);
    static QList<Note *> collectInvalidEditedOffsetNotes(SingingClip &clip);
    static QList<ResetRecord> normalizeEditedOffsets(SingingClip &clip);
    static QList<ResetRecord> normalizeEditedOffsets(SingingClip &clip,
                                                      const GroupStates &previousGroupStates);
    static QList<ResetRecord> normalizeEditedOffsets(SingingClip &clip,
                                                      const GroupStates &previousGroupStates,
                                                      const Timeline &timeline);
    static QList<Note *> notesFromResetRecords(const QList<ResetRecord> &records);
    static void restoreEditedOffsets(const QList<ResetRecord> &records);
    static void normalizeEditedOffsets(AppModel &model);
};

#endif // SINGINGCLIPPHONEMENORMALIZER_H
