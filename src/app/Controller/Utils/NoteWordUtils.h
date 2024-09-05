//
// Created by fluty on 24-7-21.
//

#ifndef NOTEWORDUTILS_H
#define NOTEWORDUTILS_H

#include "Model/AppModel/Note.h"

#include <QList>

class NoteWordUtils {
public:
    static QList<Note::WordProperties> getOriginalWordProperties(const QList<Note *> &notes);
    static void fillEditedPhonemeNames(const QList<Note *> &notes);
};

#endif // NOTEWORDUTILS_H
