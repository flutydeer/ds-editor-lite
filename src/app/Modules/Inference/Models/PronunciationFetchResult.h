#ifndef PRONUNCIATIONFETCHRESULT_H
#define PRONUNCIATIONFETCHRESULT_H

#include <lite/SynthrtEngine/PronunciationStage.h>

#include <QString>
#include <QStringList>

class PronunciationFetchResult {
public:
    QString pronunciation;
    QStringList candidates;
    /// Diagnostic source of the pronunciation.
    lite::synthrt::PronunciationStage stage = lite::synthrt::PronunciationStage::Unspecified;
};

#endif // PRONUNCIATIONFETCHRESULT_H
