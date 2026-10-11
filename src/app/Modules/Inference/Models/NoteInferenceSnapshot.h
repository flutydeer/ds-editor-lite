#ifndef NOTEINFERENCESNAPSHOT_H
#define NOTEINFERENCESNAPSHOT_H

#include <QString>
#include <QList>

class SingingClip;

class NoteInferenceSnapshot {
public:
    int noteId = -1;
    QString lyric;
    QString language;
    QString pronunciation;
    int globalStart = 0;
    int length = 0;
    int keyIndex = 0;
};

QList<NoteInferenceSnapshot> buildNoteInferenceSnapshots(const SingingClip &clip);

#endif // NOTEINFERENCESNAPSHOT_H
