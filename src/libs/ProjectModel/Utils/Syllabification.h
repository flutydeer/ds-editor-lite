#ifndef PROJECTMODEL_SYLLABIFICATION_H
#define PROJECTMODEL_SYLLABIFICATION_H

#include <lite/ProjectModel/AppModel/Phonemes.h>

#include <QStringList>

namespace Syllabification {
    struct PhonemeRange {
        int start = 0;
        int count = 0;
    };

    bool isSyllabificationLyric(const QString &lyric);
    QList<PhonemeRange> phonemeRangesForNotes(const QStringList &lyrics,
                                            const QList<PhonemeName> &phonemes);
}

#endif // PROJECTMODEL_SYLLABIFICATION_H
