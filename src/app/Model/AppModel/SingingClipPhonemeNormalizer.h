#ifndef SINGINGCLIPPHONEMENORMALIZER_H
#define SINGINGCLIPPHONEMENORMALIZER_H

#include <QList>
#include <QHash>

class AppModel;
class Note;
class SingingClip;

class SingingClipPhonemeNormalizer {
public:
    class ResetRecord {
    public:
        Note *note = nullptr;
        QList<int> editedOffsets;
    };

    static QList<Note *> collectInvalidEditedOffsetNotes(SingingClip &clip);
    static QList<ResetRecord> normalizeEditedOffsets(SingingClip &clip);
    static QList<ResetRecord> normalizeEditedOffsets(SingingClip &clip,
                                                      const QHash<Note *, int> &startDeltas);
    static QList<Note *> notesFromResetRecords(const QList<ResetRecord> &records);
    static void restoreEditedOffsets(const QList<ResetRecord> &records);
    static void normalizeEditedOffsets(AppModel &model);
};

#endif // SINGINGCLIPPHONEMENORMALIZER_H
