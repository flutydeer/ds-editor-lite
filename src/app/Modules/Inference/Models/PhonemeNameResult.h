#ifndef PHONEMENAMERESULT_H
#define PHONEMENAMERESULT_H

#include <lite/ProjectModel/AppModel/Phonemes.h>
#include <QStringList>

class PhonemeNameResult {
public:
    bool success = false;
    QList<PhonemeName> phonemeNames;
};

#endif //PHONEMENAMERESULT_H
