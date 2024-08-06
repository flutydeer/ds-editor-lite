//
// Created by fluty on 24-7-21.
//

#ifndef NOTEWORDUTILS_H
#define NOTEWORDUTILS_H

#include <QList>

class Note;
class NoteWordUtils {
public:
    static void updateOriginalWordProperties(const QList<Note *> &notes);
    static void fillEditedPhonemeNames(const QList<Note *> &notes);
};

#endif // NOTEWORDUTILS_H
