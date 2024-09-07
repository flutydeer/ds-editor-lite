//
// Created by fluty on 24-9-6.
//

#ifndef ORIGINALPARAMUTILS_H
#define ORIGINALPARAMUTILS_H

#include "Model/AppModel/Note.h"

class OriginalParamUtils {
public:
    static void updateNotePronPhoneme(const QList<Note *> &notes,
                                      const QList<Note::WordProperties> &args, SingingClip *clip);
};

#endif // ORIGINALPARAMUTILS_H
