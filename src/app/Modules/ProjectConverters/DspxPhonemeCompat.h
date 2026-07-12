#ifndef DSPXPHONEMECOMPAT_H
#define DSPXPHONEMECOMPAT_H

#include "Model/AppModel/Phonemes.h"

#include <opendspx/phonemes.h>

#include <QJsonObject>

namespace DspxPhonemeCompat {

void encode(const Phonemes &phonemes, opendspx::Phonemes &dspxPhonemes,
            QJsonObject &workspacePhoneme);

Phonemes decode(const opendspx::Phonemes &dspxPhonemes,
                const QJsonObject *workspacePhoneme = nullptr);

}

#endif // DSPXPHONEMECOMPAT_H
