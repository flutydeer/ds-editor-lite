//
// Created by FlutyDeer on 2026/6/7.
//

#ifndef NOTEINFERENCESNAPSHOT_H
#define NOTEINFERENCESNAPSHOT_H

#include <QString>

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

#endif // NOTEINFERENCESNAPSHOT_H
