//
// Created by fluty on 24-7-21.
//

#ifndef NOTEWORDUTILS_H
#define NOTEWORDUTILS_H

#include "Model/AppModel/Note.h"
#include "Model/Inference/PhonemeNameModel.h"

#include <QList>

class NoteWordUtils {
public:
    static QList<QString> getPronunciations(const QList<Note *> &notes);
    static QList<PhonemeNameResult> getPhonemeNames(const QList<QString> &input);
    // static void fillEditedPhonemeNames(const QList<Note *> &notes);
};

#endif // NOTEWORDUTILS_H
